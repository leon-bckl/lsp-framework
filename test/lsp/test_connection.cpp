#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <test/test.h>
#include <lsp/connection.h>
#include <lsp/io/stream.h>
#include <lsp/json/json.h>
#include <lsp/jsonrpc/jsonrpc.h>

using namespace lsp;

namespace{

class MemoryStream : public io::Stream{
public:
	explicit MemoryStream(std::string input = {})
		: m_input{std::move(input)}
	{
	}

	void read(char* buffer, std::size_t size) override
	{
		if(size == 0)
			return;

		if(m_readPos >= m_input.size())
		{
			if(size == 1)
			{
				*buffer = Eof;
				return;
			}

			throw io::Error{"Unexpected end of stream"};
		}

		const auto available = std::min(size, m_input.size() - m_readPos);
		std::memcpy(buffer, m_input.data() + m_readPos, available);
		m_readPos += available;

		if(available < size)
			throw io::Error{"Unexpected end of stream"};
	}

	void write(const char* buffer, std::size_t size) override
	{
		if(failWrites)
			throw io::Error{"Write failed"};

		m_output.append(buffer, size);
	}

	[[nodiscard]] const std::string& output() const{ return m_output; }

	bool failWrites = false;

private:
	std::string m_input;
	std::size_t m_readPos = 0;
	std::string m_output;
};

std::string makeMessage(std::string_view body, std::string_view contentType = {})
{
	std::string msg = "Content-Length: " + std::to_string(body.size()) + "\r\n";

	if(!contentType.empty())
		msg += "Content-Type: " + std::string(contentType) + "\r\n";

	msg += "\r\n";
	msg += body;
	return msg;
}

std::string expectedMessageText(std::string_view body)
{
	return "Content-Length: " + std::to_string(body.size()) +
		"\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8\r\n\r\n" + std::string(body);
}

} // namespace

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("ReadMessage/Request", [](){
		auto stream     = MemoryStream(makeMessage(R"({"jsonrpc":"2.0","id":1,"method":"foo","params":{"x":1}})"));
		auto connection = Connection(stream);

		auto  message = connection.readMessage();
		auto& request = std::get<jsonrpc::Request>(std::get<jsonrpc::Message>(message));

		test::check(!request.isNotification(), "hasId");
		test::compare(*request.id, jsonrpc::MessageId(json::Integer(1)));
		test::compare(request.method, std::string("foo"));
		test::check(request.params.has_value(), "hasParams");
		test::compare(request.params->object().get("x").integer(), 1);
	});

	app.addTest("ReadMessage/Notification", [](){
		auto stream     = MemoryStream(makeMessage(R"({"jsonrpc":"2.0","method":"foo"})"));
		auto connection = Connection(stream);

		auto  message = connection.readMessage();
		auto& request = std::get<jsonrpc::Request>(std::get<jsonrpc::Message>(message));

		test::check(request.isNotification(), "isNotification");
		test::compare(request.method, std::string("foo"));
	});

	app.addTest("ReadMessage/Response", [](){
		auto stream     = MemoryStream(makeMessage(R"({"jsonrpc":"2.0","id":2,"result":42})"));
		auto connection = Connection(stream);

		auto  message  = connection.readMessage();
		auto& response = std::get<jsonrpc::Response>(std::get<jsonrpc::Message>(message));

		test::compare(response.id, jsonrpc::MessageId(json::Integer(2)));
		test::check(response.result.has_value(), "hasResult");
		test::compare(response.result->integer(), 42);
		test::check(!response.error.has_value(), "!hasError");
	});

	app.addTest("ReadMessage/Batch", [](){
		auto stream = MemoryStream(makeMessage(
			R"([{"jsonrpc":"2.0","id":1,"method":"a"},{"jsonrpc":"2.0","id":2,"method":"b"}])"));
		auto connection = Connection(stream);

		auto  message = connection.readMessage();
		auto& batch   = std::get<jsonrpc::MessageBatch>(message);

		test::compare(batch.size(), 2u);
		test::compare(std::get<jsonrpc::Request>(batch[0]).method, std::string("a"));
		test::compare(std::get<jsonrpc::Request>(batch[1]).method, std::string("b"));
	});

	app.addTest("ReadMessage/HeaderFieldNameCaseInsensitive", [](){
		const std::string_view body = R"({"jsonrpc":"2.0","method":"foo"})";
		auto stream = MemoryStream("content-length: " + std::to_string(body.size()) + "\r\n\r\n" + std::string(body));
		auto connection = Connection(stream);

		auto  message = connection.readMessage();
		auto& request = std::get<jsonrpc::Request>(std::get<jsonrpc::Message>(message));

		test::compare(request.method, std::string("foo"));
	});

	app.addTest("ReadMessage/HeaderFieldWhitespaceTrimmed", [](){
		const std::string_view body = R"({"jsonrpc":"2.0","method":"foo"})";
		auto stream = MemoryStream("Content-Length :  " + std::to_string(body.size()) + "  \r\n\r\n" + std::string(body));
		auto connection = Connection(stream);

		auto  message = connection.readMessage();
		auto& request = std::get<jsonrpc::Request>(std::get<jsonrpc::Message>(message));

		test::compare(request.method, std::string("foo"));
	});

	app.addTest("ReadMessage/MalformedJsonWritesParseErrorResponse", [](){
		auto stream     = MemoryStream(makeMessage("not valid json"));
		auto connection = Connection(stream);

		test::expectException<json::ParseError>([&](){ (void)connection.readMessage(); });

		// Connection must write a jsonrpc error response for the client before rethrowing
		auto errorStream     = MemoryStream(stream.output());
		auto errorConnection = Connection(errorStream);
		auto  errorMessage   = errorConnection.readMessage();
		auto& response       = std::get<jsonrpc::Response>(std::get<jsonrpc::Message>(errorMessage));

		test::check(response.error.has_value(), "hasError");
		test::compare(response.error->code, jsonrpc::Error::ParseError);
	});

	app.addTest("ReadMessage/NonObjectOrArrayWritesInvalidRequestResponse", [](){
		auto stream     = MemoryStream(makeMessage("42"));
		auto connection = Connection(stream);

		test::expectException<jsonrpc::ProtocolError>([&](){ (void)connection.readMessage(); });

		auto errorStream     = MemoryStream(stream.output());
		auto errorConnection = Connection(errorStream);
		auto  errorMessage   = errorConnection.readMessage();
		auto& response       = std::get<jsonrpc::Response>(std::get<jsonrpc::Message>(errorMessage));

		test::check(response.error.has_value(), "hasError");
		test::compare(response.error->code, jsonrpc::Error::InvalidRequest);
	});

	app.addTest("WriteMessage/Request", [](){
		auto stream     = MemoryStream();
		auto connection = Connection(stream);

		connection.writeMessage(Connection::Message(jsonrpc::Message(jsonrpc::createRequest(json::Integer(1), "foo"))));

		test::compare(stream.output(), expectedMessageText(R"({"jsonrpc":"2.0","id":1,"method":"foo"})"));
	});

	app.addTest("WriteMessage/Response", [](){
		auto stream     = MemoryStream();
		auto connection = Connection(stream);

		connection.writeMessage(Connection::Message(jsonrpc::Message(jsonrpc::createResponse(json::Integer(1), 42))));

		test::compare(stream.output(), expectedMessageText(R"({"jsonrpc":"2.0","id":1,"result":42})"));
	});

	auto expectReadError = [](std::string rawInput, std::string_view expectedMessage)
	{
		auto stream     = MemoryStream(std::move(rawInput));
		auto connection = Connection(stream);

		test::expectException<ConnectionError>([&](){ (void)connection.readMessage(); }, expectedMessage);
	};

	app.addTest("ReadMessage/Errors", expectReadError)({
		{"ConnectionLostOnEmptyStream",
		 {"", "Connection lost"}},
		{"EofMidHeader",
		 {"Content-Length: 5\r\n", "Connection lost"}},
		{"BareNewlineInHeaderField",
		 {"Content-Length: 5\n", "Protocol: Unexpected '\\n' in header field, expected '\\r\\n'"}},
		{"HeaderFieldMissingLineFeed",
		 {"Content-Length: 5\r\rX", "Protocol: Expected header field to be terminated by '\\r\\n'"}},
		{"HeaderBlockMissingLineFeed",
		 {"Content-Length: 5\r\n\rX", "Protocol: Expected header to be terminated by '\\r\\n'"}},
		{"InvalidContentLengthValue",
		 {"Content-Length: abc\r\n\r\n{}", "Protocol: Invalid value for Content-Length header field"}},
		{"TruncatedBody",
		 {"Content-Length: 100\r\n\r\n{}", "Unexpected end of stream"}},
		{"InvalidContentType",
		 {makeMessage(R"({"jsonrpc":"2.0","method":"foo"})", "text/plain"),
		  "Protocol: Unsupported or invalid content type: text/plain"}},
		{"UnsupportedCharset",
		 {makeMessage(R"({"jsonrpc":"2.0","method":"foo"})", "application/vscode-jsonrpc; charset=ascii"),
		  "Protocol: Unsupported or invalid character encoding: ascii"}},
	});

	app.addTest("ReadMessage/InvalidFieldTypeWritesNoResponse", [](){
		auto stream     = MemoryStream(makeMessage(R"({"jsonrpc":"2.0","method":123})"));
		auto connection = Connection(stream);

		test::expectException<ConnectionError>([&](){ (void)connection.readMessage(); }, "JSON value is not string");
		test::check(stream.output().empty(), "noResponseWritten");
	});

	app.addTest("WriteMessage/Batch", [](){
		auto stream     = MemoryStream();
		auto connection = Connection(stream);

		auto batch = jsonrpc::MessageBatch();
		batch.push_back(jsonrpc::Message(jsonrpc::createRequest(json::Integer(1), "a")));
		batch.push_back(jsonrpc::Message(jsonrpc::createRequest(json::Integer(2), "b")));

		connection.writeMessage(Connection::Message(std::move(batch)));

		test::compare(stream.output(),
			expectedMessageText(R"([{"jsonrpc":"2.0","id":1,"method":"a"},{"jsonrpc":"2.0","id":2,"method":"b"}])"));
	});

	app.addTest("WriteMessage/StreamFailureIsWrappedAsConnectionError", [](){
		auto stream     = MemoryStream();
		auto connection = Connection(stream);
		stream.failWrites = true;

		test::expectException<ConnectionError>([&](){
			connection.writeMessage(Connection::Message(jsonrpc::Message(jsonrpc::createRequest(json::Integer(1), "foo"))));
		}, "Write failed");
	});

	app.addTest("Connection/MoveConstruct", [](){
		auto stream   = MemoryStream();
		auto original = Connection(stream);
		auto moved    = Connection(std::move(original));

		moved.writeMessage(Connection::Message(jsonrpc::Message(jsonrpc::createRequest(json::Integer(1), "foo"))));

		test::compare(stream.output(), expectedMessageText(R"({"jsonrpc":"2.0","id":1,"method":"foo"})"));
	});

	app.addTest("Connection/MoveAssign", [](){
		auto streamA     = MemoryStream();
		auto streamB     = MemoryStream();
		auto connectionA = Connection(streamA);
		auto connectionB = Connection(streamB);

		connectionB = std::move(connectionA);
		connectionB.writeMessage(Connection::Message(jsonrpc::Message(jsonrpc::createRequest(json::Integer(1), "foo"))));

		test::compare(streamA.output(), expectedMessageText(R"({"jsonrpc":"2.0","id":1,"method":"foo"})"));
		test::check(streamB.output().empty(), "streamBUntouched");
	});

	app.addTest("ReadWriteRoundTrip", [](){
		auto writeStream     = MemoryStream();
		auto writeConnection = Connection(writeStream);

		writeConnection.writeMessage(Connection::Message(jsonrpc::Message(
			jsonrpc::createRequest(json::String("abc"), "textDocument/foo", json::Value(json::Object({{"x", 1}}))))));

		auto readStream     = MemoryStream(writeStream.output());
		auto readConnection = Connection(readStream);

		auto  message = readConnection.readMessage();
		auto& request = std::get<jsonrpc::Request>(std::get<jsonrpc::Message>(message));

		test::compare(*request.id, jsonrpc::MessageId(json::String("abc")));
		test::compare(request.method, std::string("textDocument/foo"));
		test::compare(request.params->object().get("x").integer(), 1);
	});

	return app.main(argc, argv);
}

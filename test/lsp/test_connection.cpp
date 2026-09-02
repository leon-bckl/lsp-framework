#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <test/test.h>
#include <lsp/connection.h>
#include <lsp/io/stream.h>
#include <lsp/json/json.h>
#include <lsp/jsonrpc/jsonrpc.h>

using namespace lsp;

namespace test{

template<>
std::string toString<json::Value>(const json::Value& v)
{
	return std::visit([](const auto& actualValue){
		return test::toString(actualValue);
	}, v.variant());
}

} // namespace test

namespace{

class MemoryStream : public io::Stream{
public:
	explicit MemoryStream(std::string input = {})
		: m_input(std::move(input))
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

std::string sendMessage(auto&& fn)
{
	auto stream     = MemoryStream();
	auto connection = Connection(stream);

	fn(connection);

	return stream.output();
}

// The Send/* tests' body may be pretty-printed or compact depending on whether
// LSP_MESSAGE_DEBUG_LOG is on, so compare parsed content instead of exact text.
json::Value parseMessageBody(std::string_view rawOutput)
{
	const auto headerEnd = rawOutput.find("\r\n\r\n");
	test::check(headerEnd != std::string_view::npos, "hasHeaderBodySeparator");
	return json::parse(rawOutput.substr(headerEnd + 4));
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

	app.addTest("ReadMessage/RecoversFromMalformedMessage", [](std::string badBody, int expectedErrorCode)
	{
		auto stream = MemoryStream(
			makeMessage(badBody) +
			makeMessage(R"({"jsonrpc":"2.0","method":"foo"})"));
		auto connection = Connection(stream);

		auto  message = connection.readMessage();
		auto& request = std::get<jsonrpc::Request>(std::get<jsonrpc::Message>(message));
		test::compare(request.method, std::string("foo"));

		const auto response = parseMessageBody(stream.output());
		test::check(response.object().get("id").isNull(), "nullErrorId");
		test::compare(response.object().get("error").object().get("code").integer(), expectedErrorCode);
	})({
		{"MalformedJson",    {"not valid json", jsonrpc::Error::ParseError}},
		{"NonObjectOrArray", {"42",             jsonrpc::Error::InvalidRequest}},
	});

	app.addTest("ReadMessage/Errors", [](std::string rawInput, std::string_view expectedMessage)
	{
		auto stream     = MemoryStream(std::move(rawInput));
		auto connection = Connection(stream);

		test::expectException<ConnectionError>([&](){ (void)connection.readMessage(); }, expectedMessage);
	})({
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
		{"MalformedJsonThenEof",
		 {makeMessage("not valid json"), "Connection lost"}},
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

	app.addTest("Send/StreamFailureIsWrappedAsConnectionError", [](){
		auto stream     = MemoryStream();
		auto connection = Connection(stream);
		stream.failWrites = true;

		test::expectException<ConnectionError>([&](){
			auto sender = connection.request("foo", jsonrpc::MessageId(json::Integer(1)));
			sender.submit();
		}, "Write failed");
	});

	app.addTest("Connection/MoveConstruct", [](){
		auto stream   = MemoryStream();
		auto original = Connection(stream);
		auto moved    = Connection(std::move(original));

		auto sender = moved.request("foo", jsonrpc::MessageId(json::Integer(1)));
		sender.submit();

		test::compare(parseMessageBody(stream.output()), json::parse(R"({"jsonrpc":"2.0","id":1,"method":"foo"})"));
	});

	app.addTest("Connection/MoveAssign", [](){
		auto streamA     = MemoryStream();
		auto streamB     = MemoryStream();
		auto connectionA = Connection(streamA);
		auto connectionB = Connection(streamB);

		connectionB = std::move(connectionA);

		auto sender = connectionB.request("foo", jsonrpc::MessageId(json::Integer(1)));
		sender.submit();

		test::compare(parseMessageBody(streamA.output()), json::parse(R"({"jsonrpc":"2.0","id":1,"method":"foo"})"));
		test::check(streamB.output().empty(), "streamBUntouched");
	});

	app.addTest("Send/Request", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.request("foo", jsonrpc::MessageId(json::Integer(1)));
			sender.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"({"jsonrpc":"2.0","id":1,"method":"foo"})"));
	});

	app.addTest("Send/RequestWithParamsObject", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.request("foo", jsonrpc::MessageId(json::Integer(1)));
			sender.writeParams(std::unordered_map<std::string, int>{{"x", 1}});
			sender.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"({"jsonrpc":"2.0","id":1,"method":"foo","params":{"x":1}})"));
	});

	app.addTest("Send/RequestWithParamsArray", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.request("foo", jsonrpc::MessageId(json::Integer(1)));
			sender.writeParams(std::vector<int>{1, 2});
			sender.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"({"jsonrpc":"2.0","id":1,"method":"foo","params":[1,2]})"));
	});

	app.addTest("Send/Notification", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.notification("foo");
			sender.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"({"jsonrpc":"2.0","method":"foo"})"));
	});

	app.addTest("Send/NotificationWithParams", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.notification("foo");
			sender.writeParams(std::unordered_map<std::string, int>{{"x", 1}});
			sender.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"({"jsonrpc":"2.0","method":"foo","params":{"x":1}})"));
	});

	app.addTest("Send/ResponseNoDataDefaultsToNull", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.response(jsonrpc::MessageId(json::Integer(1)));
			sender.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"({"jsonrpc":"2.0","id":1,"result":null})"));
	});

	app.addTest("Send/ResponseWithScalarData", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.response(jsonrpc::MessageId(json::Integer(1)));
			sender.writeData(42);
			sender.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"({"jsonrpc":"2.0","id":1,"result":42})"));
	});

	app.addTest("Send/ResponseWithDataObject", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.response(jsonrpc::MessageId(json::Integer(1)));
			sender.writeData(std::unordered_map<std::string, int>{{"a", 1}});
			sender.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"({"jsonrpc":"2.0","id":1,"result":{"a":1}})"));
	});

	app.addTest("Send/ErrorResponse", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.errorResponse(jsonrpc::MessageId(json::Integer(1)), jsonrpc::Error::InvalidParams, "Invalid params");
			sender.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"({"jsonrpc":"2.0","id":1,"error":{"code":-32602,"message":"Invalid params"}})"));
	});

	app.addTest("Send/ErrorResponseWithDataObject", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.errorResponse(jsonrpc::MessageId(json::Integer(1)), jsonrpc::Error::InvalidParams, "Invalid params");
			sender.writeData(std::unordered_map<std::string, std::string>{{"field", "x"}});
			sender.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"({"jsonrpc":"2.0","id":1,"error":{"code":-32602,"message":"Invalid params","data":{"field":"x"}}})"));
	});

	app.addTest("Send/Batch", [](){
		const auto out = sendMessage([](Connection& connection){
			auto batch = connection.messageBatch();

			{
				auto rw = batch.writeNotification("foo");
			}
			{
				auto rw = batch.writeResponse(jsonrpc::MessageId(json::Integer(1)));
				rw.writeData([](std::string_view key, const int& value, json::ObjectWriter& writer)
				{
					writeJson(key, value, writer);
				}, 1);
			}

			batch.submit();
		});

		test::compare(parseMessageBody(out), json::parse(R"([{"jsonrpc":"2.0","method":"foo"},{"jsonrpc":"2.0","id":1,"result":1}])"));
	});

	app.addTest("Send/RoundTrip", [](){
		const auto out = sendMessage([](Connection& connection){
			auto sender = connection.request("textDocument/foo", jsonrpc::MessageId(json::String("abc")));
			sender.writeParams(std::unordered_map<std::string, int>{{"x", 1}});
			sender.submit();
		});

		auto readStream     = MemoryStream(out);
		auto readConnection = Connection(readStream);

		auto  message = readConnection.readMessage();
		auto& request = std::get<jsonrpc::Request>(std::get<jsonrpc::Message>(message));

		test::compare(*request.id, jsonrpc::MessageId(json::String("abc")));
		test::compare(request.method, std::string("textDocument/foo"));
		test::compare(request.params->object().get("x").integer(), 1);
	});

	return app.main(argc, argv);
}

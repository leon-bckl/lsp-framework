#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <test/test.h>
#include <lsp/io/stream.h>
#include <lsp/json/json.h>
#include <lsp/messagehandler.h>

using namespace lsp;

struct TestRequest{
	static constexpr auto Method = std::string_view("test/request");
	static constexpr auto Type   = Message::Request;

	using Params = std::unordered_map<std::string, int>;
	using Result = int;
};

struct TestNoParamsRequest{
	static constexpr auto Method = std::string_view("test/noParamsRequest");
	static constexpr auto Type   = Message::Request;

	using Result = std::vector<int>;
};

struct TestNotification{
	static constexpr auto Method = std::string_view("test/notification");
	static constexpr auto Type   = Message::Notification;

	using Params = std::vector<int>;
};

struct TestNoParamsNotification{
	static constexpr auto Method = std::string_view("test/noParamsNotification");
	static constexpr auto Type   = Message::Notification;
};

class LoopbackStream : public io::Stream{
public:
	void read(char* buffer, std::size_t size) override
	{
		if(size == 0)
			return;

		auto lock = std::unique_lock(m_mutex);

		if(!m_dataAvailable.wait_for(lock, std::chrono::seconds(2), [&](){ return m_buffer.size() - m_readOffset >= size; }))
			throw io::Error("Timed out waiting for data");

		std::memcpy(buffer, m_buffer.data() + m_readOffset, size);
		m_readOffset += size;
	}

	void write(const char* buffer, std::size_t size) override
	{
		{
			auto lock = std::unique_lock(m_mutex);

			if(m_readOffset > 0)
			{
				m_buffer.erase(0, m_readOffset);
				m_readOffset = 0;
			}

			m_buffer.append(buffer, size);
		}

		m_dataAvailable.notify_all();
	}

	[[nodiscard]] bool empty() const
	{
		auto lock = std::unique_lock(m_mutex);
		return m_readOffset >= m_buffer.size();
	}

	[[nodiscard]] std::string takeAll()
	{
		auto lock = std::unique_lock(m_mutex);
		auto result = m_buffer.substr(m_readOffset);
		m_buffer.clear();
		m_readOffset = 0;
		return result;
	}

private:
	mutable std::mutex      m_mutex;
	std::condition_variable m_dataAvailable;
	std::string             m_buffer;
	std::size_t             m_readOffset = 0;
};

std::string makeMessage(std::string_view body)
{
	return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + std::string(body);
}

json::Value parseMessageBody(std::string_view rawMessage)
{
	const auto headerEnd = rawMessage.find("\r\n\r\n");
	test::check(headerEnd != std::string_view::npos, "hasHeaderBodySeparator");
	return json::parse(rawMessage.substr(headerEnd + 4));
}

template<typename M, typename T = typename M::Result>
T getResult(RequestResult<M>& result)
{
	if(!result.wait(2000))
		test::fail("Timed out waiting for future");

	return result.get();
}

template<typename M, typename T = typename M::Result>
void expectResponseError(RequestResult<M>& result, int expectedCode, std::string_view expectedMessage)
{
	try
	{
		getResult(result);
		test::fail("Expected ResponseError to be thrown");
	}
	catch(const ResponseError& e)
	{
		test::compare(e.code(), expectedCode);
		test::compare(e.message(), expectedMessage);
	}
}

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("Request/Future", [](){
		auto stream   = LoopbackStream();
		auto handler  = MessageHandler(Connection(stream));
		auto called   = false;
		auto received = std::unordered_map<std::string, int>();

		handler.on<TestRequest>([&](std::unordered_map<std::string, int> params)
		{
			called   = true;
			received = params;
			return 42;
		});

		auto response = handler.sendRequest<TestRequest>({{"x", 1}});
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received, std::unordered_map<std::string, int>{{"x", 1}});
		test::compare(getResult(response), 42);
	});

	app.addTest("Request/Callback", [](){
		auto stream     = LoopbackStream();
		auto handler    = MessageHandler(Connection(stream));
		auto called     = false;
		auto received   = std::unordered_map<std::string, int>();
		auto thenResult = std::optional<int>();

		handler.on<TestRequest>([&](std::unordered_map<std::string, int> params)
		{
			called   = true;
			received = params;
			return 42;
		});

		handler.sendRequest<TestRequest>({{"x", 1}},
			[&](int result){ thenResult = result; },
			[](const ResponseError&){ test::fail("Expected no error"); });

		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received, std::unordered_map<std::string, int>{{"x", 1}});
		test::check(thenResult.has_value(), "hasResult");
		test::compare(*thenResult, 42);
	});

	app.addTest("NoParamsRequest/Future", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.on<TestNoParamsRequest>([&]()
		{
			called = true;
			return std::vector<int>{1, 2, 3};
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(getResult(response), std::vector<int>{1, 2, 3});
	});

	app.addTest("NoParamsRequest/Callback", [](){
		auto stream     = LoopbackStream();
		auto handler    = MessageHandler(Connection(stream));
		auto called     = false;
		auto thenResult = std::optional<std::vector<int>>();

		handler.on<TestNoParamsRequest>([&]()
		{
			called = true;
			return std::vector<int>{1, 2, 3};
		});

		handler.sendRequest<TestNoParamsRequest>(
			[&](std::vector<int> result){ thenResult = std::move(result); },
			[](const ResponseError&){ test::fail("Expected no error"); });

		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::check(thenResult.has_value(), "hasResult");
		test::compare(*thenResult, std::vector<int>{1, 2, 3});
	});

	app.addTest("Notification", [](){
		auto stream   = LoopbackStream();
		auto handler  = MessageHandler(Connection(stream));
		auto called   = false;
		auto received = std::vector<int>();

		handler.on<TestNotification>([&](std::vector<int> params)
		{
			called   = true;
			received = params;
		});

		handler.sendNotification<TestNotification>({1, 2, 3});
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received, std::vector<int>{1, 2, 3});
	});

	app.addTest("NoParamsNotification", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.on<TestNoParamsNotification>([&]()
		{
			called = true;
		});

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		test::check(called, "called");
	});

	app.addTest("CurrentRequestId", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto ids     = std::vector<MessageId>();

		handler.on<TestNoParamsRequest>([&]()
		{
			ids.push_back(MessageHandler::currentRequestId());
			return std::vector<int>{};
		});

		auto response1 = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		auto response2 = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(ids.size(), 2);
		test::compare(ids[0], response1.requestId());
		test::compare(ids[1], response2.requestId());
		test::check(!(ids[0] == ids[1]), "idsAreUnique");

		test::expectException<std::logic_error>([](){
			(void)MessageHandler::currentRequestId();
		});
	});

	app.addTest("Error/Future", [](bool useRequestError, int expectedCode, std::string_view expectedMessage){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> std::vector<int>
		{
			if(useRequestError)
				throw RequestError(1234, "custom error");

			throw std::runtime_error("boom");
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, expectedCode, expectedMessage);
	})({
		{"RequestError",     {true,  1234,                        "custom error"}},
		{"GenericException", {false, MessageError::InternalError, "boom"}},
	});

	app.addTest("Error/Callback", [](bool useRequestError, int expectedCode, std::string_view expectedMessage){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> std::vector<int>
		{
			if(useRequestError)
				throw RequestError(1234, "custom error");

			throw std::runtime_error("boom");
		});

		auto thenCalled  = false;
		auto errorResult = std::optional<ResponseError>();

		handler.sendRequest<TestNoParamsRequest>(
			[&](const std::vector<int>&){ thenCalled = true; },
			[&](const ResponseError& e){ errorResult = e; });

		handler.processNextMessage();
		handler.processNextMessage();

		test::check(!thenCalled, "!thenCalled");
		test::check(errorResult.has_value(), "hasError");
		test::compare(errorResult->code(), expectedCode);
		test::compare(errorResult->message(), expectedMessage);
	})({
		{"RequestError",     {true,  1234,                        "custom error"}},
		{"GenericException", {false, MessageError::InternalError, "boom"}},
	});

	app.addTest("Error/MethodNotFound", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, MessageError::MethodNotFound, "Method not found");
	});

	app.addTest("Notification/CallbackThrows", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsNotification>([&](){
			throw std::runtime_error("boom");
		});

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		test::check(stream.empty(), "noResponseWritten");
	});

	app.addTest("Notification/MethodNotFound", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		test::check(stream.empty(), "noResponseWritten");
	});

	app.addTest("Remove", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.on<TestNoParamsRequest>([&]()
		{
			called = true;
			return std::vector<int>{};
		});

		handler.remove(std::string(TestNoParamsRequest::Method));

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(!called, "!called");
		expectResponseError(response, MessageError::MethodNotFound, "Method not found");
	});

	app.addTest("Async/Success", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.on<TestNoParamsRequest>([&]() -> RequestFuture<TestNoParamsRequest>
		{
			called = true;
			auto promise = std::promise<std::vector<int>>();
			promise.set_value(std::vector<int>{1, 2, 3});
			return promise.get_future();
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(getResult(response), std::vector<int>{1, 2, 3});
	});

	app.addTest("Async/Error", [](bool useRequestError, int expectedCode, std::string_view expectedMessage){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> RequestFuture<TestNoParamsRequest>
		{
			auto promise = std::promise<std::vector<int>>();

			if(useRequestError)
				promise.set_exception(std::make_exception_ptr(RequestError(1234, "custom error")));
			else
				promise.set_exception(std::make_exception_ptr(std::runtime_error("boom")));

			return promise.get_future();
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, expectedCode, expectedMessage);
	})({
		{"RequestError",     {true,  1234,                        "custom error"}},
		{"GenericException", {false, MessageError::InternalError, "boom"}},
	});

	app.addTest("Generic/Request", [](){
		auto stream   = LoopbackStream();
		auto handler  = MessageHandler(Connection(stream));
		auto called   = false;
		auto received = json::Value();

		handler.onCustom<GenericRequest>("generic/request", [&](json::Value&& params) -> json::Value
		{
			called   = true;
			received = params;
			return json::Value(json::Integer(42));
		});

		auto response = handler.sendCustomRequest<GenericRequest>("generic/request", json::Value(json::Object({{"x", 1}})));
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received.object().get("x").integer(), 1);
		test::compare(getResult(response).integer(), 42);
	});

	app.addTest("Generic/Async", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.onCustom<GenericRequest>("generic/asyncRequest", [&](json::Value&&) -> RequestFuture<GenericRequest>
		{
			called = true;
			auto promise = std::promise<json::Value>();
			promise.set_value(json::Value(json::Integer(42)));
			return promise.get_future();
		});

		auto response = handler.sendCustomRequest<GenericRequest>("generic/asyncRequest");
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(getResult(response).integer(), 42);
	});

	app.addTest("Generic/RequestCallback", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.onCustom<GenericRequest>("generic/requestCallback", [&](json::Value&& params) -> json::Value
		{
			called = true;
			return json::Value(json::Integer(params.object().get("x").integer() * 2));
		});

		auto thenResult = std::optional<json::Value>();

		handler.sendCustomRequest<GenericRequest>("generic/requestCallback", json::Value(json::Object({{"x", 21}})),
			[&](json::Value&& result){ thenResult = std::move(result); },
			[](const ResponseError&){ test::fail("Expected no error"); });

		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::check(thenResult.has_value(), "hasResult");
		test::compare(thenResult->integer(), 42);
	});

	app.addTest("Generic/Notification", [](){
		auto stream   = LoopbackStream();
		auto handler  = MessageHandler(Connection(stream));
		auto called   = false;
		auto received = json::Value();

		handler.onCustom<GenericNotification>("generic/notification", [&](json::Value&& params)
		{
			called   = true;
			received = params;
		});

		handler.sendNotification("generic/notification", json::Value(json::Object({{"x", 1}})));
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received.object().get("x").integer(), 1);
	});

	app.addTest("Error/Data", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> std::vector<int>
		{
			throw RequestError(1234, "custom error", json::Value(json::Object({{"detail", "x"}})));
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		try
		{
			getResult(response);
			test::fail("Expected ResponseError to be thrown");
		}
		catch(const ResponseError& e)
		{
			test::compare(e.code(), 1234);
			test::check(e.data().has_value(), "hasData");
			test::compare(e.data()->object().get("detail").string(), "x");
		}
	});

	app.addTest("Error/InvalidParams", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestRequest>([&](std::unordered_map<std::string, int>)
		{
			return 42;
		});

		const auto message = makeMessage(R"({"jsonrpc":"2.0","id":1,"method":"test/request","params":{"x":"bad"}})");
		stream.write(message.data(), message.size());

		handler.processNextMessage();

		const auto response = parseMessageBody(stream.takeAll());
		test::compare(response.object().get("error").object().get("code").integer(), MessageError::InvalidParams);
	});

	app.addTest("MultipleInFlightRequests", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestRequest>([&](std::unordered_map<std::string, int> params)
		{
			return params.at("x");
		});

		auto responseA = handler.sendRequest<TestRequest>({{"x", 1}});
		auto responseB = handler.sendRequest<TestRequest>({{"x", 2}});

		handler.processNextMessage();
		handler.processNextMessage();
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(responseA), 1);
		test::compare(getResult(responseB), 2);
	});

	app.addTest("Batch", [](){
		auto stream              = LoopbackStream();
		auto handler             = MessageHandler(Connection(stream));
		auto requestCalled       = false;
		auto paramsRequestCalled = false;
		auto notificationCalled  = false;

		handler.on<TestNoParamsRequest>([&]()
		{
			requestCalled = true;
			return std::vector<int>{1, 2, 3};
		});

		handler.on<TestRequest>([&](std::unordered_map<std::string, int> params)
		{
			paramsRequestCalled = true;
			return params.at("x") * 2;
		});

		handler.on<TestNoParamsNotification>([&]()
		{
			notificationCalled = true;
		});

		const auto batchBody = std::string(
			R"([{"jsonrpc":"2.0","id":1,"method":"test/noParamsRequest"},)"
			R"({"jsonrpc":"2.0","method":"test/noParamsNotification"},)"
			R"({"jsonrpc":"2.0","id":2,"method":"test/request","params":{"x":5}}])");
		const auto message = makeMessage(batchBody);
		stream.write(message.data(), message.size());

		handler.processNextMessage();

		test::check(requestCalled, "requestCalled");
		test::check(paramsRequestCalled, "paramsRequestCalled");
		test::check(notificationCalled, "notificationCalled");

		const auto response = parseMessageBody(stream.takeAll());
		test::check(response.isArray(), "isArray");
		test::compare(response.array().size(), 2);
		test::compare(response.array()[0].object().get("id").integer(), 1);
		test::compare(response.array()[0].object().get("result").array().size(), 3);
		test::compare(response.array()[1].object().get("id").integer(), 2);
		test::compare(response.array()[1].object().get("result").integer(), 10);
	});

	return app.main(argc, argv);
}

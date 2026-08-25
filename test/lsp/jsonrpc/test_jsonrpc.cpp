#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <test/test.h>
#include <lsp/json/json.h>
#include <lsp/jsonrpc/jsonrpc.h>

using namespace lsp;
using namespace lsp::jsonrpc;

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

json::Object parseObject(std::string_view text)
{
	return json::parse(text).object();
}

json::Array parseArray(std::string_view text)
{
	return json::parse(text).array();
}

} // namespace

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	/*
	 * Request
	 */

	app.addTest("FromJson/Request/Basic", [](){
		auto  message = messageFromJson(parseObject(R"({"jsonrpc":"2.0","id":1,"method":"foo"})"));
		auto& request = std::get<Request>(message);

		test::check(!request.isNotification(), "hasId");
		test::compare(request.method, std::string("foo"));
		test::check(!request.params.has_value(), "!hasParams");
	});

	app.addTest("FromJson/Request/Notification", [](){
		auto  message = messageFromJson(parseObject(R"({"jsonrpc":"2.0","method":"foo"})"));
		auto& request = std::get<Request>(message);

		test::check(request.isNotification(), "isNotification");
		test::compare(request.method, std::string("foo"));
	});

	app.addTest("FromJson/Request/Id", [](std::string_view json, MessageId expectedId){
		auto  message = messageFromJson(parseObject(json));
		auto& request = std::get<Request>(message);

		test::check(!request.isNotification(), "hasId");
		test::compare(*request.id, expectedId);
	})({
		{"Integer", {R"({"jsonrpc":"2.0","id":1,"method":"foo"})",     MessageId(json::Integer(1))}},
		{"String",  {R"({"jsonrpc":"2.0","id":"abc","method":"foo"})", MessageId(json::String("abc"))}},
		{"Null",    {R"({"jsonrpc":"2.0","id":null,"method":"foo"})",  MessageId(json::Null())}},
	});

	app.addTest("FromJson/Request/Params", [](std::string_view json, std::optional<json::Value> expectedParams){
		auto  message = messageFromJson(parseObject(json));
		auto& request = std::get<Request>(message);

		test::compare(request.params, expectedParams);
	})({
		{"Object",      {R"({"jsonrpc":"2.0","id":1,"method":"foo","params":{"x":1}})", json::Value(json::Object({{"x", 1}}))}},
		{"Array",       {R"({"jsonrpc":"2.0","id":1,"method":"foo","params":[1,2]})",   json::Value(json::Array({1, 2}))}},
		{"NullAllowed", {R"({"jsonrpc":"2.0","id":1,"method":"foo","params":null})",    std::nullopt}},
	});

	auto expectProtocolError = [](std::string json, std::string_view expectedMessage)
	{
		test::expectException<ProtocolError>([&](){
			(void)messageFromJson(parseObject(json));
		}, expectedMessage);
	};

	app.addTest("FromJson/Request/MethodWrongTypeThrows", [](){
		test::expectException<json::TypeError>([](){
			(void)messageFromJson(parseObject(R"({"jsonrpc":"2.0","method":123})"));
		});
	});

	/*
	 * Response
	 */

	app.addTest("FromJson/Response/Result", [](std::string_view json, MessageId expectedId, int expectedResult){
		auto  message  = messageFromJson(parseObject(json));
		auto& response = std::get<Response>(message);

		test::compare(response.id, expectedId);
		test::check(response.result.has_value(), "hasResult");
		test::compare(response.result->integer(), expectedResult);
		test::check(!response.error.has_value(), "!hasError");
	})({
		{"Basic",              {R"({"jsonrpc":"2.0","id":1,"result":42})", MessageId(json::Integer(1)), 42}},
		{"MissingIdDefaultsToNull", {R"({"jsonrpc":"2.0","result":1})",    MessageId(json::Null()),     1}},
	});

	app.addTest("FromJson/Response/Error", [](std::string_view json, int expectedCode, std::string_view expectedMessage, std::optional<json::Value> expectedData){
		auto  message  = messageFromJson(parseObject(json));
		auto& response = std::get<Response>(message);

		test::check(!response.result.has_value(), "!hasResult");
		test::check(response.error.has_value(), "hasError");
		test::compare(response.error->code, expectedCode);
		test::compare(response.error->message, std::string(expectedMessage));
		test::compare(response.error->data, expectedData);
	})({
		{"NoData",
		 {R"({"jsonrpc":"2.0","id":1,"error":{"code":-32600,"message":"Invalid Request"}})", -32600, "Invalid Request", std::nullopt}},
		{"WithData",
		 {R"({"jsonrpc":"2.0","id":1,"error":{"code":-32602,"message":"Invalid params","data":{"field":"x"}}})",
		  -32602, "Invalid params", json::Value(json::Object({{"field", "x"}}))}},
	});

	app.addTest("FromJson/ProtocolErrors", expectProtocolError)({
		{"MissingJsonrpc",        {R"({"id":1,"method":"foo"})", "jsonrpc property is missing"}},
		{"WrongJsonrpcType",      {R"({"jsonrpc":2,"id":1,"method":"foo"})", "jsonrpc property expected to be a string"}},
		{"WrongJsonrpcVersion",   {R"({"jsonrpc":"1.0","id":1,"method":"foo"})", "Invalid or unsupported jsonrpc version"}},
		{"InvalidIdType",         {R"({"jsonrpc":"2.0","id":true,"method":"foo"})", "Request id type must be string, number or null"}},
		{"InvalidParamsType",     {R"({"jsonrpc":"2.0","id":1,"method":"foo","params":"bad"})", "Params type must be object or array"}},
		{"MissingResultAndError", {R"({"jsonrpc":"2.0","id":1})", "Response must have either 'result' or 'error'"}},
		{"BothResultAndError",    {R"({"jsonrpc":"2.0","id":1,"result":1,"error":{"code":-32600,"message":"bad"}})",
		                           "Response must have either 'result' or 'error'"}},
		{"ErrorMissingCode",      {R"({"jsonrpc":"2.0","id":1,"error":{"message":"bad"}})", "Response error is missing the error code"}},
		{"ErrorCodeWrongType",    {R"({"jsonrpc":"2.0","id":1,"error":{"code":"bad","message":"bad"}})", "Response error code must be a number"}},
		{"ErrorMissingMessage",   {R"({"jsonrpc":"2.0","id":1,"error":{"code":-32600}})", "Response error is missing the error message"}},
		{"ErrorMessageWrongType", {R"({"jsonrpc":"2.0","id":1,"error":{"code":-32600,"message":123}})", "Response error message must be a string"}},
	});

	/*
	 * Batch
	 */

	app.addTest("FromJson/Batch/Requests", [](){
		auto batch = messageBatchFromJson(
			parseArray(R"([{"jsonrpc":"2.0","id":1,"method":"a"},{"jsonrpc":"2.0","id":2,"method":"b"}])"));

		test::compare(batch.size(), 2);
		test::compare(std::get<Request>(batch[0]).method, std::string("a"));
		test::compare(std::get<Request>(batch[1]).method, std::string("b"));
	});

	app.addTest("FromJson/Batch/Mixed", [](){
		auto batch = messageBatchFromJson(
			parseArray(R"([{"jsonrpc":"2.0","method":"a"},{"jsonrpc":"2.0","id":1,"result":42}])"));

		test::compare(batch.size(), 2);
		test::check(std::holds_alternative<Request>(batch[0]), "isRequest");
		test::check(std::holds_alternative<Response>(batch[1]), "isResponse");
	});

	app.addTest("FromJson/Batch/EmptyThrows", [](){
		test::expectException<ProtocolError>([](){
			(void)messageBatchFromJson(parseArray("[]"));
		}, "Message batch must not be empty");
	});

	app.addTest("FromJson/Batch/NonObjectElementThrows", [](){
		test::expectException<json::TypeError>([](){
			(void)messageBatchFromJson(parseArray("[1]"));
		});
	});

	return app.main(argc, argv);
}

#include <string>
#include <utility>
#include <test/test.h>
#include <lsp/json/json.h>
#include <lsp/json/writer.h>
#include <lsp/jsonrpc/messagewriter.h>

using namespace lsp;
using namespace lsp::jsonrpc;

namespace{

auto build(std::string_view indent, auto&& fn) -> std::string
{
	auto out    = std::string();
	auto writer = json::Writer(out, indent);
	fn(writer);
	return out;
}

} // namespace

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	/*
	 * RequestWriter
	 */

	app.addTest("Request/Basic", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = RequestWriter::writeRequest(writer.beginObject(), MessageId(json::Integer(1)), "foo");
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"method":"foo"})"));
	});

	app.addTest("Request/StringId", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = RequestWriter::writeRequest(writer.beginObject(), MessageId(json::String("abc")), "foo");
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":"abc","method":"foo"})"));
	});

	app.addTest("Request/ParamsObject", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw     = RequestWriter::writeRequest(writer.beginObject(), MessageId(json::Integer(1)), "foo");
			auto params = rw.writeParamsObject();
			params.write("a", 1);
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"method":"foo","params":{"a":1}})"));
	});

	app.addTest("Request/ParamsArray", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw     = RequestWriter::writeRequest(writer.beginObject(), MessageId(json::Integer(1)), "foo");
			auto params = rw.writeParamsArray();
			params.write(1);
			params.write(2);
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"method":"foo","params":[1,2]})"));
	});

	app.addTest("Notification/Basic", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = RequestWriter::writeNotification(writer.beginObject(), "foo");
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","method":"foo"})"));
	});

	app.addTest("Notification/ParamsObject", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw     = RequestWriter::writeNotification(writer.beginObject(), "foo");
			auto params = rw.writeParamsObject();
			params.write("a", true);
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","method":"foo","params":{"a":true}})"));
	});

	/*
	 * ResponseWriter
	 */

	app.addTest("Response/DataObject", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw     = ResponseWriter::writeResponse(writer.beginObject(), MessageId(json::Integer(1)));
			auto result = rw.writeDataObject();
			result.write("a", 1);
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"result":{"a":1}})"));
	});

	app.addTest("Response/DataArray", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw     = ResponseWriter::writeResponse(writer.beginObject(), MessageId(json::Integer(1)));
			auto result = rw.writeDataArray();
			result.write(1);
			result.write(2);
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"result":[1,2]})"));
	});

	app.addTest("Response/ScalarResult", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = ResponseWriter::writeResponse(writer.beginObject(), MessageId(json::Integer(1)));
			rw.writeData(42);
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"result":42})"));
	});

	app.addTest("Response/NullResult", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = ResponseWriter::writeResponse(writer.beginObject(), MessageId(json::Integer(1)));
			rw.writeData(nullptr);
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"result":null})"));
	});

	app.addTest("Response/ErrorBasic", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = ResponseWriter::writeError(writer.beginObject(), MessageId(json::Integer(1)), Error::InvalidRequest, "Invalid Request");
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"error":{"code":-32600,"message":"Invalid Request"}})"));
	});

	app.addTest("Response/ErrorDataObject", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw   = ResponseWriter::writeError(writer.beginObject(), MessageId(json::Integer(1)), Error::InvalidRequest, "Invalid Request");
			auto data = rw.writeDataObject();
			data.write("reason", "bad");
		});

		test::compare(out, std::string_view(
			R"({"jsonrpc":"2.0","id":1,"error":{"code":-32600,"message":"Invalid Request","data":{"reason":"bad"}}})"));
	});

	app.addTest("Response/ErrorDataArray", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw   = ResponseWriter::writeError(writer.beginObject(), MessageId(json::Integer(1)), Error::InvalidRequest, "Invalid Request");
			auto data = rw.writeDataArray();
			data.write(1);
			data.write(2);
		});

		test::compare(out, std::string_view(
			R"({"jsonrpc":"2.0","id":1,"error":{"code":-32600,"message":"Invalid Request","data":[1,2]}})"));
	});

	app.addTest("Response/ErrorScalarData", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = ResponseWriter::writeError(writer.beginObject(), MessageId(json::Integer(1)), Error::InvalidRequest, "Invalid Request");
			rw.writeData("bad");
		});

		test::compare(out, std::string_view(
			R"({"jsonrpc":"2.0","id":1,"error":{"code":-32600,"message":"Invalid Request","data":"bad"}})"));
	});

	app.addTest("Response/NoDataDefaultsToNullResult", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = ResponseWriter::writeResponse(writer.beginObject(), MessageId(json::Integer(1)));
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"result":null})"));
	});

	app.addTest("Response/MoveConstruct", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw1 = ResponseWriter::writeResponse(writer.beginObject(), MessageId(json::Integer(1)));
			auto rw2 = std::move(rw1);
			rw2.writeData(1);
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"result":1})"));
	});

	app.addTest("Response/MoveAssign", [](){
		auto outA    = std::string();
		auto outB    = std::string();
		auto writerA = json::Writer(outA);
		auto writerB = json::Writer(outB);

		{
			auto rw = ResponseWriter::writeResponse(writerA.beginObject(), MessageId(json::Integer(1)));
			rw      = ResponseWriter::writeResponse(writerB.beginObject(), MessageId(json::Integer(2)));
			rw.writeData(2);
		}

		test::compare(outA, std::string_view(R"({"jsonrpc":"2.0","id":1,"result":null})"));
		test::compare(outB, std::string_view(R"({"jsonrpc":"2.0","id":2,"result":2})"));
	});

	app.addTest("Request/ManualFinalize", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw     = RequestWriter::writeRequest(writer.beginObject(), MessageId(json::Integer(1)), "foo");
			auto params = rw.writeParamsObject();
			params.write("a", 1);
			params.finalize();
			rw.finalize();
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"method":"foo","params":{"a":1}})"));
	});

	app.addTest("Response/ManualFinalizeWritesExplicitResult", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = ResponseWriter::writeResponse(writer.beginObject(), MessageId(json::Integer(1)));
			rw.writeData(1);
			rw.finalize();
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"result":1})"));
	});

	app.addTest("Response/ManualFinalizeDefaultsToNullResult", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = ResponseWriter::writeResponse(writer.beginObject(), MessageId(json::Integer(1)));
			rw.finalize();
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"result":null})"));
	});

	app.addTest("Response/FinalizeIsIdempotent", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = ResponseWriter::writeResponse(writer.beginObject(), MessageId(json::Integer(1)));
			rw.finalize();
			rw.finalize();
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":1,"result":null})"));
	});

	app.addTest("Response/ErrorNullId", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto rw = ResponseWriter::writeError(writer.beginObject(), MessageId(json::Null{}), Error::ParseError, "Parse error");
		});

		test::compare(out, std::string_view(R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}})"));
	});

	/*
	 * BatchWriter
	 */

	app.addTest("Batch/RequestsAndNotifications", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto batch = BatchWriter(writer.beginArray());

			{
				auto rw     = batch.writeRequest(MessageId(json::Integer(1)), "foo");
				auto params = rw.writeParamsObject();
				params.write("a", 1);
			}
			{
				auto rw = batch.writeNotification("bar");
			}
		});

		test::compare(out, std::string_view(
			R"([{"jsonrpc":"2.0","id":1,"method":"foo","params":{"a":1}},{"jsonrpc":"2.0","method":"bar"}])"));
	});

	app.addTest("Batch/ManualFinalize", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto batch = BatchWriter(writer.beginArray());
			auto rw    = batch.writeNotification("foo");

			rw.finalize();
			batch.finalize();
		});

		test::compare(out, std::string_view(R"([{"jsonrpc":"2.0","method":"foo"}])"));
	});

	app.addTest("Batch/ResponsesAndErrors", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto batch = BatchWriter(writer.beginArray());

			{
				auto rw = batch.writeResponse(MessageId(json::Integer(1)));
				rw.writeData(1);
			}
			{
				auto rw = batch.writeError(MessageId(json::Integer(2)), Error::MethodNotFound, "Method not found");
			}
		});

		test::compare(out, std::string_view(
			R"([{"jsonrpc":"2.0","id":1,"result":1},{"jsonrpc":"2.0","id":2,"error":{"code":-32601,"message":"Method not found"}}])"));
	});

	app.addTest("Batch/StructuredPayloads", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto batch = BatchWriter(writer.beginArray());

			{
				auto rw     = batch.writeResponse(MessageId(json::Integer(1)));
				auto result = rw.writeDataObject();
				result.write("a", 1);
			}
			{
				auto rw     = batch.writeResponse(MessageId(json::Integer(2)));
				auto result = rw.writeDataArray();
				result.write(1);
				result.write(2);
			}
			{
				auto rw   = batch.writeError(MessageId(json::Integer(3)), Error::InvalidParams, "Invalid params");
				auto data = rw.writeDataObject();
				data.write("field", "x");
			}
			{
				auto rw   = batch.writeError(MessageId(json::Integer(4)), Error::InvalidParams, "Invalid params");
				auto data = rw.writeDataArray();
				data.write(1);
				data.write(2);
			}
		});

		test::compare(out, std::string_view(
			R"([{"jsonrpc":"2.0","id":1,"result":{"a":1}},)"
			R"({"jsonrpc":"2.0","id":2,"result":[1,2]},)"
			R"({"jsonrpc":"2.0","id":3,"error":{"code":-32602,"message":"Invalid params","data":{"field":"x"}}},)"
			R"({"jsonrpc":"2.0","id":4,"error":{"code":-32602,"message":"Invalid params","data":[1,2]}}])"));
	});

	app.addTest("Batch/Empty", [](){
		const auto out = build({}, [](json::Writer& writer){
			auto batch = BatchWriter(writer.beginArray());
		});

		test::compare(out, std::string_view("[]"));
	});

	/*
	 * Formatting
	 */

	app.addTest("Format/RequestWithParams", [](){
		const auto out = build("\t", [](json::Writer& writer){
			auto rw     = RequestWriter::writeRequest(writer.beginObject(), MessageId(json::Integer(1)), "foo");
			auto params = rw.writeParamsObject();
			params.write("a", 1);
		});

		test::compare(out, std::string_view(
			"{\n\t\"jsonrpc\": \"2.0\",\n\t\"id\": 1,\n\t\"method\": \"foo\",\n\t\"params\": {\n\t\t\"a\": 1\n\t}\n}"));
	});

	return app.main(argc, argv);
}

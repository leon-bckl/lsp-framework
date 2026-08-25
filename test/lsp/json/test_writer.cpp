#include <cstdint>
#include <limits>
#include <test/test.h>
#include <lsp/json/json.h>
#include <lsp/json/writer.h>

using namespace lsp;
using namespace lsp::json;

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	auto build = [](std::string_view indent, auto&& fn) -> std::string
	{
		auto out    = std::string();
		auto writer = Writer(out, indent);
		fn(writer);
		return out;
	};

	app.addTest("ArrayWriter/ObjectElements", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw = writer.beginArray();

			{
				auto ow = aw.beginObject();
				ow.write("a", 1);
			}
			{
				auto ow = aw.beginObject();
				ow.write("b", 2);
			}
		});

		test::compare(out, std::string_view(R"([{"a":1},{"b":2}])"));
	});

	app.addTest("ArrayWriter/ArrayElements", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw = writer.beginArray();

			{
				auto inner = aw.beginArray();
				inner.write(1);
			}
			{
				auto inner = aw.beginArray();
				inner.write(2);
			}
		});

		test::compare(out, std::string_view("[[1],[2]]"));
	});

	app.addTest("ArrayWriter/ObjectElementsFormatted", [&build](){
		const auto out = build("\t", [](Writer& writer){
			auto aw = writer.beginArray();

			{
				auto ow = aw.beginObject();
				ow.write("a", 1);
			}
			{
				auto ow = aw.beginObject();
				ow.write("b", 2);
			}
		});

		test::compare(out, std::string_view("[\n\t{\n\t\t\"a\": 1\n\t},\n\t{\n\t\t\"b\": 2\n\t}\n]"));
	});

	app.addTest("ObjectWriter/MoveConstruct", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow1 = writer.beginObject();
			ow1.write("a", 1);

			auto ow2 = std::move(ow1);
			ow2.write("b", 2);
		});

		test::compare(out, std::string_view(R"({"a":1,"b":2})"));
	});

	app.addTest("ArrayWriter/MoveConstruct", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw1 = writer.beginArray();
			aw1.write(1);

			auto aw2 = std::move(aw1);
			aw2.write(2);
		});

		test::compare(out, std::string_view("[1,2]"));
	});

	app.addTest("ObjectWriter/MoveAssign", [](){
		auto outA    = std::string();
		auto outB    = std::string();
		auto writerA = Writer(outA);
		auto writerB = Writer(outB);

		{
			auto ow = writerA.beginObject();
			ow.write("x", 1);
			ow = writerB.beginObject();
			ow.write("y", 2);
		}

		test::compare(outA, std::string_view(R"({"x":1})"));
		test::compare(outB, std::string_view(R"({"y":2})"));
	});

	app.addTest("ArrayWriter/MoveAssign", [](){
		auto outA    = std::string();
		auto outB    = std::string();
		auto writerA = Writer(outA);
		auto writerB = Writer(outB);

		{
			auto aw = writerA.beginArray();
			aw.write(1);
			aw = writerB.beginArray();
			aw.write(2);
		}

		test::compare(outA, std::string_view("[1]"));
		test::compare(outB, std::string_view("[2]"));
	});

	app.addTest("Object/ManualFinalize", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow = writer.beginObject();
			auto inner = ow.beginObject("nested");
			inner.write("a", 1);
			inner.finalize();
			ow.write("done", true);
		});

		test::compare(out, std::string_view(R"({"nested":{"a":1},"done":true})"));
	});

	app.addTest("Array/ManualFinalize", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow    = writer.beginObject();
			auto inner = ow.beginArray("items");
			inner.write(1);
			inner.write(2);
			inner.finalize();
			ow.write("done", true);
		});

		test::compare(out, std::string_view(R"({"items":[1,2],"done":true})"));
	});

	app.addTest("ObjectWriter/FinalizeIsIdempotent", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow = writer.beginObject();
			ow.write("a", 1);
			ow.finalize();
			ow.finalize();
		});

		test::compare(out, std::string_view(R"({"a":1})"));
	});

	app.addTest("ArrayWriter/FinalizeIsIdempotent", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw = writer.beginArray();
			aw.write(1);
			aw.finalize();
			aw.finalize();
		});

		test::compare(out, std::string_view("[1]"));
	});

	app.addTest("Array/Empty", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw = writer.beginArray();
		});

		test::compare(out, std::string_view("[]"));
	});

	app.addTest("Array/SingleElement", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw = writer.beginArray();
			aw.write(1);
		});

		test::compare(out, std::string_view("[1]"));
	});

	app.addTest("Array/MultiElement", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw = writer.beginArray();
			aw.write(1);
			aw.write(2);
			aw.write(3);
		});

		test::compare(out, std::string_view("[1,2,3]"));
	});

	app.addTest("Array/Nested", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw = writer.beginArray();

			{
				auto inner = aw.beginArray();
				inner.write(1);
				inner.write(2);
			}
			{
				auto inner = aw.beginArray();
				inner.write(3);
				inner.write(4);
			}
		});

		test::compare(out, std::string_view("[[1,2],[3,4]]"));
	});

	app.addTest("Array/Mixed", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw = writer.beginArray();
			aw.write(1);
			aw.write(std::string_view("two"));
			aw.write(true);
			aw.write(nullptr);
		});

		test::compare(out, std::string_view(R"([1,"two",true,null])"));
	});

	app.addTest("Object/Empty", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow = writer.beginObject();
		});

		test::compare(out, std::string_view("{}"));
	});

	app.addTest("Object/SingleKey", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow = writer.beginObject();
			ow.write("a", 1);
		});

		test::compare(out, std::string_view(R"({"a":1})"));
	});

	app.addTest("Object/MultiKey", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow = writer.beginObject();
			ow.write("a", 1);
			ow.write("b", 2);
			ow.write("c", 3);
		});

		test::compare(out, std::string_view(R"({"a":1,"b":2,"c":3})"));
	});

	app.addTest("Object/Nested", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow    = writer.beginObject();
			auto inner = ow.beginObject("outer");
			inner.write("inner", 42);
		});

		test::compare(out, std::string_view(R"({"outer":{"inner":42}})"));
	});

	app.addTest("Object/KeyEscaping", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow = writer.beginObject();
			ow.write("a\"b", 1);
		});

		test::compare(out, std::string_view(R"({"a\"b":1})"));
	});

	app.addTest("Object/StringValueTypes", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow = writer.beginObject();

			const char* cstr = "cstr";
			const auto  str  = std::string("str");

			ow.write("a", cstr);
			ow.write("b", str);
		});

		test::compare(out, std::string_view(R"({"a":"cstr","b":"str"})"));
	});

	app.addTest("Number/IntegralTypes", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw = writer.beginArray();
			aw.write(short{-7});
			aw.write(static_cast<unsigned int>(300));
			aw.write(std::numeric_limits<std::uint64_t>::max());
		});

		test::compare(out, std::string_view("[-7,300,18446744073709551615]"));
	});

	app.addTest("Object/EmbeddedValue", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow = writer.beginObject();
			ow.write("a", Value(42));
			ow.write("b", Object({{"x", 1}}));
			ow.write("c", Array({1, 2}));
		});

		test::compare(out, std::string_view(R"({"a":42,"b":{"x":1},"c":[1,2]})"));
	});

	app.addTest("Array/EmbeddedValue", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto aw = writer.beginArray();
			aw.write(Value(42));
			aw.write(Object({{"x", 1}}));
			aw.write(Array({1, 2}));
		});

		test::compare(out, std::string_view(R"([42,{"x":1},[1,2]])"));
	});

	auto expectNonFinite = [&build](double value, std::string_view expected){
		const auto out = build({}, [value](Writer& writer){
			auto aw = writer.beginArray();
			aw.write(value);
		});

		test::compare(out, expected);
	};

	app.addTest("Number/NanInf", expectNonFinite)({
		{"NaN",              {std::numeric_limits<double>::quiet_NaN(), "[null]"}},
		{"Infinity",         {std::numeric_limits<double>::infinity(),  "[null]"}},
		{"NegativeInfinity", {-std::numeric_limits<double>::infinity(), "[null]"}},
	});

	app.addTest("Composite/NestedArrayAndObject", [&build](){
		const auto out = build({}, [](Writer& writer){
			auto ow = writer.beginObject();

			{
				auto aw = ow.beginArray("a");
				aw.write(1);
				aw.write(2);

				auto inner = aw.beginObject();
				inner.write("b", true);
			}

			ow.write("c", nullptr);
		});

		test::compare(out, std::string_view(R"({"a":[1,2,{"b":true}],"c":null})"));
	});

	app.addTest("Format/EmptyObject", [&build](){
		const auto out = build("\t", [](Writer& writer){
			auto ow = writer.beginObject();
		});

		test::compare(out, std::string_view("{}"));
	});

	app.addTest("Format/EmptyArray", [&build](){
		const auto out = build("\t", [](Writer& writer){
			auto aw = writer.beginArray();
		});

		test::compare(out, std::string_view("[]"));
	});

	app.addTest("Format/SingleKeyObject", [&build](){
		const auto out = build("\t", [](Writer& writer){
			auto ow = writer.beginObject();
			ow.write("a", 1);
		});

		test::compare(out, std::string_view("{\n\t\"a\": 1\n}"));
	});

	app.addTest("Format/SingleElementArray", [&build](){
		const auto out = build("\t", [](Writer& writer){
			auto aw = writer.beginArray();
			aw.write(1);
		});

		test::compare(out, std::string_view("[\n\t1\n]"));
	});

	app.addTest("Format/NestedObject", [&build](){
		const auto out = build("\t", [](Writer& writer){
			auto ow = writer.beginObject();
			ow.write("a", 1);
			auto aw = ow.beginArray("b");
			aw.write(1);
			aw.write(2);
		});

		test::compare(out, std::string_view("{\n\t\"a\": 1,\n\t\"b\": [\n\t\t1,\n\t\t2\n\t]\n}"));
	});

	app.addTest("Format/MixedArray", [&build](){
		const auto out = build("\t", [](Writer& writer){
			auto aw = writer.beginArray();
			aw.write(1);

			{
				auto inner = aw.beginObject();
				inner.write("x", true);
			}

			aw.write(std::string_view("s"));
		});

		test::compare(out, std::string_view("[\n\t1,\n\t{\n\t\t\"x\": true\n\t},\n\t\"s\"\n]"));
	});

	app.addTest("Format/NestedEmptyObject", [&build](){
		const auto out = build("\t", [](Writer& writer){
			auto ow    = writer.beginObject();
			auto inner = ow.beginObject("a");
		});

		test::compare(out, std::string_view("{\n\t\"a\": {}\n}"));
	});

	app.addTest("Format/NestedEmptyArray", [&build](){
		const auto out = build("\t", [](Writer& writer){
			auto aw    = writer.beginArray();
			auto inner = aw.beginArray();
		});

		test::compare(out, std::string_view("[\n\t[]\n]"));
	});

	return app.main(argc, argv);
}

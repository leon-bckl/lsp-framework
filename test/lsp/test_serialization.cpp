#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>
#include <test/test.h>
#include <lsp/json/json.h>
#include <lsp/json/writer.h>
#include <lsp/nullable.h>
#include <lsp/serialization.h>
#include <lsp/uri.h>

using namespace lsp;

namespace lsp{

struct test_Point{
	int x;
	int y;
};

void writeJson(const test_Point& value, json::ObjectWriter& writer)
{
	lsp::writeJson("x", value.x, writer);
	lsp::writeJson("y", value.y, writer);
}

struct test_Line{
	test_Point start;
	test_Point end;
};

void writeJson(const test_Line& value, json::ObjectWriter& writer)
{
	lsp::writeJson("start", value.start, writer);
	lsp::writeJson("end", value.end, writer);
}

struct test_Polygon{
	std::vector<test_Point> points;
};

void writeJson(const test_Polygon& value, json::ObjectWriter& writer)
{
	lsp::writeJson("points", value.points, writer);
}

enum class test_Color{ Red, Green, MAX_VALUE };

template<>
const std::string_view Enumeration<test_Color, std::string>::s_values[] = { "red", "green" };

} // namespace lsp

namespace{

std::string build(auto&& fn)
{
	auto out    = std::string();
	auto writer = json::Writer(out);
	fn(writer);
	return out;
}

} // namespace

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	/*
	 * Primitives
	 */

	app.addTest("ToJson/Primitive/TopLevel", [](){
		test::compare(build([](json::Writer& writer){ writeJson(42, writer); }), std::string_view("42"));
	});

	app.addTest("ToJson/Primitive/ArrayElement", [](){
		const auto out = build([](json::Writer& writer){
			auto aw = writer.beginArray();
			writeJson(1, aw);
			writeJson(2, aw);
		});

		test::compare(out, std::string_view("[1,2]"));
	});

	app.addTest("ToJson/Primitive/ObjectProperty", [](){
		const auto out = build([](json::Writer& writer){
			auto ow = writer.beginObject();
			writeJson("a", 1, ow);
		});

		test::compare(out, std::string_view(R"({"a":1})"));
	});

	/*
	 * std::vector
	 */

	app.addTest("ToJson/Vector/TopLevel", [](){
		const auto out = build([](json::Writer& writer){
			writeJson(std::vector<int>{1, 2, 3}, writer);
		});

		test::compare(out, std::string_view("[1,2,3]"));
	});

	app.addTest("ToJson/Vector/Empty", [](){
		const auto out = build([](json::Writer& writer){
			writeJson(std::vector<int>{}, writer);
		});

		test::compare(out, std::string_view("[]"));
	});

	app.addTest("ToJson/Vector/NestedInArray", [](){
		const auto out = build([](json::Writer& writer){
			auto aw = writer.beginArray();
			writeJson(std::vector<int>{1, 2}, aw);
			writeJson(std::vector<int>{3, 4}, aw);
		});

		test::compare(out, std::string_view("[[1,2],[3,4]]"));
	});

	app.addTest("ToJson/Vector/AsObjectProperty", [](){
		const auto out = build([](json::Writer& writer){
			auto ow = writer.beginObject();
			writeJson("items", std::vector<int>{1, 2}, ow);
		});

		test::compare(out, std::string_view(R"({"items":[1,2]})"));
	});

	/*
	 * std::tuple
	 */

	app.addTest("ToJson/Tuple/TopLevel", [](){
		const auto out = build([](json::Writer& writer){
			writeJson(std::tuple<int, std::string>{1, "a"}, writer);
		});

		test::compare(out, std::string_view(R"([1,"a"])"));
	});

	/*
	 * std::unordered_map
	 */

	app.addTest("ToJson/Map/TopLevel", [](){
		const auto out = build([](json::Writer& writer){
			writeJson(std::unordered_map<std::string, int>{{"key", 1}}, writer);
		});

		test::compare(out, std::string_view(R"({"key":1})"));
	});

	app.addTest("ToJson/Map/AsArrayElement", [](){
		const auto out = build([](json::Writer& writer){
			auto aw = writer.beginArray();
			writeJson(std::unordered_map<std::string, int>{{"key", 1}}, aw);
		});

		test::compare(out, std::string_view(R"([{"key":1}])"));
	});

	app.addTest("ToJson/Map/AsObjectProperty", [](){
		const auto out = build([](json::Writer& writer){
			auto ow = writer.beginObject();
			writeJson("m", std::unordered_map<std::string, int>{{"key", 1}}, ow);
		});

		test::compare(out, std::string_view(R"({"m":{"key":1}})"));
	});

	app.addTest("ToJson/Map/UriKey", [](){
		const auto uri = Uri::parse("file:///a/b");
		const auto out = build([&uri](json::Writer& writer){
			writeJson(std::unordered_map<Uri, int>{{uri, 1}}, writer);
		});

		test::compare(out, R"({")" + uri.toString() + R"(":1})");
	});

	/*
	 * std::variant
	 */

	app.addTest("ToJson/Variant", [](std::variant<int, std::string> value, std::string_view expected){
		const auto out = build([&value](json::Writer& writer){
			writeJson(value, writer);
		});

		test::compare(out, expected);
	})({
		{"Int",    {42,  "42"}},
		{"String", {"a", R"("a")"}},
	});

	/*
	 * std::optional
	 */

	app.addTest("ToJson/Optional", [](bool hasValue, std::string_view expected){
		const auto out = build([hasValue](json::Writer& writer){
			writeJson(hasValue ? std::optional<int>(42) : std::optional<int>(), writer);
		});

		test::compare(out, expected);
	})({
		{"HasValue", {true,  "42"}},
		{"Empty",    {false, "null"}},
	});

	/*
	 * std::unique_ptr
	 */

	app.addTest("ToJson/UniquePtr/HasValue", [](){
		const auto out = build([](json::Writer& writer){
			writeJson(std::make_unique<int>(42), writer);
		});

		test::compare(out, std::string_view("42"));
	});

	app.addTest("ToJson/UniquePtr/Null", [](){
		const auto out = build([](json::Writer& writer){
			writeJson(std::unique_ptr<int>(), writer);
		});

		test::compare(out, std::string_view("null"));
	});

	/*
	 * Nullable
	 */

	app.addTest("ToJson/Nullable", [](bool hasValue, std::string_view expected){
		const auto out = build([hasValue](json::Writer& writer){
			writeJson(hasValue ? Nullable<int>(42) : Nullable<int>(nullptr), writer);
		});

		test::compare(out, expected);
	})({
		{"HasValue", {true,  "42"}},
		{"Null",     {false, "null"}},
	});

	/*
	 * NullableVariant
	 */

	app.addTest("ToJson/NullableVariant", [](bool hasValue, std::string_view expected){
		const auto out = build([hasValue](json::Writer& writer){
			writeJson(hasValue ? NullableVariant<int, std::string>(42) : NullableVariant<int, std::string>(nullptr), writer);
		});

		test::compare(out, expected);
	})({
		{"HasValue", {true,  "42"}},
		{"Null",     {false, "null"}},
	});

	/*
	 * Enumeration
	 */

	app.addTest("ToJson/Enumeration", [](std::string value, std::string_view expected){
		const auto out = build([&value](json::Writer& writer){
			writeJson(Enumeration<test_Color, std::string>(std::move(value)), writer);
		});

		test::compare(out, expected);
	})({
		{"Red",    {"red",    R"("red")"}},
		{"Green",  {"green",  R"("green")"}},
		{"Custom", {"custom", R"("custom")"}},
	});

	/*
	 * Uri
	 */

	app.addTest("ToJson/Uri", [](){
		const auto uri = Uri::parse("file:///a/b");
		const auto out = build([&uri](json::Writer& writer){
			writeJson(uri, writer);
		});

		test::compare(out, "\"" + uri.toString() + "\"");
	});

	/*
	 * Struct (stand-in for a generated type)
	 */

	app.addTest("ToJson/Struct/TopLevel", [](){
		const auto out = build([](json::Writer& writer){
			writeJson(test_Point{1, 2}, writer);
		});

		test::compare(out, std::string_view(R"({"x":1,"y":2})"));
	});

	app.addTest("ToJson/Struct/AsArrayElement", [](){
		const auto out = build([](json::Writer& writer){
			writeJson(std::vector<test_Point>{{1, 2}, {3, 4}}, writer);
		});

		test::compare(out, std::string_view(R"([{"x":1,"y":2},{"x":3,"y":4}])"));
	});

	app.addTest("ToJson/Struct/AsObjectProperty", [](){
		const auto out = build([](json::Writer& writer){
			auto ow = writer.beginObject();
			writeJson("point", test_Point{1, 2}, ow);
		});

		test::compare(out, std::string_view(R"({"point":{"x":1,"y":2}})"));
	});

	app.addTest("ToJson/Struct/NestedObject", [](){
		const auto out = build([](json::Writer& writer){
			writeJson(test_Line{{1, 2}, {3, 4}}, writer);
		});

		test::compare(out, std::string_view(R"({"start":{"x":1,"y":2},"end":{"x":3,"y":4}})"));
	});

	app.addTest("ToJson/Struct/ContainingArray", [](){
		const auto out = build([](json::Writer& writer){
			writeJson(test_Polygon{{{1, 2}, {3, 4}, {5, 6}}}, writer);
		});

		test::compare(out, std::string_view(R"({"points":[{"x":1,"y":2},{"x":3,"y":4},{"x":5,"y":6}]})"));
	});

	return app.main(argc, argv);
}

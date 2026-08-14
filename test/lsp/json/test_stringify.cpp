#include <test/test.h>
#include <lsp/json/json.h>

using namespace lsp;
using namespace lsp::json;

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	auto expectString = [](Value value, std::string_view expected)
	{
		test::compare(stringify(value), expected);
	};

	/*
	 * Literals
	 */

	app.addTest("Literals", expectString)({
		{"Null",  {nullptr, "null"}},
		{"True",  {true,    "true"}},
		{"False", {false,   "false"}},
	});

	/*
	 * Numbers
	 */

	app.addTest("Numbers", expectString)({
		{"Zero",               {0,        "0"}},
		{"PositiveInteger",    {42,       "42"}},
		{"NegativeInteger",    {-42,      "-42"}},
		{"Decimal",            {3.14,     "3.14"}},
		{"DecimalWholeNumber", {3.0,      "3.0"}},
		{"NegativeDecimal",    {-2.5,     "-2.5"}},
		{"NegativeZero",       {-0.0,     "-0.0"}},
		{"SmallDecimal",       {0.001,    "0.001"}},
		{"SmallFixed",         {0.0001,   "0.0001"}},
		{"LargeFixed",         {100000.0, "100000.0"}},
		{"SmallScientific",    {1e-7,     "1e-07"}},
		{"LargeScientific",    {1e21,     "1e+21"}},
	});

	/*
	 * Strings
	 */

	app.addTest("Strings", expectString)({
		{"Empty",                 {"",              R"("")"}},
		{"Simple",                {"hello",         R"("hello")"}},
		{"QuoteEscape",           {"a\"b",          R"("a\"b")"}},
		{"BackslashEscape",       {"a\\b",          R"("a\\b")"}},
		{"BackspaceEscape",       {"a\bb",          R"("a\bb")"}},
		{"TabEscape",             {"a\tb",          R"("a\tb")"}},
		{"NewlineEscape",         {"a\nb",          R"("a\nb")"}},
		{"FormFeedEscape",        {"a\fb",          R"("a\fb")"}},
		{"CarriageReturnEscape",  {"a\rb",          R"("a\rb")"}},
		{"UnnamedControlChar",    {"a\ab",          R"("a\u0007b")"}},
		{"MixedEscapes",          {"a\tb\nc\\d\"e", R"("a\tb\nc\\d\"e")"}},
		{"UnicodePassedThrough",  {"中",            "\"中\""}},
	});

	/*
	 * Arrays
	 */

	app.addTest("Arrays", expectString)({
		{"Empty",         {Array(),          "[]"}},
		{"SingleElement", {Array({{1}}),     "[1]"}},
		{"MultiElement",  {Array({1, 2, 3}), "[1,2,3]"}},
		{"Nested",        {Array({
			Array({1, 2}),
			Array({3, 4})}
		), "[[1,2],[3,4]]"}},
		{"Mixed",         {Array({
			1,
			"two",
			true,
			nullptr}
		), R"([1,"two",true,null])"}},
	});

	/*
	 * Objects
	 */

	app.addTest("Objects", expectString)({
		{"Empty",     {Object(),                                     "{}"}},
		{"SingleKey", {Object({{"a", 1}}),                           R"({"a":1})"}},
		{"MultiKey",  {Object({{"a", 1}, {"b", 2}, {"c", 3}}),       R"({"a":1,"b":2,"c":3})"}},
		{"Nested",    {Object({{"outer", Object({{"inner", 42}})}}), R"({"outer":{"inner":42}})"}},
		{"KeyEscaping", {Object({{"a\"b", 1}}),                      R"({"a\"b":1})"}},
	});

	/*
	 * Composite
	 */

	app.addTest("Composite", expectString)({
		{"NestedArrayAndObject",
		 {Object({
		      {"a", Array({1, 2, Object({{"b", true}})})},
		      {"c", nullptr}}
		  ), R"({"a":[1,2,{"b":true}],"c":null})"}},
	});

	/*
	 * Format
	 */

	auto expectFormatted = [](Value value, std::string_view expected)
	{
		test::compare(stringify(value, true), expected);
	};

	app.addTest("Format", expectFormatted)({
		{"EmptyObject",        {Object(),            "{}"}},
		{"EmptyArray",         {Array(),             "[]"}},
		{"SingleKeyObject",    {Object({{"a", 1}}),  "{\n\t\"a\": 1\n}"}},
		{"SingleElementArray", {Array({{1}}),        "[\n\t1\n]"}},
		{"NestedObject",       {Object({{"a", 1}, {"b", Array({1, 2})}}),
		                        "{\n\t\"a\": 1,\n\t\"b\": [\n\t\t1,\n\t\t2\n\t]\n}"}},
		{"MixedArray",         {Array({1, Object({{"x", true}}), "s"}),
		                        "[\n\t1,\n\t{\n\t\t\"x\": true\n\t},\n\t\"s\"\n]"}},
		{"NestedEmptyObject",  {Object({{"a", Object()}}), "{\n\t\"a\": {}\n}"}},
		{"NestedEmptyArray",   {Array({{Array()}}),        "[\n\t[]\n]"}},
	});

	return app.main(argc, argv);
}

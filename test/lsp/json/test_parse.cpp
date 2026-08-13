#include <test/test.h>
#include <lsp/json/json.h>
#include <limits>

using namespace lsp;
using namespace lsp::json;

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	auto expectValue = [](std::string_view text, Value expected)
	{
		test::compare(parse(text), expected);
	};

	app.addTest("Literals", expectValue)({
		{"Null",  {"null",  nullptr}},
		{"True",  {"true",  true}},
		{"False", {"false", false}},
	});

	app.addTest("Numbers", expectValue)({
		{"Zero",                        {"0",              0}},
		{"PositiveInteger",             {"42",             42}},
		{"NegativeInteger",             {"-42",            -42}},
		{"NegativeZero",                {"-0",             0}},
		{"Int32Max",                    {"2147483647",     std::numeric_limits<Integer>::max()}},
		{"Int32Min",                    {"-2147483648",    std::numeric_limits<Integer>::min()}},
		{"IntegerOverflowToDecimal",    {"2147483648",     2147483648.0}}, // Out of range 32 bit int falls back to Decimal
		{"Decimal",                     {"3.14",           3.14}},
		{"NegativeDecimal",             {"-2.5",           -2.5}},
		{"FractionOnly",                {"0.5",            0.5}},
		{"ExponentLower",               {"1e10",           1e10}},
		{"ExponentUpper",               {"1E2",            100.0}},
		{"NegativeExponentLower",       {"1e-10",          1e-10}},
		{"NegativeExponentUpper",       {"1E-2",           0.01}},
		{"DecimalWithExponent",         {"1.5e3",          1500.0}},
		{"DecimalWithNegativeExponent", {"1.5e-3",         0.0015}},
		{"NegativeDecimalWithNegativeExponent",{"-1.5e-3", -0.0015}},
		{"FractionBeforeExponent",      {"0.001e3",        1.0}},
		{"PositiveExponentLower",       {"1e+10",          1e10}},
		{"PositiveExponentUpper",       {"1E+2",           100.0}},
		{"DecimalWithPositiveExponent", {"1.5e+3",         1500.0}},
	});

	app.addTest("Strings", expectValue)({
		{"Empty",                      {R"("")",                ""}},
		{"Simple",                     {R"("hello")",           "hello"}},
		{"NullEscape",                 {R"("\0")",              String(1, '\0')}},
		{"BellEscape",                 {R"("\a")",              "\a"}},
		{"BackspaceEscape",            {R"("\b")",              "\b"}},
		{"TabEscape",                  {R"("\t")",              "\t"}},
		{"NewlineEscape",              {R"("\n")",              "\n"}},
		{"VerticalTabEscape",          {R"("\v")",              "\v"}},
		{"FormFeedEscape",             {R"("\f")",              "\f"}},
		{"CarriageReturnEscape",       {R"("\r")",              "\r"}},
		{"QuoteEscape",                {R"("quote\"here")",     "quote\"here"}},
		{"BackslashEscape",            {R"("back\\slash")",     "back\\slash"}},
		{"ForwardSlashEscape",         {R"("forward\/slash")",  "forward/slash"}},
		{"UnicodeEscape",              {R"("unicode: \u00e9")", "unicode: \u00e9"}},
		{"MixedEscapes",               {R"("a\tb\nc\\d\"eéf")", "a\tb\nc\\d\"eéf"}},
		{"TruncatedUnicodeEscape",     {R"("\u12")",            R"(\u12)"}},
		{"InvalidHexUnicodeEscape",    {R"("\u12zz")",          R"(\u12zz)"}},
		{"AllInvalidHexUnicodeEscape", {R"("\uZZZZ")",          R"(\uZZZZ)"}},
		{"UnknownEscapeChar",          {R"("\q")",              "q"}},
		{"UnicodeEscapeAscii",         {R"("\u0041")",          "A"}},
		{"UnicodeEscapeThreeByte",     {R"("\u4e2d")",          "中"}},
	});

	app.addTest("Arrays", expectValue)({
		{"Empty",         {"[]",      Array()}},
		{"SingleElement", {"[1]",     Array({{1}})}},
		{"MultiElement",  {"[1,2,3]", Array({1, 2, 3})}},
		{"Nested",        {"[[1,2],[3,4]]",
			Array({
				Array({1, 2}),
				Array({3, 4})}
			)}},
		{"Mixed",         {R"([1,"two",true,null])",
			Array({
				1,
				"two",
				true,
				nullptr}
			)}},
	});

	app.addTest("Objects", expectValue)({
		{"Empty",     {"{}",                        Object()}},
		{"SingleKey", {R"({"a":1})",                Object({{"a", 1}})}},
		{"MultiKey",  {R"({"a":1,"b":2,"c":3})",    Object({{"a", 1}, {"b", 2}, {"c", 3}})}},
		{"Nested",    {R"({"outer":{"inner":42}})", Object({{"outer", Object({{"inner", 42}})}})}},
	});

	app.addTest("Composite", expectValue)({
		{"WhitespaceTolerance",
		 {"  \t\n  { \"a\" : 1 , \"b\" : [ 1 , 2 ] }  \t\n ",
		  Object({
		      {"a", 1},
		      {"b", Array({1, 2})}}
		  )}},
		{"NestedArrayAndObject",
		 {R"( { "a" : [ 1 , 2 , { "b" : true } ] , "c" : null } )",
		  Object({
		      {"a", Array({1, 2, Object({{"b", true}})})},
		      {"c", nullptr}}
		  )}},
	});

	app.addTest("Errors", [](std::string_view text, std::string_view expectedMessage, std::size_t expectedPos)
	{
		try
		{
			(void)parse(text);
			test::fail("Expected ParseError");
		}
		catch(const ParseError& e)
		{
			test::compare(e.what(), expectedMessage);
			test::compare(e.textPos(), expectedPos);
		}
	})({
		{"EmptyInput",           {"",                 "Unexpected end of input",       0}},
		{"UnterminatedObject",   {"{",                "Unexpected end of input",       1}},
		{"UnterminatedArray",    {"[",                "Unexpected end of input",       1}},
		{"UnterminatedString",   {R"("abc)",          "Unmatched '\"'",                0}},
		{"MissingColon",         {R"({"a" 1})",       "Expected ':'",                  5}},
		{"MissingCommaObject",   {R"({"a":1 "b":2})", "Expected ','",                  7}},
		{"TrailingCommaObject",  {R"({"a":1,})",      "Trailing ','",                  6}},
		{"MissingCommaArray",    {"[1 2]",            "Expected ','",                  3}},
		{"TrailingCommaArray",   {"[1,]",             "Trailing ','",                  2}},
		{"DuplicateKey",         {R"({"a":1,"a":2})", "Duplicate key 'a'",             7}},
		{"TrailingCharacters",   {"true false",       "Trailing characters in json",   5}},
		{"UnexpectedToken",      {"#",                "Unexpected token",              0}},
		{"UnknownIdentifier",    {"nul",              "Unexpected 'nul'",              0}},
		{"CaseSensitiveLiteral", {"True",             "Unexpected 'True'",             0}},
		{"LiteralPrefix",        {"truefoo",          "Unexpected 'truefoo'",          0}},
		{"MalformedDecimal",     {"1.2.3",            "Invalid number value: '1.2.3'", 0}},
		{"DanglingExponent",     {"1e",               "Invalid number value: '1e'",    0}},
		{"LoneMinusSign",        {"-",                "Invalid number value: '-'",     0}},
		{"UnquotedKey",          {"{a:1}",            "String expected",               1}},
	});

	// This just covers the removal of the quotes. The rest is already tested in the string parsing test.
	app.addTest("FromStringLiteral", [](std::string_view input, std::string_view expected)
	{
		test::compare(fromStringLiteral(input), expected);
	})({
		{"WithQuotes",        {R"("hello")", "hello"}},
		{"WithoutQuotes",     {"hello",      "hello"}},
		{"LeadingQuoteOnly",  {R"("hello)",  "hello"}},
		{"TrailingQuoteOnly", {R"(hello")",  "hello"}},
		{"Empty",             {"",           ""}},
	});

	return app.main(argc, argv);
}

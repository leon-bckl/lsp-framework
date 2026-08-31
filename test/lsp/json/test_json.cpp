#include <test/test.h>
#include <lsp/json/json.h>

using namespace lsp;
using namespace lsp::json;

namespace test{

template<>
std::string toString<Value>(const Value& v)
{
	return std::visit([](const auto& actualValue){
		return test::toString(actualValue);
	}, v.variant());
}

} // namespace test

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	/*
	 * Value
	 */

	app.addTest("Value/Null", [](){
		const auto implicitNullVal = Value();
		test::check(implicitNullVal.isNull(), "implicitIsNull");

		const auto explicitNullVal = Value(nullptr);
		test::check(explicitNullVal.isNull(), "explicitIsNull");
	});

	app.addTest("Value/Boolean", [](){
		const auto boolVal = Value(true);
		test::check(boolVal.isBoolean(), "isBoolean");
		test::compare(boolVal.boolean(), true);
		test::check(!boolVal.isNumber(), "!isNumber");
	});

	app.addTest("Value/Integer", [](){
		const auto intVal = Value(42);
		test::check(intVal.isInteger(), "isInteger");
		test::check(intVal.isNumber(), "intIsNumber");
		test::compare(intVal.integer(), 42);
		test::check(!intVal.isDecimal(), "!isDecimal");
	});

	app.addTest("Value/Decimal", [](){
		const auto decVal = Value(3.14);
		test::check(decVal.isDecimal(), "isDecimal");
		test::check(decVal.isNumber(), "decimalIsNumber");
		test::compare(decVal.decimal(), 3.14);
		test::check(!decVal.isInteger(), "!isInteger");
	});

	app.addTest("Value/String", [](){
		const auto cstrVal = Value("test");
		test::check(cstrVal.isString(), "isString");
		test::compare(cstrVal.string(), "test");

		const auto strViewVal = Value(std::string_view("test"));
		test::check(strViewVal.isString(), "isString");
		test::compare(strViewVal.string(), "test");

		const auto strVal = Value(String("test"));
		test::check(strVal.isString(), "isString");
		test::compare(strVal.string(), "test");
	});

	app.addTest("Value/Array", [](){
		const auto arrVal = Value(Array({Value(42)}));
		test::check(arrVal.isArray(), "isArray");
		test::compare(arrVal.array().size(), 1);
		test::compare(arrVal.array()[0], 42);
	});

	app.addTest("Value/Object", [](){
		const auto objVal = Value(Object({{"key", "test"}}));
		test::check(objVal.isObject(), "isObject");
		test::compare(objVal.object().size(), 1);
		test::compare(objVal.object().get("key"), "test");
	});

	app.addTest("Value/TypeErrors", [](){
		const auto val = Value();

		test::expectException<TypeError>([&](){ (void)val.boolean(); }, "JSON value is not boolean");
		test::expectException<TypeError>([&](){ (void)val.integer(); }, "JSON value is not integer");
		test::expectException<TypeError>([&](){ (void)val.decimal(); }, "JSON value is not decimal");
		test::expectException<TypeError>([&](){ (void)val.string(); }, "JSON value is not string");
		test::expectException<TypeError>([&](){ (void)val.object(); }, "JSON value is not object");
		test::expectException<TypeError>([&](){ (void)val.array(); }, "JSON value is not array");
		test::expectException<TypeError>([&](){ (void)val.number(); }, "JSON value is not number");
	});

	/*
	 * Object
	 */

	app.addTest("Object/DefaultConstruct", [](){
		const auto obj = Object();
		test::compare(obj.size(), 0);
		test::check(obj.isEmpty(), "isEmpty");
	});

	app.addTest("Object/InitializerList", [](){
		const auto obj = Object{{"a", 1}, {"b", "x"}};
		test::compare(obj.size(), 2);
		test::check(obj.contains("a"), "containsA");
		test::check(obj.contains("b"), "containsB");
		test::compare(obj.get("a").integer(), 1);
		test::compare(obj.get("b").string(), "x");
	});

	app.addTest("Object/InsertNewKey", [](){
		auto obj = Object();
		auto& ref = obj.insert("key", Value(1));
		test::compare(obj.size(), 1);
		test::check(obj.contains("key"), "contains");
		test::compare(ref.integer(), 1);
		test::compare(obj.get("key").integer(), 1);
	});

	app.addTest("Object/InsertExistingKey", [](){
		auto obj = Object();
		obj.insert("key", Value(1));
		auto& ref = obj.insert("key", Value(2));
		test::compare(obj.size(), 1);
		test::compare(ref.integer(), 2);
		test::compare(obj.get("key").integer(), 2);
	});

	app.addTest("Object/AppendNewKey", [](){
		auto obj = Object();
		auto& ref = obj.append("key", Value(1));
		test::compare(obj.size(), 1);
		test::check(obj.contains("key"), "contains");
		test::compare(ref.integer(), 1);
		test::compare(obj.get("key").integer(), 1);
	});

	app.addTest("Object/AppendDuplicateKey", [](){
		auto obj = Object();
		obj.append("key", Value(1));
		auto& ref = obj.append("key", Value(2));

		// append does not check for collisions, so both entries coexist
		test::compare(obj.size(), 2);
		test::compare(ref.integer(), 2);

		// find/get return the first matching entry
		test::compare(obj.get("key").integer(), 1);

		std::vector<int> values;
		for(auto& [key, value] : obj)
			values.push_back(value.integer());
		test::compare(values, std::vector<int>{1, 2});
	});

	app.addTest("Object/Remove", [](){
		auto obj = Object();
		obj.insert("key", Value(1));

		obj.remove("key");
		test::check(!obj.contains("key"), "!contains");
		test::compare(obj.size(), 0);

		obj.remove("missing"); // no-op
		test::compare(obj.size(), 0);
	});

	app.addTest("Object/Clear", [](){
		auto obj = Object();
		obj.insert("a", Value(1));
		obj.insert("b", Value(2));

		obj.clear();
		test::compare(obj.size(), 0);
		test::check(obj.isEmpty(), "isEmpty");
	});

	app.addTest("Object/Reserve", [](){
		auto obj = Object();

		obj.reserve(100);
		test::check(obj.capacity() >= 100, "capacity");
		test::check(obj.isEmpty(), "isEmpty");
	});

	app.addTest("Object/SubscriptOperator", [](){
		auto obj = Object();

		auto& inserted = obj["key"];
		test::check(inserted.isNull(), "defaultIsNull");
		test::compare(obj.size(), 1);

		inserted = Value(42);
		test::compare(obj["key"].integer(), 42);
		test::compare(obj.size(), 1);
	});

	app.addTest("Object/Find", [](){
		auto obj = Object();
		obj.insert("key", Value(1));

		auto* found = obj.find("key");
		test::check(found != nullptr, "found");
		test::compare(found->integer(), 1);
		test::check(obj.find("missing") == nullptr, "missingIsNull");

		const auto& constObj = obj;
		const auto* constFound = constObj.find("key");
		test::check(constFound != nullptr, "constFound");
		test::compare(constFound->integer(), 1);
	});

	app.addTest("Object/Contains", [](){
		auto obj = Object();
		obj.insert("key", Value(1));

		test::check(obj.contains("key"), "contains");
		test::check(!obj.contains("missing"), "!contains");
	});

	app.addTest("Object/Get", [](){
		auto obj = Object();
		obj.insert("key", Value(1));

		test::compare(obj.get("key").integer(), 1);
		test::expectException<TypeError>([&](){ (void)obj.get("missing"); });
	});

	app.addTest("Object/Equality", [](){
		const auto a = Object{{"a", 1}, {"b", 2}};
		const auto b = Object{{"b", 2}, {"a", 1}}; // different insertion order
		test::check(a == b, "differentOrderEqual");

		const auto empty1 = Object();
		const auto empty2 = Object();
		test::check(empty1 == empty2, "emptyEqual");

		const auto differentSize = Object{{"a", 1}};
		test::check(a != differentSize, "differentSizeNotEqual");

		const auto differentValue = Object{{"a", 1}, {"b", 3}};
		test::check(a != differentValue, "differentValueNotEqual");

		const auto differentKeys = Object{{"a", 1}, {"c", 2}}; // same size, "b" replaced by "c"
		test::check(a != differentKeys, "differentKeysNotEqual");
	});

	app.addTest("Object/CopyIndependence", [](){
		auto original = Object();
		original.insert("nested", Value(Object{{"x", 1}}));

		auto copy = original;
		copy.get("nested").object().insert("y", Value(2));

		test::compare(copy.get("nested").object().size(), 2);
		test::compare(original.get("nested").object().size(), 1);
	});

	app.addTest("Object/Move", [](){
		auto a = Object();
		a.insert("key", Value(1));
		auto moveConstructed = std::move(a);
		test::compare(moveConstructed.get("key").integer(), 1);

		auto c = Object{{"e", 5}, {"f", 6}};
		auto moveAssignedOverExisting = Object{{"old", 99}};
		moveAssignedOverExisting = std::move(c);
		test::compare(moveAssignedOverExisting.size(), 2);
		test::check(moveAssignedOverExisting.contains("e"), "containsE");
		test::check(moveAssignedOverExisting.contains("f"), "containsF");
		test::check(!moveAssignedOverExisting.contains("old"), "!containsOld");
	});

	app.addTest("Object/Assignment", [](){
		auto target = Object{{"c", 1}, {"d", 2}};
		const auto source = Object{{"a", 3}, {"b", 4}};

		target = source;
		test::compare(target.size(), 2);
		test::check(target.contains("a"), "containsA");
		test::check(target.contains("b"), "containsB");
		test::check(!target.contains("c"), "!containsC");
		test::check(!target.contains("d"), "!containsD");

		auto& selfRef = target;
		target = selfRef; // self-assignment, avoiding -Wself-assign-overloaded
		test::compare(target.size(), 2);
		test::check(target.contains("a"), "selfAssignContainsA");
	});

	app.addTest("Object/Iteration", [](){
		auto obj = Object{{"a", 1}, {"b", 2}, {"c", 3}};

		std::vector<int> values;
		for(auto& [key, value] : obj)
			values.push_back(value.integer());

		test::compare(values, std::vector<int>{1, 2, 3});

		auto collectKeys = [](auto first, auto last)
		{
			std::vector<std::string> keys;
			for(; first != last; ++first)
				keys.push_back(first->key());
			return keys;
		};

		test::compare(collectKeys(obj.begin(), obj.end()), std::vector<std::string>{"a", "b", "c"});

		const auto& constObj = obj;
		test::compare(collectKeys(constObj.begin(), constObj.end()), std::vector<std::string>{"a", "b", "c"});

		test::compare(collectKeys(obj.cbegin(), obj.cend()), std::vector<std::string>{"a", "b", "c"});
	});

	app.addTest("Stringify", [](Value value, std::string_view expected)
	{
		test::compare(stringify(value), expected);
	})({
		{"Null", {nullptr, "null"}},
		{"True", {true,    "true"}},

		{"Integer", {42,   "42"}},
		{"Decimal", {3.14, "3.14"}},

		{"Simple",      {"hello", R"("hello")"}},
		{"QuoteEscape", {"a\"b",  R"("a\"b")"}},

		{"SimpleObject", {Object({{"a", 1}}), R"({"a":1})"}},
		{"SimpleArray",  {Array({1, 2}),      "[1,2]"}},
	});

	/*
	 * Parse is implemented using json::Parser so the test cases here are minimal.
	 */

	app.addTest("Parse", [](std::string_view text, Value expected)
	{
		test::compare(parse(text), expected);
	})({
		{"Null",         {"null",       nullptr}},
		{"True",         {"true",       true}},
		{"False",        {"false",      false}},
		{"Integer",      {"42",         42}},
		{"Decimal",      {"3.14",       3.14}},
		{"String",       {R"("hello")", "hello"}},
		{"SimpleObject", {R"({"a":1})", Object({{"a", 1}})}},
		{"SimpleArray",  {"[1,2]",      Array({1, 2})}},
	});

	app.addTest("Parse/Error", []()
	{
		test::expectException<ParseError>([](){ (void)parse("{"); });
	});

	return app.main(argc, argv);
}

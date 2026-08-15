#include <test/test.h>
#include <lsp/enumeration.h>
#include <string>

enum class Color{
	Red,
	Green,
	Blue,
	MAX_VALUE
};

enum class Status{
	Ok,
	Warning,
	Error,
	MAX_VALUE
};

namespace lsp{

using ColorEnum = Enumeration<Color, std::string>;
template<>
const ColorEnum::ConstInitType ColorEnum::s_values[] = {
	"red",
	"green",
	"blue"
};

using StatusEnum = Enumeration<Status, int>;
template<>
const StatusEnum::ConstInitType StatusEnum::s_values[] = {
	0,
	100,
	200
};

} // namespace lsp

using namespace lsp;

template<typename EnumType, typename ValueType>
void expectKnownValue(const Enumeration<EnumType, ValueType>& e, EnumType expectedIndex,
                       typename Enumeration<EnumType, ValueType>::ConstInitType expectedValue)
{
	test::check(!e.hasCustomValue());
	test::compare(e.index(), expectedIndex);
	test::compare(e.value(), expectedValue);
}

template<typename EnumType, typename ValueType>
void expectCustomValue(const Enumeration<EnumType, ValueType>& e,
                        typename Enumeration<EnumType, ValueType>::ConstInitType expectedValue)
{
	test::check(e.hasCustomValue());
	test::compare(e.index(), EnumType::MAX_VALUE);
	test::compare(e.value(), expectedValue);
}

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("DefaultConstruct", [](){
		auto e = ColorEnum();
		test::check(e.hasCustomValue());
		test::compare(e.index(), Color::MAX_VALUE);
	});

	app.addTest("ConstructFromIndex", [](){
		auto e = ColorEnum(Color::Green);
		expectKnownValue(e, Color::Green, "green");
	});

	app.addTest("ConstructFromMatchingValue", [](){
		auto e = ColorEnum(std::string("blue"));
		expectKnownValue(e, Color::Blue, "blue");
	});

	app.addTest("ConstructFromCustomValue", [](){
		auto e = ColorEnum(std::string("magenta"));
		expectCustomValue(e, "magenta");
	});

	app.addTest("AssignIndex", [](){
		auto e = ColorEnum();
		e = Color::Red;
		expectKnownValue(e, Color::Red, "red");
	});

	app.addTest("AssignValueSwitchesFromCustomToKnown", [](){
		auto e = ColorEnum(std::string("custom"));
		test::check(e.hasCustomValue());

		e = std::string("red");
		expectKnownValue(e, Color::Red, "red");
	});

	app.addTest("EqualityWithEnumType", [](){
		auto e = ColorEnum(Color::Blue);
		test::check(e == Color::Blue);
		test::check(!(e != Color::Blue));
		test::check(e != Color::Red);
	});

	app.addTest("EqualityWithValue", [](){
		auto e = ColorEnum(Color::Blue);
		test::check(e == "blue");
		test::check(!(e != "blue"));
		test::check(e != "red");

		auto custom = ColorEnum(std::string("teal"));
		test::check(custom == "teal");
	});

	app.addTest("ConversionOperators", [](){
		auto        e   = ColorEnum(Color::Red);
		std::string str = e;
		Color       c   = e;
		test::compare(str, "red");
		test::compare(c, Color::Red);
	});

	app.addTest("StaticValueLookup", [](){
		test::compare(ColorEnum::value(Color::Green), "green");
		test::compare(ColorEnum::value(Color::Blue), "blue");
	});

	app.addTest("IntEnumConstructFromIndex", [](){
		auto s = StatusEnum(Status::Warning);
		expectKnownValue(s, Status::Warning, 100);
	});

	app.addTest("IntEnumConstructFromMatchingValue", [](){
		auto s = StatusEnum(200);
		expectKnownValue(s, Status::Error, 200);
	});

	app.addTest("IntEnumConstructFromCustomValue", [](){
		auto s = StatusEnum(999);
		expectCustomValue(s, 999);
	});

	app.addTest("IntEnumEqualityWithValue", [](){
		auto s = StatusEnum(Status::Ok);
		test::check(s == 0);
		test::check(!(s != 0));
		test::check(s != 100);
	});

	app.addTest("IntEnumEqualityWithEnumType", [](){
		auto s = StatusEnum(Status::Warning);
		test::check(s == Status::Warning);
		test::check(!(s != Status::Warning));
		test::check(s != Status::Ok);
	});

	app.addTest("IntEnumConversionOperators", [](){
		auto   s = StatusEnum(Status::Error);
		int    i = s;
		Status t = s;
		test::compare(i, 200);
		test::compare(t, Status::Error);
	});

	return app.main(argc, argv);
}

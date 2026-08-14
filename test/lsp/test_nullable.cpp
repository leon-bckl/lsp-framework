#include <string>
#include <variant>
#include <test/test.h>
#include <lsp/nullable.h>

using namespace lsp;

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("Nullable::DefaultConstruct", [](){
		auto n = Nullable<int>();
		test::check(n.isNull());
	});

	app.addTest("Nullable::NullptrConstruct", [](){
		auto n = Nullable<int>(nullptr);
		test::check(n.isNull());
	});

	app.addTest("Nullable::CopyConstruct", [](){
		const int  value = 5;
		auto       n     = Nullable<int>(value);
		test::check(!n.isNull());
		test::compare(n.value(), 5);
	});

	app.addTest("Nullable::MoveConstruct", [](){
		auto n = Nullable<std::string>(std::string("hello"));
		test::check(!n.isNull());
		test::compare(n.value(), "hello");
	});

	app.addTest("Nullable::AssignValue", [](){
		auto n = Nullable<int>();
		n = 10;
		test::check(!n.isNull());
		test::compare(n.value(), 10);
	});

	app.addTest("Nullable::AssignMovedValue", [](){
		auto n = Nullable<std::string>();
		n = std::string("moved");
		test::check(!n.isNull());
		test::compare(n.value(), "moved");
	});

	app.addTest("Nullable::AssignNullptrResets", [](){
		auto n = Nullable<int>(5);
		n = nullptr;
		test::check(n.isNull());
	});

	app.addTest("Nullable::Emplace", [](){
		auto n = Nullable<std::string>();
		n.emplace(3, 'x');
		test::check(!n.isNull());
		test::compare(n.value(), "xxx");
	});

	app.addTest("Nullable::Reset", [](){
		auto n = Nullable<int>(5);
		n.reset();
		test::check(n.isNull());
	});

	app.addTest("Nullable::Dereference", [](){
		auto n = Nullable<int>(5);
		test::compare(*n, 5);

		*n = 6;
		test::compare(n.value(), 6);
	});

	app.addTest("Nullable::ArrowOperator", [](){
		auto n = Nullable<std::string>(std::string("hello"));
		test::compare(n->size(), std::string("hello").size());

		n->push_back('!');
		test::compare(n.value(), "hello!");
	});

	app.addTest("Nullable::ConstAccess", [](){
		const auto n = Nullable<int>(42);
		test::check(!n.isNull());
		test::compare(n.value(), 42);
		test::compare(*n, 42);
	});

	app.addTest("NullableVariant::DefaultConstruct", [](){
		auto n = NullableVariant<int, std::string>();
		test::check(n.isNull());
	});

	app.addTest("NullableVariant::NullptrConstruct", [](){
		auto n = NullableVariant<int, std::string>(nullptr);
		test::check(n.isNull());
	});

	app.addTest("NullableVariant::ConstructFromAlternative", [](){
		auto n = NullableVariant<int, std::string>(5);
		test::check(!n.isNull());
		test::check(n.holdsAlternative<int>(), "holdsInt");
		test::check(!n.holdsAlternative<std::string>(), "doesNotHoldString");
		test::compare(n.get<int>(), 5);
	});

	app.addTest("NullableVariant::ConstructFromOtherAlternative", [](){
		auto n = NullableVariant<int, std::string>(std::string("hi"));
		test::check(n.holdsAlternative<std::string>(), "holdsString");
		test::compare(n.get<std::string>(), "hi");
	});

	app.addTest("NullableVariant::AssignSwitchesAlternative", [](){
		auto n = NullableVariant<int, std::string>();

		n = 42;
		test::check(n.holdsAlternative<int>(), "holdsIntAfterFirstAssign");
		test::compare(n.get<int>(), 42);

		n = std::string("foo");
		test::check(n.holdsAlternative<std::string>(), "holdsStringAfterSecondAssign");
		test::check(!n.holdsAlternative<int>(), "noLongerHoldsInt");
		test::compare(n.get<std::string>(), "foo");
	});

	app.addTest("NullableVariant::AssignNullptrResets", [](){
		auto n = NullableVariant<int, std::string>(5);
		n = nullptr;
		test::check(n.isNull());
	});

	app.addTest("NullableVariant::EmplaceVariantCopy", [](){
		auto n = NullableVariant<int, std::string>();
		const std::variant<int, std::string> v = std::string("v");
		n.emplace(v);
		test::check(n.holdsAlternative<std::string>());
		test::compare(n.get<std::string>(), "v");
	});

	app.addTest("NullableVariant::EmplaceVariantMove", [](){
		auto n = NullableVariant<int, std::string>();
		n.emplace(std::variant<int, std::string>(7));
		test::check(n.holdsAlternative<int>());
		test::compare(n.get<int>(), 7);
	});

	app.addTest("NullableVariant::EmplaceTyped", [](){
		auto n = NullableVariant<int, std::string>();
		n.emplace<std::string>(3, 'y');
		test::check(n.holdsAlternative<std::string>());
		test::compare(n.get<std::string>(), "yyy");
	});

	app.addTest("NullableVariant::Reset", [](){
		auto n = NullableVariant<int, std::string>(5);
		n.reset();
		test::check(n.isNull());
	});

	app.addTest("NullableVariant::ValueAndDereference", [](){
		auto n = NullableVariant<int, std::string>(5);
		test::compare(std::get<int>(n.value()), 5);
		test::compare(std::get<int>(*n), 5);
	});

	app.addTest("NullableVariant::ConstGet", [](){
		const auto n = NullableVariant<int, std::string>(std::string("const"));
		test::compare(n.get<std::string>(), "const");
	});

	return app.main(argc, argv);
}

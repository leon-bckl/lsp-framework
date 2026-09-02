#include <array>
#include <concepts>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <test/test.h>
#include <lsp/task_function.h>

using namespace lsp;

namespace{

// Counts live instances and copies so tests can verify that captured state is
// moved into the task and destroyed exactly once.
struct Tracker{
	static inline int alive  = 0;
	static inline int copies = 0;

	static void reset(){ alive = 0; copies = 0; }

	Tracker(){ ++alive; }
	Tracker(const Tracker&){ ++alive; ++copies; }
	Tracker(Tracker&&) noexcept{ ++alive; }
	Tracker& operator=(const Tracker&) = delete;
	Tracker& operator=(Tracker&&)      = delete;
	~Tracker(){ --alive; }
};

} // namespace

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("TaskFunction/ReturnsValue", [](){
		auto task = TaskFunction<int>([]{ return 42; });
		test::check(static_cast<bool>(task), "valid");
		test::compare(std::move(task).run(), 42);
	});

	app.addTest("TaskFunction/ReturnsVoid", [](){
		auto ran  = false;
		auto task = TaskFunction<void>([&]{ ran = true; });
		std::move(task).run();
		test::check(ran, "ran");
	});

	app.addTest("TaskFunction/MoveOnlyResult", [](){
		auto task = TaskFunction<std::unique_ptr<int>>([]{ return std::make_unique<int>(7); });
		test::compare(*std::move(task).run(), 7);
	});

	app.addTest("TaskFunction/MoveOnlyCapture", [](){
		auto task = TaskFunction<int>([p = std::make_unique<int>(5)]{ return *p; });
		test::compare(std::move(task).run(), 5);
	});

	app.addTest("TaskFunction/BoundArguments", [](){
		auto task = TaskFunction<int>([](int a, int b){ return a * b; }, 6, 7);
		test::compare(std::move(task).run(), 42);
	});

	app.addTest("TaskFunction/MoveOnlyBoundArgument", [](){
		auto task = TaskFunction<int>([](std::unique_ptr<int> p){ return *p; }, std::make_unique<int>(9));
		test::compare(std::move(task).run(), 9);
	});

	app.addTest("TaskFunction/VoidResultDiscardsReturnValue", [](){
		auto ran  = false;
		auto task = TaskFunction<void>([&]{ ran = true; return 42; });
		std::move(task).run();
		test::check(ran, "ran");
	});

	app.addTest("TaskFunction/VoidResultDestroysDiscardedReturnValue", [](){
		Tracker::reset();
		auto task = TaskFunction<void>([]{ return Tracker(); });
		std::move(task).run();
		test::compare(Tracker::alive, 0);
	});

	app.addTest("TaskFunction/CallOperatorRunsTask", [](){
		auto task = TaskFunction<int>([]{ return 21; });
		test::compare(std::move(task)(), 21);
	});

	app.addTest("TaskFunction/PropagatesException", [](){
		auto task = TaskFunction<int>([]() -> int { throw std::runtime_error("boom"); });
		test::expectException<std::runtime_error>([&](){ (void)std::move(task).run(); }, "boom");
	});

	app.addTest("TaskFunction/LargeCaptureUsesHeapStorage", [](){
		auto big = std::array<int, 64>();
		big.fill(2);

		auto task = TaskFunction<int>([big]{
			auto sum = 0;
			for(const auto value : big)
				sum += value;
			return sum;
		});

		test::compare(std::move(task).run(), 128);
	});

	app.addTest("TaskFunction/EmptyIsFalsy", [](){
		test::check(!TaskFunction<int>(), "defaultConstructedFalsy");
		test::check(!TaskFunction<int>(nullptr), "nullptrConstructedFalsy");
	});

	app.addTest("TaskFunction/RunConsumesTask", [](){
		auto task = TaskFunction<int>([]{ return 1; });
		(void)std::move(task).run();
		test::check(!task, "falsyAfterRun");
	});

	app.addTest("TaskFunction/MoveConstructionTransfersState", [](){
		auto original = TaskFunction<int>([]{ return 3; });
		auto moved    = std::move(original);

		test::check(!original, "sourceEmpty");
		test::check(static_cast<bool>(moved), "targetValid");
		test::compare(std::move(moved).run(), 3);
	});

	app.addTest("TaskFunction/MoveAssignmentDestroysReplacedTask", [](){
		Tracker::reset();
		{
			auto task = TaskFunction<void>([t = Tracker()]{ (void)t; });
			task = TaskFunction<void>([]{});
			test::compare(Tracker::alive, 0);
		}
		test::compare(Tracker::alive, 0);
	});

	app.addTest("TaskFunction/CapturedStateIsMovedNotCopied", [](){
		Tracker::reset();
		{
			auto tracker = Tracker();
			auto task    = TaskFunction<void>([t = std::move(tracker)]{ (void)t; });
			auto moved   = std::move(task);
			std::move(moved).run();
		}
		test::compare(Tracker::copies, 0);
		test::compare(Tracker::alive, 0);
	});

	app.addTest("TaskFunction/DestroysInlineCaptureWithoutRun", [](){
		Tracker::reset();
		{
			auto task = TaskFunction<void>([t = Tracker()]{ (void)t; });
		}
		test::compare(Tracker::alive, 0);
	});

	app.addTest("TaskFunction/DestroysHeapCaptureWithoutRun", [](){
		Tracker::reset();
		{
			auto padding = std::array<std::byte, 64>();
			auto task    = TaskFunction<void>([t = Tracker(), padding]{ (void)t; (void)padding; });
		}
		test::compare(Tracker::alive, 0);
	});

	app.addTest("TaskFunction/ConstraintsRejectInvalidCallables", [](){
		static_assert(std::constructible_from<TaskFunction<int>, int(*)()>);
		static_assert(std::constructible_from<TaskFunction<void>, int(*)()>);
		static_assert(!std::constructible_from<TaskFunction<int>, int>);
		static_assert(!std::constructible_from<TaskFunction<int>, void(*)()>);
	});

	return app.main(argc, argv);
}

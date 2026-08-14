#ifdef LSP_TEST_PROCESS_IMPLEMENTATION

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <thread>
#include <lsp/process.h>

using namespace lsp;

int main(int argc, char** argv)
{
	if(argc < 1) // Should never happen
		return -1;

	if(argc == 1)
		return 0;

	if(std::strcmp(argv[1], "SingleArg") == 0)
	{
		return 1;
	}
	else if(std::strcmp(argv[1], "MultiArg") == 0)
	{
		if(std::strcmp(argv[2], "--first") == 0 &&
		   std::strcmp(argv[3], "second") == 0 &&
		   std::strcmp(argv[4], "foo") == 0 &&
		   std::strcmp(argv[5], "--bar") == 0)
		{
			return 2;
		}
	}
	else if(std::strcmp(argv[1], "EscapedArgs") == 0)
	{
		if(std::strcmp(argv[2], "hello world") == 0 &&
		   std::strcmp(argv[3], "C:\\Program Files\\") == 0 &&
		   std::strcmp(argv[4], "quote\"inside") == 0 &&
		   std::strcmp(argv[5], "") == 0 &&
		   std::strcmp(argv[6], "back\\slash") == 0 &&
		   std::strcmp(argv[7], "\"") == 0 &&
		   std::strcmp(argv[8], "it's") == 0)
		{
			return 3;
		}
	}
	else if(std::strcmp(argv[1], "StdIn") == 0)
	{
		const auto expected   = std::string_view("Hello World!");
		char       buffer[16] = {};

		std::fread(buffer, static_cast<int>(expected.size()), 1, stdin);

		if(buffer == expected)
			return 4;
	}
	else if(std::strcmp(argv[1], "StdOut") == 0)
	{
		std::fprintf(stdout, "Hello World!");
		std::fflush(stdout);
		return 5;
	}
	else if(std::strcmp(argv[1], "StdIO") == 0)
	{
		std::fprintf(stdout, "Hello World!");
		std::fflush(stdout);

		const auto expected   = std::string_view("ping");
		char       buffer[16] = {};

		std::fread(buffer, static_cast<int>(expected.size()), 1, stdin);

		if(buffer == expected)
		{
			std::fprintf(stdout, "pong");
			std::fflush(stdout);
			return 6;
		}
	}
	else if(std::strcmp(argv[1], "ProcessId") == 0)
	{
		int id = -1;
		std::scanf("%d", &id);

		if(Process::currentProcessId() == id)
			return 7;
	}
	else if(std::strcmp(argv[1], "Terminate") == 0)
	{
		for(;;)
			std::this_thread::sleep_for(std::chrono::seconds(2));
	}

	return -1; // Test failure
}

#else

#include <string>
#include <test/test.h>
#include <lsp/process.h>
#include <lsp/io/stream.h>

using namespace lsp;

#ifndef LSP_TEST_PROCESS_EXE
	#define LSP_TEST_PROCESS_EXE "TestProcess"
#endif

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("DefaultConstructed", [](){
		auto proc = Process();

		test::check(!proc.isRunning(), "!isRunning");
		test::compare(proc.id(), -1);
		test::compare(proc.wait(), -1);
		test::expectException<ProcessError>([&proc](){ (void)proc.stdIO(); }, "Process is not running - Cannot get stdio");
		proc.terminate(); // Should be a noop and not throw or crash...
	});

	app.addTest("NoArgs", [](){
		auto proc = Process(LSP_TEST_PROCESS_EXE);
		test::compare(proc.wait(), 0);
	});

	app.addTest("MoveConstructed", [](){
		auto proc     = Process(LSP_TEST_PROCESS_EXE);
		const auto id = proc.id();
		auto proc2    = Process(std::move(proc));

		test::check(id != -1, "validId");
		test::compare(proc.id(), -1);
		test::compare(proc2.id(), id);
		test::compare(proc.wait(), -1);
		test::compare(proc2.wait(), 0);
	});

	app.addTest("MoveAssigned", [](){
		auto       proc  = Process(LSP_TEST_PROCESS_EXE);
		const auto id    = proc.id();
		auto       proc2 = Process();

		proc2 = std::move(proc);
		test::check(id != -1, "validId");
		test::compare(proc.id(), -1);
		test::compare(proc2.id(), id);
		test::compare(proc.wait(), -1);
		test::compare(proc2.wait(), 0);
	});

	app.addTest("SingleArg", [](){
		auto proc = Process(LSP_TEST_PROCESS_EXE, {"SingleArg"});
		test::compare(proc.wait(), 1);
	});

	app.addTest("MultiArg", [](){
		auto proc = Process(LSP_TEST_PROCESS_EXE, {"MultiArg", "--first", "second", "foo", "--bar"});
		test::compare(proc.wait(), 2);
	});

	app.addTest("EscapedArgs", [](){
		auto proc = Process(LSP_TEST_PROCESS_EXE, {
			"EscapedArgs",
			"hello world",
			"C:\\Program Files\\",
			"quote\"inside",
			"",
			"back\\slash",
			"\"",
			"it's"
		});
		test::compare(proc.wait(), 3);
	});

	app.addTest("StdIn", [](){
		auto       proc  = Process(LSP_TEST_PROCESS_EXE, {"StdIn"});
		auto&      stdIO = proc.stdIO();
		const auto msg   = std::string_view("Hello World!");

		stdIO.write(msg.data(), msg.size());
		test::compare(proc.wait(), 4);
	});

	app.addTest("StdOut", [](){
		auto       proc       = Process(LSP_TEST_PROCESS_EXE, {"StdOut"});
		auto&      stdIO      = proc.stdIO();
		const auto expected   = std::string_view("Hello World!");
		char       buffer[16] = {};

		stdIO.read(buffer, expected.size());
		test::compare(buffer, expected);
		test::compare(proc.wait(), 5);
	});

	app.addTest("StdIO", [](){
		auto  proc       = Process(LSP_TEST_PROCESS_EXE, {"StdIO"});
		auto& stdIO      = proc.stdIO();
		auto  expected   = std::string_view("Hello World!");
		char  buffer[16] = {};

		stdIO.read(buffer, expected.size());
		test::compare(buffer, expected);
		stdIO.write("ping", 4);
		expected = "pong";
		buffer[expected.size()] = '\0';
		stdIO.read(buffer, expected.size());
		test::compare(buffer, expected);
		test::compare(proc.wait(), 6);
	});

	app.addTest("ProcessId", [](){
		auto       proc  = Process(LSP_TEST_PROCESS_EXE, {"ProcessId"});
		auto&      stdIO = proc.stdIO();
		const auto idStr = std::to_string(proc.id());

		stdIO.write(idStr.data(), idStr.size());
		test::compare(proc.wait(), 7);
	});

	app.addTest("Terminate", [](){
		auto       proc = Process(LSP_TEST_PROCESS_EXE, {"Terminate"});
		const auto id   = proc.id();

		test::check(proc.isRunning(), "isRunning");
		test::check(id != -1, "validId");
		test::check(Process::exists(id), "processExists");
		proc.terminate();
		test::check(!proc.isRunning(), "!isRunning");
		test::compare(proc.id(), -1);
		test::compare(proc.wait(), -1);
		test::check(!Process::exists(id), "!processExists");
	});

	app.addTest("CurrentProcessIdExists", [](){
		const auto id = Process::currentProcessId();
		test::check(Process::exists(id));
	});

	return app.main(argc, argv);
}

#endif

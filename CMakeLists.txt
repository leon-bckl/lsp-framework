cmake_minimum_required(VERSION 3.10)

project(ucls)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

include_directories(src)

set(HEADERS
	src/json/json.h
	src/jsonrpc/jsonrpc.h
	src/lsp/message.h
	src/lsp/server.h
	src/uscript/name.h
	src/uscript/names.h
	src/uscript/uscript.h
)

set(SOURCES
	src/main.cpp
	src/json/json.cpp
	src/jsonrpc/jsonrpc.cpp
	src/lsp/server.cpp
	src/uscript/name.cpp
	src/uscript/uscript.cpp
)

add_executable(ucls ${SOURCES} ${HEADERS})

cmake_minimum_required(VERSION 3.10)

project(lsp)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

set(HEADERS
	lsp/json.h
	lsp/jsonrpc.h
	lsp/message.h
	lsp/server.h
)

set(SOURCES
	lsp/json.cpp
	lsp/jsonrpc.cpp
	lsp/server.cpp
)

add_library(lsp STATIC)
target_include_directories(lsp PUBLIC .)
target_sources(lsp PRIVATE ${SOURCES})

cmake_minimum_required(VERSION 3.23)

project(lsp)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

set(HEADERS
	src/json/json.h
	src/jsonrpc/jsonrpc.h
	src/lsp/message.h
	src/lsp/server.h
)

set(SOURCES
	src/json/json.cpp
	src/jsonrpc/jsonrpc.cpp
	src/lsp/server.cpp
)

add_library(lsp STATIC)
target_include_directories(lsp PUBLIC src)
target_sources(lsp PUBLIC ${SOURCES})

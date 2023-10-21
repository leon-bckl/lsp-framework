cmake_minimum_required(VERSION 3.10)

project(lsp)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /bigobj")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

set(HEADERS
	# json
	lsp/json/json.h
	# jsonrpc
	lsp/json/jsonrpc.h
	# server
	lsp/server/languageadapter.h.h
	lsp/server/message.h
	lsp/server/server.h
	# util
	lsp/util/util.h
)

set(SOURCES
	# json
	lsp/json/json.cpp
	# jsonrpc
	lsp/jsonrpc/jsonrpc.cpp
	# server
	lsp/server/server.cpp
)

add_library(lsp STATIC)
target_include_directories(lsp PUBLIC .)
target_sources(lsp PRIVATE ${SOURCES})

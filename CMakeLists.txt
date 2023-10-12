cmake_minimum_required(VERSION 3.10)

project(ucls)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(HEADERS
	json/json.h
	jsonrpc/jsonrpc.h
	lsp/message.h
	lsp/server.h
	uscript/name.h
	uscript/names.h
	uscript/uscript.h
)

set(SOURCES
	main.cpp
	json/json.cpp
	jsonrpc/jsonrpc.cpp
	lsp/server.cpp
	uscript/name.cpp
	uscript/uscript.cpp
)

add_executable(ucls ${SOURCES} ${HEADERS})

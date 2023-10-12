cmake_minimum_required(VERSION 3.10)

project(ucls)

set(CMAKE_CXX_STANDARD 20)

message(STATUS "COMPILER_ID: " ${CMAKE_CXX_COMPILER_ID})

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

set(HEADERS
	json.h
	lsp.h
	unreal.h
)

set(SOURCES
	main.cpp
	json.cpp
	unreal.cpp
)

add_executable(ucls ${SOURCES} ${HEADERS})

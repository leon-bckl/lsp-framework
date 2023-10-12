cmake_minimum_required(VERSION 3.10)

project(ucls)

set(CMAKE_CXX_STANDARD 20)

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

cmake_minimum_required(VERSION 3.12)

project(lsp)

option(MESSAGE_TYPES "Enable message types" OFF)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /bigobj")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")
endif()

add_executable(lspgen lspgen/lspgen.cpp lsp/json/json.cpp lsp/util/str.cpp)
target_include_directories(lspgen PRIVATE .)

SET(SRC_DIR lsp/)

set(HEADERS
	# lsp
	connection.h
	error.h
	messagebase.h
	messagehandler.h
	serialization.h
	# io
	io/standardio.h
	# json
	json/json.h
	# jsonrpc
	json/jsonrpc.h
	# util
	util/nullable.h
	util/str.h
	util/uri.h
	util/util.h
)
list(TRANSFORM HEADERS PREPEND ${SRC_DIR})

set(SOURCES
	# lsp
	connection.cpp
	messagehandler.cpp
	# io
	io/standardio.cpp
	# json
	json/json.cpp
	# jsonrpc
	jsonrpc/jsonrpc.cpp
	# util
	util/str.cpp
	util/uri.cpp
)
list(TRANSFORM SOURCES PREPEND ${SRC_DIR})

set(GENERATED_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_FILES_DIR}/lsp)

set(GENERATED_HEADERS
	${GENERATED_FILES_DIR}/lsp/types.h
	${GENERATED_FILES_DIR}/lsp/messages.h
)

set(GENERATED_SOURCES
	${GENERATED_FILES_DIR}/lsp/types.cpp
	${GENERATED_FILES_DIR}/lsp/messages.cpp
)

set(LSP_METAMODEL_JSON ${CMAKE_CURRENT_SOURCE_DIR}/lspgen/lspmetamodel.json)

add_custom_command(
	COMMAND lspgen ${LSP_METAMODEL_JSON}
	WORKING_DIRECTORY ${GENERATED_FILES_DIR}/lsp
	OUTPUT
		${GENERATED_HEADERS}
		${GENERATED_SOURCES}
	DEPENDS
		lspgen
		${LSP_METAMODEL_JSON}
	COMMENT "Generating lsp types from meta model..."
)

add_library(lsp STATIC)
target_include_directories(lsp PUBLIC . ${GENERATED_FILES_DIR})
target_sources(lsp PRIVATE ${SOURCES} ${GENERATED_SOURCES})

if(MESSAGE_TYPES)
	target_compile_definitions(lsp PUBLIC LSP_MESSAGE_TYPES)
endif()

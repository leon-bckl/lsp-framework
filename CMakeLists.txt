cmake_minimum_required(VERSION 3.10)

project(lsp)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /bigobj")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")
endif()

add_executable(lspgen lspgen/lspgen.cpp lsp/json/json.cpp lsp/util/str.cpp)
target_include_directories(lspgen PRIVATE .)

set(GENERATED_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_FILES_DIR}/lsp)

set(HEADERS
	# lsp
	lsp/connection.h
	lsp/error.h
	lsp/messagebase.h
	lsp/messagehandler.h
	lsp/serialization.h
	# io
	lsp/io/standardio.h
	# json
	lsp/json/json.h
	# jsonrpc
	lsp/json/jsonrpc.h
	# util
	lsp/util/nullable.h
	lsp/util/str.h
	lsp/util/uri.h
	lsp/util/util.h
	# generated
	${GENERATED_FILES_DIR}/lsp/types.h
	${GENERATED_FILES_DIR}/lsp/messages.h
)

set(SOURCES
	# lsp
	lsp/connection.cpp
	lsp/messagehandler.cpp
	# io
	lsp/io/standardio.cpp
	# json
	lsp/json/json.cpp
	# jsonrpc
	lsp/jsonrpc/jsonrpc.cpp
	# util
	lsp/util/str.cpp
	lsp/util/uri.cpp
	# generated
	${GENERATED_FILES_DIR}/lsp/types.cpp
	${GENERATED_FILES_DIR}/lsp/messages.cpp
)

set(LSP_METAMODEL_JSON ${CMAKE_CURRENT_SOURCE_DIR}/lspgen/lspmetamodel.json)

add_custom_command(
	COMMAND lspgen ${LSP_METAMODEL_JSON}
	WORKING_DIRECTORY ${GENERATED_FILES_DIR}/lsp
	OUTPUT
		${GENERATED_FILES_DIR}/lsp/types.h
		${GENERATED_FILES_DIR}/lsp/types.cpp
		${GENERATED_FILES_DIR}/lsp/messages.h
		${GENERATED_FILES_DIR}/lsp/messages.cpp
	DEPENDS lspgen ${LSP_METAMODEL_JSON}
	COMMENT "Generating lsp types from meta model..."
)

add_library(lsp STATIC)
target_include_directories(lsp PUBLIC . ${GENERATED_FILES_DIR})
target_sources(lsp PRIVATE ${SOURCES})

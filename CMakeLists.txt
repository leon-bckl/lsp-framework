cmake_minimum_required(VERSION 3.10)

project(lsp)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /bigobj")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")
endif()

add_executable(lspgen lspgen/lspgen.cpp lsp/json/json.cpp)
target_include_directories(lspgen PRIVATE .)

set(GENERATED_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_FILES_DIR}/lsp)

set(HEADERS
	# json
	lsp/json/json.h
	# jsonrpc
	lsp/json/jsonrpc.h
	# server
	lsp/server/connection.h
	lsp/server/languageadapter.h
	lsp/server/server.h
	# util
	lsp/util/util.h
	# generated
	${GENERATED_FILES_DIR}/lsp/types.h
)

set(SOURCES
	# json
	lsp/json/json.cpp
	# jsonrpc
	lsp/jsonrpc/jsonrpc.cpp
	# server
	lsp/server/connection.cpp
	lsp/server/server.cpp
	# generated
	${GENERATED_FILES_DIR}/lsp/types.cpp
)

set(LSP_METAMODEL_JSON ${CMAKE_CURRENT_SOURCE_DIR}/lspgen/lspmetamodel.json)

add_custom_command(
	COMMAND lspgen ${LSP_METAMODEL_JSON}
	WORKING_DIRECTORY ${GENERATED_FILES_DIR}/lsp
	OUTPUT
		${GENERATED_FILES_DIR}/lsp/types.h
		${GENERATED_FILES_DIR}/lsp/types.cpp
	DEPENDS lspgen ${LSP_METAMODEL_JSON}
	COMMENT "Generating lsp types from meta model..."
)

add_library(lsp STATIC)
target_include_directories(lsp PUBLIC . ${GENERATED_FILES_DIR})
target_sources(lsp PRIVATE ${SOURCES})

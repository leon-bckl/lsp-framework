cmake_minimum_required(VERSION 3.12)

project(lsp)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(/W4 /bigobj)
else()
	add_compile_options(-Wall -Wextra -pedantic)
	add_compile_options($<$<CONFIG:DEBUG>:-fsanitize=address,undefined>)
endif()

add_executable(lspgen lspgen/lspgen.cpp lsp/json/json.cpp lsp/str.cpp)
target_include_directories(lspgen PRIVATE .)

SET(SRC_DIR lsp/)

set(HEADERS
	# lsp
	connection.h
	error.h
	fileuri.h
	messagebase.h
	messagedispatcher.h
	messagehandler.h
	nullable.h
	requesthandler.h
	serialization.h
	str.h
	util.h
	# io
	io/standardio.h
	# json
	json/json.h
	# jsonrpc
	jsonrpc/jsonrpc.h
)
list(TRANSFORM HEADERS PREPEND ${SRC_DIR})

set(SOURCES
	# lsp
	connection.cpp
	messagedispatcher.cpp
	requesthandler.cpp
	str.cpp
	fileuri.cpp
	# io
	io/standardio.cpp
	# json
	json/json.cpp
	# jsonrpc
	jsonrpc/jsonrpc.cpp
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
	OUTPUT ${GENERATED_HEADERS} ${GENERATED_SOURCES}
	DEPENDS lspgen ${LSP_METAMODEL_JSON}
	COMMENT "Generating lsp types from meta model..."
)

add_library(lsp STATIC)
target_include_directories(lsp PUBLIC . ${GENERATED_FILES_DIR})
target_sources(lsp PRIVATE ${HEADERS} ${SOURCES} ${GENERATED_HEADERS} ${GENERATED_SOURCES})

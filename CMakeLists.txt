cmake_minimum_required(VERSION 3.12)

project(lsp)

option(LSP_USE_SANITIZERS "Use address and undefined behavior sanitizers" ON)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(/W4 /bigobj)
else()
	add_compile_options(-Wall -Wextra -pedantic)

	if(LSP_USE_SANITIZERS)
		add_compile_options(
			$<$<CONFIG:DEBUG>:-fsanitize=address,undefined>
			$<$<CONFIG:DEBUG>:-fno-omit-frame-pointer>
		)
		add_link_options($<$<CONFIG:DEBUG>:-fsanitize=address,undefined>)
	endif()

	if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options(-Wa,-mbig-obj)
	endif()
endif()

add_executable(lspgen lspgen/lspgen.cpp lsp/json/json.cpp lsp/str.cpp)
target_include_directories(lspgen PRIVATE .)

set(LSP_SRC_DIR lsp/)

set(LSP_HEADERS
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
list(TRANSFORM LSP_HEADERS PREPEND ${LSP_SRC_DIR})

set(LSP_SOURCES
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
list(TRANSFORM LSP_SOURCES PREPEND ${LSP_SRC_DIR})

set(LSP_GENERATED_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${LSP_GENERATED_FILES_DIR}/lsp)

set(LSP_GENERATED_HEADERS
	${LSP_GENERATED_FILES_DIR}/lsp/types.h
	${LSP_GENERATED_FILES_DIR}/lsp/messages.h
)

set(LSP_GENERATED_SOURCES
	${LSP_GENERATED_FILES_DIR}/lsp/types.cpp
	${LSP_GENERATED_FILES_DIR}/lsp/messages.cpp
)

set(LSP_METAMODEL_JSON ${CMAKE_CURRENT_SOURCE_DIR}/lspgen/lspmetamodel.json)

add_custom_command(
	COMMAND lspgen ${LSP_METAMODEL_JSON}
	WORKING_DIRECTORY ${LSP_GENERATED_FILES_DIR}/lsp
	OUTPUT ${LSP_GENERATED_HEADERS} ${LSP_GENERATED_SOURCES}
	DEPENDS lspgen ${LSP_METAMODEL_JSON}
	COMMENT "Generating lsp types from meta model..."
)

add_library(lsp STATIC)
target_include_directories(lsp PUBLIC . ${LSP_GENERATED_FILES_DIR})
target_sources(lsp PRIVATE ${LSP_HEADERS} ${LSP_SOURCES} ${LSP_GENERATED_HEADERS} ${LSP_GENERATED_SOURCES})

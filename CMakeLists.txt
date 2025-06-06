cmake_minimum_required(VERSION 3.12)

project(lsp)

# compiler flags

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(/W4 /bigobj)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	option(LSP_USE_SANITIZERS "Use address and undefined behavior sanitizers for Debug builds" ON)

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

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# lspgen

add_executable(lspgen lspgen/lspgen.cpp lsp/json/json.cpp)
target_include_directories(lspgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# liblsp

set(LSP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lsp/)

set(LSP_HEADERS
	# lsp
	concepts.h
	connection.h
	enumeration.h
	error.h
	exception.h
	fileuri.h
	messagebase.h
	messagehandler.h
	nullable.h
	process.h
	requestresult.h
	serialization.h
	strmap.h
	threadpool.h
	uri.h
	# io
	io/socket.h
	io/standardio.h
	io/stream.h
	# json
	json/json.h
	# jsonrpc
	jsonrpc/jsonrpc.h
)
list(TRANSFORM LSP_HEADERS PREPEND ${LSP_SRC_DIR})

set(LSP_SOURCES
	# lsp
	connection.cpp
	fileuri.cpp
	messagehandler.cpp
	process.cpp
	threadpool.cpp
	uri.cpp
	# io
	io/socket.cpp
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
)

set(LSP_METAMODEL_JSON ${CMAKE_CURRENT_SOURCE_DIR}/lspgen/metaModel.json)

add_custom_command(
	COMMAND lspgen ${LSP_METAMODEL_JSON}
	WORKING_DIRECTORY ${LSP_GENERATED_FILES_DIR}/lsp
	OUTPUT ${LSP_GENERATED_HEADERS} ${LSP_GENERATED_SOURCES}
	DEPENDS lspgen ${LSP_METAMODEL_JSON}
	COMMENT "Generating lsp types from meta model..."
)

add_library(lsp STATIC
	${LSP_HEADERS}
	${LSP_SOURCES}
	${LSP_GENERATED_HEADERS}
	${LSP_GENERATED_SOURCES}
)
target_include_directories(lsp PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${LSP_GENERATED_FILES_DIR}
)

if(WIN32)
	target_link_libraries(lsp PUBLIC Ws2_32)
endif()

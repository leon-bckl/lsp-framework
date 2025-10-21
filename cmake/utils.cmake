# Some utils functions or macros for cmake configuration

# read lsp version from header file
macro(lsp_read_version)
	file(READ "${CMAKE_CURRENT_LIST_DIR}/lsp/version.h" file_contents)
	string(REGEX MATCH "LSP_VER_MAJOR ([0-9]+)" _ "${file_contents}")
	if(NOT CMAKE_MATCH_COUNT EQUAL 1)
		message(FATAL_ERROR "Failed to read major verison from version.h")
	endif()
	set(LSP_VERSION_MAJOR ${CMAKE_MATCH_1})

	string(REGEX MATCH "LSP_VER_MINOR ([0-9]+)" _ "${file_contents}")
	if(NOT CMAKE_MATCH_COUNT EQUAL 1)
		message(FATAL_ERROR "Failed to read minor verison from version.h")
	endif()
	set(LSP_VERSION_MINOR ${CMAKE_MATCH_1})

	string(REGEX MATCH "LSP_VER_PATCH ([0-9]+)" _ "${file_contents}")
	if(NOT CMAKE_MATCH_COUNT EQUAL 1)
		message(FATAL_ERROR "Failed to read patch verison from version.h")
	endif()
	set(LSP_VERSION_PATCH ${CMAKE_MATCH_1})

	set(LSP_VERSION "${LSP_VERSION_MAJOR}.${LSP_VERSION_MINOR}.${LSP_VERSION_PATCH}")
endmacro()
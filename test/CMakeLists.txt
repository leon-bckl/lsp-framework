include_directories(./testtest)

function(lsp_add_test name)
	add_executable(test_${name})
	target_sources(test_${name} PRIVATE test_${name}.cpp)
	target_link_libraries(test_${name} PRIVATE lsp)
	add_test(NAME test_${name} COMMAND test_${name})
endfunction()

add_subdirectory(lsp)

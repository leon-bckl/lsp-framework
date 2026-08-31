# Server
add_executable(example_server ${LSP_DIR}/examples/example_server.cpp)
target_link_libraries(example_server lsp)

# Client
add_executable(example_client ${LSP_DIR}/examples/example_client.cpp)
target_link_libraries(example_client lsp)

cmake_minimum_required(VERSION 3.15)

project(sv_explorer)

# Note: this example just pulls the head master branch
# for slang, but a real project would very likely pin
# this to a specific tag and only update when ready
# to accept a new version.
add_link_options(-fuse-ld=mold)
# Use sockpp as a static lib
option(SOCKPP_BUILD_SHARED "" OFF)
option(SOCKPP_BUILD_STATIC "" ON)

# As workaround
option(SLANG_USE_MIMALLOC ""  ON)

# Add all third parties 
include(third-party.cmake)
include(lsp-server/include/types/generated_headers.cmake)
list(TRANSFORM SVLS_GEN_HEADERS PREPEND lsp-server/include/types/)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# add_subdirectory(lib-diplomat-index)

add_executable(sv-blackbox main_blackbox.cpp
documenter_visitor.h
documenter_visitor.cpp)

target_link_libraries(sv-blackbox 
PUBLIC slang::slang
PRIVATE nlohmann_json::nlohmann_json)


add_executable(slang-lsp 
lsp-server/main_lsp.cpp

lsp-server/rpc/tcp_interface_server.cpp
lsp-server/rpc/rpc_transport.cpp
lsp-server/lsp/lsp.cpp
lsp-server/lsp/lsp_errors.cpp
lsp-server/lsp/lsp_default_binds.cpp
lsp-server/include/types/src/structs_json_bindings.cpp
# lsp-server/include/types/src/enums_json_bindings.cpp

lsp-server/diplomat/src/diplomat_lsp.cpp
lsp-server/diplomat/src/diplomat_lsp_ws_settings.cpp
lsp-server/diplomat/src/sv_document.cpp
lsp-server/diplomat/src/visitor_module_bb.cpp
lsp-server/diplomat/src/diagnostic_client.cpp
lsp-server/diplomat/src/diplomat_index.cpp

)

target_precompile_headers(slang-lsp PRIVATE ${SVLS_GEN_HEADERS})

target_include_directories(slang-lsp 
PRIVATE lsp-server/include
PRIVATE lsp-server/diplomat/include)

target_link_libraries(slang-lsp 
PRIVATE argparse::argparse
PRIVATE stduuid
PRIVATE fmt::fmt
PRIVATE sockpp-static
PRIVATE spdlog::spdlog
PRIVATE nlohmann_json::nlohmann_json
PUBLIC slang::slang
)

add_executable(lsp-test-client 
lsp-server/main_demo_client.cpp
lsp-server/rpc/tcp_interface_server.cpp
)

target_link_libraries(lsp-test-client
PRIVATE argparse::argparse
PRIVATE sockpp-static
PRIVATE spdlog::spdlog
PRIVATE nlohmann_json::nlohmann_json
)

target_include_directories(lsp-test-client 
PRIVATE lsp-server/include)

add_executable(sv-explorer
explorer/main.cpp
explorer/hier_visitor.cpp
explorer/explorer_visitor.cpp)

target_link_libraries(sv-explorer
PRIVATE nlohmann_json::nlohmann_json
PRIVATE slang::slang
PRIVATE libdiplomatindex)

target_include_directories(sv-explorer 
PRIVATE explorer/)

add_executable(sv-formatter
formatter/sv_formatter.cpp
formatter/main_formatter.cpp)

target_link_libraries(sv-formatter
PRIVATE argparse::argparse
PRIVATE fmt::fmt
PRIVATE spdlog::spdlog
PRIVATE slang::slang
)

target_include_directories(sv-formatter 
PRIVATE formatter/)
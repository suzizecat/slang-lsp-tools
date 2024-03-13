cmake_minimum_required(VERSION 3.15)

project(sv_explorer)

option(DIPLOMAT_USE_MOLD "Use mold linker" OFF)
if(${DIPLOMAT_USE_MOLD})
    message(STATUS "Use mold as a linker")    
    add_link_options(-fuse-ld=mold)
endif()

# Use sockpp as a static lib
option(SOCKPP_BUILD_SHARED "" OFF)
option(SOCKPP_BUILD_STATIC "" ON)

# For SPDLOG
option(SPDLOG_ENABLE_PCH "" ON)
option(SPDLOG_FMT_EXTERNAL "" ON)


# As workaround
option(SLANG_USE_MIMALLOC ""  ON)

# Add all third parties 
include(third-party.cmake)

set(LSP_METAMODEL_PATH ${CMAKE_CURRENT_BINARY_DIR}/metaModel.json)
set(LSP_DATASTRUCT_ROOT ${CMAKE_SOURCE_DIR}/lsp-server/include/types)
set(LSP_DATASTRUCT_CMAKE ${LSP_DATASTRUCT_ROOT}/generated_headers.cmake)

if(NOT EXISTS ${LSP_DATASTRUCT_CMAKE} OR NOT EXISTS ${LSP_METAMODEL_PATH})
    
    message(STATUS "Regenerating LSP data structures using " ${LSP_METAMODEL_PATH})
    file(
        DOWNLOAD https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/metaModel/metaModel.json 
        ${LSP_METAMODEL_PATH}
        )

    execute_process(COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/LSPGen/main.py -fo ${LSP_DATASTRUCT_ROOT} ${LSP_METAMODEL_PATH})
    message(STATUS "Regeneration done. ")

endif()

include(${LSP_DATASTRUCT_CMAKE})
list(TRANSFORM SVLS_GEN_HEADERS PREPEND ${LSP_DATASTRUCT_ROOT}/)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# add_subdirectory(lib-diplomat-index)
###########################################################
# SV-FORMATTER 
###########################################################
message(STATUS "Setup SV-Formatter library and executable ")
add_library(sv-formatter-lib STATIC 
    formatter/format_DataDeclaration.cpp
    formatter/spacing_manager.cpp
)

target_link_libraries(sv-formatter-lib PRIVATE slang::slang)
target_include_directories(sv-formatter-lib INTERFACE formatter/)

add_executable(sv-formatter-exe
formatter/main_formatter.cpp
)

target_link_libraries(sv-formatter-exe
PRIVATE argparse::argparse
PRIVATE fmt::fmt
PRIVATE spdlog::spdlog

PRIVATE slang::slang
PRIVATE sv-formatter-lib
)

target_include_directories(sv-formatter-exe
PRIVATE $<TARGET_PROPERTY:sv-formatter-lib,INTERFACE_INCLUDE_DIRECTORIES>)

set_property(TARGET sv-formatter-exe PROPERTY OUTPUT_NAME sv-formatter)
set_property(TARGET sv-formatter-lib PROPERTY OUTPUT_NAME sv-formatter)

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
lsp-server/diplomat/src/diplomat_lsp_binds.cpp
lsp-server/diplomat/src/diplomat_lsp_ws_settings.cpp
lsp-server/diplomat/src/sv_document.cpp
lsp-server/diplomat/src/visitor_module_bb.cpp
lsp-server/diplomat/src/visitor_index.cpp
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
PRIVATE sv-formatter-lib
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
PRIVATE slang::slang)

target_include_directories(sv-explorer 
PRIVATE explorer/)


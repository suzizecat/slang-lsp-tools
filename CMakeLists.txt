cmake_minimum_required(VERSION 3.15)

project(sv_explorer)

option(DIPLOMAT_USE_MOLD "Use mold linker" OFF)
set(DIPLOMAT_VERSION "auto" CACHE STRING "Version string to be outputed on --version call") 


if(${DIPLOMAT_USE_MOLD})
    message(STATUS "Use mold as a linker")    
    add_link_options(-fuse-ld=mold)
endif()

set(VERSION_STRING "undefined")

if("${DIPLOMAT_VERSION}" STREQUAL "auto")
	execute_process(COMMAND git describe --tags RESULT_VARIABLE GIT_OK OUTPUT_VARIABLE VERSION_STRING OUTPUT_STRIP_TRAILING_WHITESPACE)
	
	# message(STATUS "Git returned ${GIT_OK} ${VERSION_STRING}")
	if ("${GIT_OK}" EQUAL "0")

		message(STATUS "Use automatic version string from GIT")
		set(VERSION_STRING "${VERSION_STRING}-custom")
	else()
		message(WARNING "Failed to retrieve version using git. Use -DDIPLOMAT_VERSION to manually set a version string")
		set(VERSION_STRING "undefined")
	endif()
else()
	message(STATUS "Using manually provided version string")
endif()

message(STATUS "Software version: " ${VERSION_STRING})

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

utils/ast-printer/ast_print.cpp

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
PRIVATE $<TARGET_PROPERTY:sv-formatter-lib,INTERFACE_INCLUDE_DIRECTORIES>
PRIVATE utils/ast-printer)

set_property(TARGET sv-formatter-exe PROPERTY OUTPUT_NAME sv-formatter)
set_property(TARGET sv-formatter-lib PROPERTY OUTPUT_NAME sv-formatter)

message(STATUS "Setup SV-Explorer library and executable ")


add_library(sv-explorer-lib STATIC 
explorer/hier_visitor.cpp
)

target_link_libraries(sv-explorer-lib 
PRIVATE slang::slang
PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(sv-explorer-lib INTERFACE explorer/)


add_executable(sv-explorer-exe
explorer/main.cpp
explorer/explorer_visitor.cpp
)

target_link_libraries(sv-explorer-exe
PRIVATE nlohmann_json::nlohmann_json
PRIVATE slang::slang
PRIVATE sv-explorer-lib
)


target_include_directories(sv-explorer-exe
PRIVATE $<TARGET_PROPERTY:sv-explorer-lib,INTERFACE_INCLUDE_DIRECTORIES>
PRIVATE utils/ast_print)

set_property(TARGET sv-explorer-exe PROPERTY OUTPUT_NAME sv-explorer)
set_property(TARGET sv-explorer-lib PROPERTY OUTPUT_NAME sv-explorer)

######################################################################
######################################################################
######################################################################


# target_link_libraries(sv-explorer
# PRIVATE nlohmann_json::nlohmann_json
# PRIVATE slang::slang)

# target_include_directories(sv-explorer 
# PRIVATE explorer/)

######################################################################
######################################################################
######################################################################

message(STATUS "Setup SV-LSP executable and other stuff")


# add_library(diplomat-system-lib STATIC 
# system/src/process_handler_lnx.cpp
# )

# target_include_directories(diplomat-system-lib PRIVATE system/include)


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

target_compile_definitions(slang-lsp PUBLIC DIPLOMAT_VERSION_STRING=\"${VERSION_STRING}\")

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
PRIVATE sv-explorer-lib
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


add_executable(sv-tree
utils/ast-printer/ast_print.cpp)

target_link_libraries(sv-tree
PRIVATE slang::slang
PRIVATE argparse::argparse)

# target_include_directories(sv-explorer 
# PRIVATE utils/ast_print)

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
	execute_process(COMMAND git describe --tags --abbrev=0 RESULT_VARIABLE GIT_OK OUTPUT_VARIABLE VERSION_STRING_BASE OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND git describe --tags --dirty RESULT_VARIABLE GIT_OK OUTPUT_VARIABLE GIT_VERSION_FULL OUTPUT_STRIP_TRAILING_WHITESPACE)
	string(REPLACE "${VERSION_STRING_BASE}-" "${VERSION_STRING_BASE}+" VERSION_STRING ${GIT_VERSION_FULL})
	
	# message(STATUS "Git returned ${GIT_OK} ${VERSION_STRING}")
	if ("${GIT_OK}" EQUAL "0")

		message(STATUS "Use automatic version string from GIT")
		set(VERSION_STRING "${VERSION_STRING}.custom")
	else()
		message(WARNING "Failed to retrieve version using git. Use -DDIPLOMAT_VERSION to manually set a version string")
		set(VERSION_STRING "undefined")
	endif()
else()
	message(STATUS "Using manually provided version string")
    set(VERSION_STRING ${DIPLOMAT_VERSION})
endif()

message(STATUS "Diplomat software version: " ${VERSION_STRING})

add_compile_options("$<$<CONFIG:DEBUG>:-DDIPLOMAT_DEBUG>")
# add_compile_options("-DDIPLOMAT_VERSION_STRING=\"${VERSION_STRING}\"")


# Use sockpp as a static lib
option(SOCKPP_BUILD_SHARED "" OFF)
option(SOCKPP_BUILD_STATIC "" ON)

# For SPDLOG
option(SPDLOG_ENABLE_PCH "" ON)
option(SPDLOG_FMT_EXTERNAL "" ON)


# As workaround
option(SLANG_USE_MIMALLOC ""  ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Add all third parties 
include(cmake/third-party.cmake)
include(cmake/create-component.cmake)
include(cmake/lsp-datastructures.cmake)


# add_subdirectory(lib-diplomat-index)
###########################################################
# SV-FORMATTER 
###########################################################
include(cmake/sv-tree.cmake)
include(cmake/sv-formatter.cmake)
include(cmake/sv-explorer.cmake)
include(cmake/sv-indexer.cmake)
 

######################################################################
######################################################################
######################################################################


message(STATUS "Setup SV-LSP executable and other stuff")


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
lsp-server/diplomat/src/diplomat_document_cache.cpp
#lsp-server/diplomat/src/sv_document.cpp
lsp-server/diplomat/src/visitor_module_bb.cpp
lsp-server/diplomat/src/hier_visitor.cpp
#lsp-server/diplomat/src/visitor_index.cpp
lsp-server/diplomat/src/diagnostic_client.cpp
#lsp-server/diplomat/src/diplomat_index.cpp
)


target_compile_definitions(slang-lsp PRIVATE DIPLOMAT_VERSION_STRING=\"${VERSION_STRING}\")

target_precompile_headers(slang-lsp PRIVATE ${SVLS_GEN_HEADERS})

target_include_directories(slang-lsp 
PRIVATE lsp-server/include
PRIVATE lsp-server/diplomat/include
#PRIVATE $<TARGET_PROPERTY:sv-indexer-lib,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(slang-lsp 
PRIVATE argparse::argparse
PRIVATE stduuid
PRIVATE fmt::fmt
PRIVATE sockpp-static
PRIVATE spdlog::spdlog
PRIVATE nlohmann_json::nlohmann_json
PRIVATE sv-formatter-lib
# PRIVATE sv-explorer-lib
PRIVATE sv-indexer-lib
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


# add_executable(sv-tree
# utils/ast-printer/ast_print.cpp
# utils/ast-printer/ast_print_main.cpp
# )

# target_include_directories(sv-tree 
# PRIVATE utils/ast-printer
# )

# target_link_libraries(sv-tree
# PUBLIC slang::slang
# PRIVATE argparse::argparse)

add_custom_target(full 
DEPENDS 
	sv-formatter-exe
	sv-indexer-exe
	# sv-tree-exe
	sv-explorer-exe
	slang-lsp)

# target_include_directories(sv-explorer 
# PRIVATE utils/ast_print)

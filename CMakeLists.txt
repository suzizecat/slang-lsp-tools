cmake_minimum_required(VERSION 3.15)

project(sv_explorer)

# Note: this example just pulls the head master branch
# for slang, but a real project would very likely pin
# this to a specific tag and only update when ready
# to accept a new version.

# Add all third parties 
include(third-party.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${rpc_SOURCE_DIR}/include)

add_executable(sv-blackbox main_blackbox.cpp
documenter_visitor.h
documenter_visitor.cpp)

target_link_libraries(sv-blackbox 
PRIVATE slang::slang
PRIVATE nlohmann_json::nlohmann_json)



add_executable(slang-lsp 
lsp-server/main_lsp.cpp
lsp-server/include/rpc_transport.hpp
lsp-server/rpc_transport.cpp
lsp-server/include/lsp_initialization.hpp
lsp-server/include/lsp.hpp
lsp-server/lsp/lsp.cpp

)

target_include_directories(slang-lsp PRIVATE lsp-server/include)

target_link_libraries(slang-lsp 
PRIVATE argparse::argparse
PRIVATE spdlog::spdlog
PRIVATE nlohmann_json::nlohmann_json
PRIVATE httplib::httplib
PRIVATE slang::slang
# PRIVATE json-rpc-cxx
)


# add_executable(rpc-test rpc-test/warehouse/main.cpp
# rpc-test/inmemoryconnector.hpp
# rpc-test/cpphttplibconnector.hpp
# rpc-test/warehouse/warehouseapp.hpp
# rpc-test/warehouse/types.h
# rpc-test/warehouse/warehouseapp.cpp
# )
# target_include_directories(rpc-test PRIVATE rpc-test)

# target_link_libraries(rpc-test 
# PRIVATE httplib::httplib
# PRIVATE nlohmann_json::nlohmann_json
# PRIVATE json-rpc-cxx)




include(FetchContent)

# Base library, might be used as dependencies for others


FetchContent_Declare(fmt
GIT_REPOSITORY https://github.com/fmtlib/fmt.git
GIT_TAG 9.1.0 # For correct management with slang
GIT_SHALLOW ON)

FetchContent_Declare(httplib
GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
GIT_TAG v0.13.3
GIT_SHALLOW ON)

FetchContent_Declare(spdlog
GIT_REPOSITORY https://github.com/gabime/spdlog.git
GIT_TAG v1.12.0
GIT_SHALLOW ON)

FetchContent_Declare(json 
GIT_REPOSITORY git@github.com:nlohmann/json.git
GIT_TAG v3.11.2
GIT_SHALLOW ON)

FetchContent_Declare(uri 
GIT_REPOSITORY https://github.com/ben-zen/uri-library.git
GIT_TAG 23690f8
GIT_SHALLOW ON)


# Core feature providers
FetchContent_Declare( slang
  GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
  GIT_TAG v3.0
  GIT_SHALLOW ON)

FetchContent_Declare(rpc
GIT_REPOSITORY https://github.com/jsonrpcx/json-rpc-cxx.git
GIT_TAG v0.3.1
GIT_SHALLOW ON)

FetchContent_Declare(argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v2.9
  GIT_SHALLOW ON)

FetchContent_Populate(uri)
include_directories(${uri_SOURCE_DIR})

FetchContent_MakeAvailable(fmt)  
FetchContent_MakeAvailable(json spdlog httplib)
FetchContent_MakeAvailable(argparse slang)
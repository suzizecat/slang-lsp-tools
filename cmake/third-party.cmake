include(FetchContent)

# Base library, might be used as dependencies for others


FetchContent_Declare(fmt
GIT_REPOSITORY https://github.com/fmtlib/fmt.git
#GIT_TAG 10.2.1 # For correct management with slang
GIT_TAG 11.0.2
GIT_SHALLOW ON)

FetchContent_Declare(network
GIT_REPOSITORY https://github.com/fpagliughi/sockpp.git
GIT_TAG v1.0.0
GIT_SHALLOW ON)

FetchContent_Declare(spdlog
GIT_REPOSITORY https://github.com/gabime/spdlog.git
GIT_TAG v1.14.1
GIT_SHALLOW ON)

FetchContent_Declare(json 
GIT_REPOSITORY https://github.com/nlohmann/json.git
GIT_TAG v3.11.3
GIT_SHALLOW ON)

FetchContent_Declare(uri 
GIT_REPOSITORY https://github.com/ben-zen/uri-library.git
GIT_TAG 6b1b15a
GIT_SHALLOW OFF)

FetchContent_Declare(uuid 
GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
GIT_TAG v1.2.3
GIT_SHALLOW ON)

# Core feature providers
FetchContent_Declare( slang
  GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
  GIT_TAG v7.0
  GIT_SHALLOW ON)


FetchContent_Declare(argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v3.1
  GIT_SHALLOW ON)

FetchContent_MakeAvailable(uri)

# FetchContent_MakeAvailable(pgm-index)

FetchContent_MakeAvailable(fmt network uuid)  
FetchContent_MakeAvailable(json spdlog)
FetchContent_MakeAvailable(argparse slang)

include_directories($<BUILD_INTERFACE:${uri_SOURCE_DIR}>)

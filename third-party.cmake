include(FetchContent)

# Base library, might be used as dependencies for others


FetchContent_Declare(fmt
GIT_REPOSITORY https://github.com/fmtlib/fmt.git
GIT_TAG 10.2.1 # For correct management with slang
GIT_SHALLOW ON)

FetchContent_Declare(network
GIT_REPOSITORY https://github.com/fpagliughi/sockpp.git
GIT_TAG v0.8.1
GIT_SHALLOW ON)

FetchContent_Declare(spdlog
GIT_REPOSITORY https://github.com/gabime/spdlog.git
GIT_TAG v1.13.0
GIT_SHALLOW ON)

FetchContent_Declare(json 
GIT_REPOSITORY https://github.com/nlohmann/json.git
GIT_TAG v3.11.2
GIT_SHALLOW ON)

FetchContent_Declare(uri 
GIT_REPOSITORY https://github.com/ben-zen/uri-library.git
GIT_TAG 23690f8
GIT_SHALLOW ON)

FetchContent_Declare(uuid 
GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
GIT_TAG v1.2.3
GIT_SHALLOW ON)


FetchContent_Declare(pgm-index 
GIT_REPOSITORY https://github.com/gvinciguerra/PGM-index.git
GIT_TAG master
GIT_SHALLOW ON)


# Core feature providers
FetchContent_Declare( slang
  GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
  #GIT_TAG v4.0
  GIT_TAG 66697a085436d9bf5b609cda8944b27074aa5290
  GIT_SHALLOW ON)


FetchContent_Declare(argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v3.0
  GIT_SHALLOW ON)

FetchContent_Populate(uri)

# FetchContent_MakeAvailable(pgm-index)

FetchContent_MakeAvailable(fmt network uuid)  
FetchContent_MakeAvailable(json spdlog)
FetchContent_MakeAvailable(argparse slang)

include_directories($<BUILD_INTERFACE:${uri_SOURCE_DIR}>)
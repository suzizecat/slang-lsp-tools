cmake_minimum_required(VERSION 3.15)
project(lib-diplomat-index)

# Remove unused PGM targets
option(BUILD_EXAMPLES "Build the examples" OFF)
option(BUILD_PGM_TUNER "Build the tuner target" OFF)
option(BUILD_PGM_BENCHMARK "Build the benchmark target" OFF)


include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if (COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif ()

FetchContent_Declare(pgm 
GIT_REPOSITORY https://github.com/gvinciguerra/PGM-index.git
GIT_TAG master
GIT_SHALLOW ON)

FetchContent_MakeAvailable(pgm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_link_options(-fuse-ld=mold)

set(LIB_TARGET libdiplomatindex)

add_library(${LIB_TARGET} STATIC
src/diplomat_index.cpp
)

message(STATUS "PGM source dir is " ${pgm_SOURCE_DIR}/include )
target_link_libraries(${LIB_TARGET} PUBLIC pgmindexlib )

target_include_directories(${LIB_TARGET} PUBLIC include
#PUBLIC $<BUILD_INTERFACE:${pgm_SOURCE_DIR}>/include
PUBLIC ${pgm_SOURCE_DIR}/include
)

add_executable(demo-index src/demo.cpp)
target_link_libraries(demo-index  PUBLIC pgmindexlib )


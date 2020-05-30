cmake_minimum_required(VERSION 3.10)
project(libcast C CXX)

option(ENABLE_TRACE_LOGGING "Enable Trace Logging" ON)

if (ENABLE_TRACE_LOGGING)
message("Tracing is ENABLED")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

### Dependencies
# Protobuf
INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)

include(FindPkgConfig)

# JSONCPP
pkg_check_modules(JSONCPP jsoncpp REQUIRED)
include_directories(${JSONCPP_INCLUDE_DIRS})

# zlib
pkg_check_modules(ZLIB zlib REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

# OpenSSL
#pkg_check_modules(OPENSSL openssl REQUIRED)
#include_directories(${OPENSSL_INCLUDE_DIRS})

# Boring ssl
# cloned using git submodule
# $ cd third_party/boringssl
# $ git submodule add https://boringssl.googlesource.com/boringssl.git src
# $ git checkout 78987bb7bb4764ca3a8b08b0a6f7bd14b53c3e4f (need to figure out how to do this)

# abseil
# cloned using git submodule
# $ cd third_party/
# $ git submodule add https://github.com/abseil/abseil-cpp.git src

include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/third_party/abseil/src")
include_directories("${PROJECT_SOURCE_DIR}/third_party/boringssl/src/include")
include_directories(${PROTOBUF_INCLUDE_DIRS})

add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/boringssl")
add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/abseil/src")
add_subdirectory(${PROJECT_SOURCE_DIR}/platform)
add_subdirectory(${PROJECT_SOURCE_DIR}/util)
add_subdirectory(${PROJECT_SOURCE_DIR}/cast)
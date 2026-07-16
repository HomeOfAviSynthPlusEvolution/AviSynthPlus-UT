cmake_minimum_required(VERSION 3.24)
project(AviSynthPlusUT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AVSUT_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
option(AVSUT_ENABLE_COVERAGE "Enable GCC gcov coverage instrumentation" OFF)

if(AVSUT_ENABLE_SANITIZERS AND AVSUT_ENABLE_COVERAGE)
  message(FATAL_ERROR
    "AVSUT_ENABLE_SANITIZERS and AVSUT_ENABLE_COVERAGE require separate builds")
endif()

if(AVSUT_ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

if(AVSUT_ENABLE_COVERAGE)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "AVSUT_ENABLE_COVERAGE requires GCC")
  endif()
  add_compile_options(-O0 -g --coverage)
  add_link_options(--coverage)
endif()

include(FetchContent)
include(CTest)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  xxhash
  GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
  GIT_TAG v0.8.3
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest xxhash)

include(cmake/UpstreamTargets.cmake)
if(AVSUT_ENABLE_COVERAGE)
  add_subdirectory(tests/coverage)
endif()
add_subdirectory(tests)

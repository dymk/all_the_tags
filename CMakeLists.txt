cmake_minimum_required(VERSION 2.6)
project(all_the_tags CXX)

set(SANITIZE_FLAGS_Q
    "-g -fno-omit-frame-pointer")
set(SANITIZE_FLAGS_NQ
    -g -fno-omit-frame-pointer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(ASMJIT_STATIC TRUE)
add_subdirectory(asmjit)
target_link_libraries(asmjit)

include_directories(src)
include_directories(asmjit/src)
add_library(all_the_tags STATIC
    src/all_the_tags/context.cc src/all_the_tags/query.cc src/all_the_tags/tag.cc)
target_compile_options(all_the_tags PRIVATE "${SANITIZE_FLAGS_NQ}")

# target_compile_features(all_the_tags PUBLIC
#     cxx_auto_type cxx_lambdas)

set(TEST true)
add_subdirectory(test/hayai)
find_package(hayai REQUIRED)

include_directories(test)
include_directories(test/hayai/src)
include_directories(test/hayai/vendor/gtest/include)
add_executable(all_the_tags_testrunner
    test/runner.cc
    test/test_helper.cc
    test/bench_query.cc
    test/test_entitity_tagging.cc
    test/test_query.cc
    test/test_tag_implication.cc
    test/test_tag_values.cc)
target_link_libraries(all_the_tags_testrunner all_the_tags asmjit gtest)
target_compile_options(all_the_tags_testrunner PRIVATE "${SANITIZE_FLAGS_NQ}")

add_executable(all_the_tags_fuzzer
    test/fuzzer.cc)
target_link_libraries(all_the_tags_fuzzer all_the_tags asmjit)
target_compile_options(all_the_tags_fuzzer PRIVATE "${SANITIZE_FLAGS_NQ}")
set_target_properties(
    all_the_tags_testrunner
    all_the_tags_fuzzer
PROPERTIES
    LINK_FLAGS "${SANITIZE_FLAGS_Q}")

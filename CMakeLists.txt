cmake_minimum_required(VERSION 2.6)
project(all_the_tags CXX)

set(ASMJIT_STATIC TRUE)
add_subdirectory(asmjit)
target_link_libraries(asmjit)

include_directories(.)
include_directories(asmjit/src)
add_library(all_the_tags STATIC
    context.cc query.cc tag.cc)

target_compile_features(all_the_tags PUBLIC
    cxx_auto_type cxx_lambdas)

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

add_executable(all_the_tags_fuzzer
    test/fuzzer.cc)
target_link_libraries(all_the_tags_fuzzer all_the_tags asmjit)

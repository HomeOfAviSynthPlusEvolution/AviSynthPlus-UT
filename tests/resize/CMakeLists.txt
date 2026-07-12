add_executable(resize_tests resize_tests.cpp)
target_include_directories(resize_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/tests
  ${PROJECT_SOURCE_DIR}/third_party/AviSynthPlus/avs_core/include
  ${PROJECT_SOURCE_DIR}/third_party/AviSynthPlus/avs_core
  ${PROJECT_BINARY_DIR}/upstream-build/avs_core
  ${xxhash_SOURCE_DIR}
)
# AvsCore enables this member set for the supported 64-bit GCC/Clang builds.
target_compile_definitions(resize_tests PRIVATE INTEL_INTRINSICS_AVX512)
target_link_libraries(resize_tests PRIVATE
  GTest::gtest_main
  AvsCoreExternal
)

include(GoogleTest)
gtest_discover_tests(resize_tests TEST_PREFIX "resize.")

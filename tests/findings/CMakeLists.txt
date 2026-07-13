add_executable(finding_tests
  finding_b1_convert_tests.cpp
  finding_b2_convert_tests.cpp
)
target_include_directories(finding_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/tests
  ${PROJECT_SOURCE_DIR}/third_party/AviSynthPlus/avs_core/include
  ${PROJECT_SOURCE_DIR}/third_party/AviSynthPlus/avs_core
  ${PROJECT_BINARY_DIR}/upstream-build/avs_core
)
target_compile_definitions(finding_tests PRIVATE INTEL_INTRINSICS)
target_link_libraries(finding_tests PRIVATE
  GTest::gtest_main
  AvsCoreExternal
)

include(GoogleTest)
gtest_discover_tests(finding_tests TEST_PREFIX "finding.")

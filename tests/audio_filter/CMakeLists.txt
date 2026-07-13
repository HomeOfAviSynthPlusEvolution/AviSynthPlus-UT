add_executable(audio_filter_tests audio_filter_tests.cpp)
target_compile_definitions(audio_filter_tests PRIVATE INTEL_INTRINSICS)
target_include_directories(audio_filter_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/tests
  ${PROJECT_SOURCE_DIR}/third_party/AviSynthPlus/avs_core/include
  ${PROJECT_SOURCE_DIR}/third_party/AviSynthPlus/avs_core
  ${PROJECT_BINARY_DIR}/upstream-build/avs_core
)
target_link_libraries(audio_filter_tests PRIVATE
  GTest::gtest_main
  AvsCoreExternal
)

include(GoogleTest)
gtest_discover_tests(audio_filter_tests TEST_PREFIX "audio_filter.")

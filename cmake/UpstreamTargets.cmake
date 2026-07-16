include(ExternalProject)
find_package(Threads REQUIRED)

set(AVS_UPSTREAM_BINARY_DIR "${PROJECT_BINARY_DIR}/upstream-build")
set(AVS_UPSTREAM_LIBRARY_NAME
  "${CMAKE_STATIC_LIBRARY_PREFIX}avisynth${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(CMAKE_CONFIGURATION_TYPES)
  set(AVS_UPSTREAM_LIBRARY
    "${AVS_UPSTREAM_BINARY_DIR}/avs_core/$<CONFIG>/${AVS_UPSTREAM_LIBRARY_NAME}")
else()
  set(AVS_UPSTREAM_LIBRARY
    "${AVS_UPSTREAM_BINARY_DIR}/avs_core/${AVS_UPSTREAM_LIBRARY_NAME}")
endif()
file(MAKE_DIRECTORY "${AVS_UPSTREAM_BINARY_DIR}/avs_core")

set(AVS_UPSTREAM_CMAKE_ARGS
  -DBUILD_SHARED_LIBS=OFF
  -DHEADERS_ONLY=OFF
  -DENABLE_PLUGINS=OFF
  -DENABLE_CUDA=OFF
  -DENABLE_INTEL_SIMD=ON
)
if(AVSUT_ENABLE_COVERAGE)
  list(APPEND AVS_UPSTREAM_CMAKE_ARGS
    "-DCMAKE_C_FLAGS:STRING=-O0 -g --coverage"
    "-DCMAKE_CXX_FLAGS:STRING=-O0 -g --coverage"
  )
endif()
if(CMAKE_C_COMPILER)
  list(APPEND AVS_UPSTREAM_CMAKE_ARGS
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  )
endif()
if(CMAKE_CXX_COMPILER)
  list(APPEND AVS_UPSTREAM_CMAKE_ARGS
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  )
endif()
if(MSVC AND WIN32)
  # The external Visual Studio build compiles multiple files into one PDB.
  list(APPEND AVS_UPSTREAM_CMAKE_ARGS
    -DCMAKE_CXX_FLAGS=/FS
  )
endif()
if(NOT CMAKE_CONFIGURATION_TYPES)
  list(APPEND AVS_UPSTREAM_CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  )
endif()

ExternalProject_Add(avisynth_upstream
  SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/AviSynthPlus"
  BINARY_DIR "${AVS_UPSTREAM_BINARY_DIR}"
  INSTALL_COMMAND ""
  CMAKE_ARGS ${AVS_UPSTREAM_CMAKE_ARGS}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target AvsCore --config $<CONFIG>
  BUILD_BYPRODUCTS "${AVS_UPSTREAM_LIBRARY}"
  USES_TERMINAL_CONFIGURE TRUE
  USES_TERMINAL_BUILD TRUE
)

if(MSVC AND WIN32 AND CMAKE_GENERATOR MATCHES "Visual Studio")
  # The static upstream build does not produce AviSynth.exp, but its
  # MSVC-only post-build copy step expects the file to exist.
  ExternalProject_Add_Step(avisynth_upstream prepare_fake_exp
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "${AVS_UPSTREAM_BINARY_DIR}/avs_core/$<CONFIG>"
    COMMAND ${CMAKE_COMMAND} -E touch
      "${AVS_UPSTREAM_BINARY_DIR}/avs_core/$<CONFIG>/AviSynth.exp"
    COMMENT "Preparing static-build compatibility export file"
    DEPENDEES configure
    DEPENDERS build
    ALWAYS 1
    USES_TERMINAL TRUE
  )
endif()

add_library(AvsCoreExternal STATIC IMPORTED GLOBAL)
set_target_properties(AvsCoreExternal PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES
    "${PROJECT_SOURCE_DIR}/third_party/AviSynthPlus/avs_core/include;${AVS_UPSTREAM_BINARY_DIR}/avs_core"
)
if(CMAKE_CONFIGURATION_TYPES)
  set_property(TARGET AvsCoreExternal PROPERTY
    IMPORTED_CONFIGURATIONS "${CMAKE_CONFIGURATION_TYPES}"
  )
  foreach(AVS_CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${AVS_CONFIG}" AVS_CONFIG_UPPER)
    set_property(TARGET AvsCoreExternal PROPERTY
      "IMPORTED_LOCATION_${AVS_CONFIG_UPPER}"
      "${AVS_UPSTREAM_BINARY_DIR}/avs_core/${AVS_CONFIG}/${AVS_UPSTREAM_LIBRARY_NAME}"
    )
  endforeach()
else()
  set_target_properties(AvsCoreExternal PROPERTIES
    IMPORTED_LOCATION "${AVS_UPSTREAM_LIBRARY}"
  )
endif()
target_compile_definitions(AvsCoreExternal INTERFACE AVS_STATIC_LIB)

if(WIN32)
  target_link_libraries(AvsCoreExternal INTERFACE
    uuid winmm vfw32 msacm32 gdi32 user32 advapi32 ole32 imagehlp
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
  target_link_libraries(AvsCoreExternal INTERFACE Threads::Threads root)
else()
  target_link_libraries(AvsCoreExternal INTERFACE
    Threads::Threads ${CMAKE_DL_LIBS} m
  )
endif()

add_dependencies(AvsCoreExternal avisynth_upstream)

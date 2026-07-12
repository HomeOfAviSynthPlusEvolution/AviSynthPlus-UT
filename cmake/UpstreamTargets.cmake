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

add_library(AvsCoreExternal STATIC IMPORTED GLOBAL)
set_target_properties(AvsCoreExternal PROPERTIES
  IMPORTED_LOCATION "${AVS_UPSTREAM_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES
    "${PROJECT_SOURCE_DIR}/third_party/AviSynthPlus/avs_core/include;${AVS_UPSTREAM_BINARY_DIR}/avs_core"
)
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

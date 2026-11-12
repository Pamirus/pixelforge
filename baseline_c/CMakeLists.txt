# The C baseline: plain C11 with malloc and free. Later stages are compared with it.
add_library(pixelforge_c_core STATIC
  pf_image.c
  pf_filters.c)
target_include_directories(pixelforge_c_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pixelforge_c_core PRIVATE stb pixelforge_warnings)

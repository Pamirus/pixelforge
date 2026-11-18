# The C++ version of pixelforge. It changes stage by stage; see docs/ROADMAP.md.
add_library(pixelforge_core STATIC
  filters.cpp
  image.cpp
  image_view.cpp
  io.cpp)
target_include_directories(pixelforge_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(pixelforge_core PRIVATE stb pixelforge_warnings)

add_executable(pixelforge main.cpp)
target_link_libraries(pixelforge PRIVATE pixelforge_core pixelforge_warnings)

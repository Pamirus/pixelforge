# The C++ version of pixelforge. It changes stage by stage; see docs/ROADMAP.md.
add_library(pixelforge_core STATIC
  image.cpp)
target_include_directories(pixelforge_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(pixelforge_core PRIVATE pixelforge_warnings)

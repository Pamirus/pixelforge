# stb is third-party code: build it once as a static C library, without warnings,
# and expose its headers as system headers so they do not warn in our code either.
add_library(stb STATIC stb_impl.c)
target_include_directories(stb SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT MSVC)
  target_compile_options(stb PRIVATE -w)
endif()

# stb_image calls pow() and ldexp(); on Linux those live in libm.
if(UNIX AND NOT APPLE)
  target_link_libraries(stb PUBLIC m)
endif()

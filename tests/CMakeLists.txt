include(FetchContent)

FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
  URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
  SYSTEM)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # same MSVC runtime as our targets
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(pixelforge_tests
  stb_smoke_test.cpp
  c_baseline_test.cpp)
target_link_libraries(pixelforge_tests PRIVATE
  pixelforge_c_core stb GTest::gtest_main pixelforge_warnings)
gtest_discover_tests(pixelforge_tests)

# Command-line checks for the C baseline. They pass when the output matches.
add_test(NAME cli_c.help COMMAND pixelforge_c --help)
add_test(NAME cli_c.no_arguments COMMAND pixelforge_c)
add_test(NAME cli_c.bad_brightness COMMAND pixelforge_c in.png out.png --brightness 1000)
add_test(NAME cli_c.unknown_option COMMAND pixelforge_c in.png out.png --sepia)
add_test(NAME cli_c.missing_input COMMAND pixelforge_c no-such-file.png out.png)
set_tests_properties(cli_c.help cli_c.no_arguments
  PROPERTIES PASS_REGULAR_EXPRESSION "usage: pixelforge_c")
set_tests_properties(cli_c.bad_brightness
  PROPERTIES PASS_REGULAR_EXPRESSION "--brightness needs a whole number")
set_tests_properties(cli_c.unknown_option
  PROPERTIES PASS_REGULAR_EXPRESSION "unknown option '--sepia'")
set_tests_properties(cli_c.missing_input
  PROPERTIES PASS_REGULAR_EXPRESSION "cannot load no-such-file.png")

# The whole path on a real 4K photo: load, both filters, save.
add_test(NAME cli_c.photo
  COMMAND pixelforge_c ${PROJECT_SOURCE_DIR}/images/cat-4k.jpg
          ${CMAKE_CURRENT_BINARY_DIR}/cat-4k-gray.jpg --grayscale --brightness 20)

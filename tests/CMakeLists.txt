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
  blur_test.cpp
  c_baseline_test.cpp
  counted_image_test.cpp
  factory_test.cpp
  filters_test.cpp
  histogram_test.cpp
  image_test.cpp
  image_view_test.cpp
  lazy_test.cpp
  io_test.cpp
  parallel_test.cpp
  negative_variants_test.cpp
  planar_test.cpp
  poly_test.cpp
  return_value_test.cpp
  scoped_timer_test.cpp)
target_link_libraries(pixelforge_tests PRIVATE
  pixelforge_core pixelforge_c_core stb GTest::gtest_main pixelforge_warnings)
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

# The same checks for the C++ command line, plus its extra filters and options.
add_test(NAME cli.help COMMAND pixelforge --help)
add_test(NAME cli.no_arguments COMMAND pixelforge)
add_test(NAME cli.bad_brightness COMMAND pixelforge in.png out.png --brightness 1000)
add_test(NAME cli.bad_threshold COMMAND pixelforge in.png out.png --threshold -1)
add_test(NAME cli.bad_blur COMMAND pixelforge in.png out.png --blur 11)
add_test(NAME cli.missing_number COMMAND pixelforge in.png out.png --threshold)
add_test(NAME cli.unknown_option COMMAND pixelforge in.png out.png --sepia)
add_test(NAME cli.missing_input COMMAND pixelforge no-such-file.png out.png)
add_test(NAME cli.unknown_output_format
  COMMAND pixelforge ${PROJECT_SOURCE_DIR}/docs/images/cat.jpg
          ${CMAKE_CURRENT_BINARY_DIR}/cat.gif)
set_tests_properties(cli.help cli.no_arguments
  PROPERTIES PASS_REGULAR_EXPRESSION "usage: pixelforge INPUT OUTPUT")
set_tests_properties(cli.bad_brightness
  PROPERTIES PASS_REGULAR_EXPRESSION "--brightness needs a whole number from -255 to 255")
set_tests_properties(cli.bad_threshold cli.missing_number
  PROPERTIES PASS_REGULAR_EXPRESSION "--threshold needs a whole number from 0 to 255")
set_tests_properties(cli.bad_blur
  PROPERTIES PASS_REGULAR_EXPRESSION "--blur needs a whole number from 1 to 10")
set_tests_properties(cli.unknown_option
  PROPERTIES PASS_REGULAR_EXPRESSION "unknown option '--sepia'")
set_tests_properties(cli.missing_input
  PROPERTIES PASS_REGULAR_EXPRESSION "cannot load no-such-file.png: no such file")
set_tests_properties(cli.unknown_output_format
  PROPERTIES PASS_REGULAR_EXPRESSION "cat.gif: unsupported format")

# The whole path on the 4K photo with every filter. --time prints each step.
add_test(NAME cli.photo
  COMMAND pixelforge ${PROJECT_SOURCE_DIR}/images/cat-4k.jpg
          ${CMAKE_CURRENT_BINARY_DIR}/cat-4k-all.png
          --grayscale --brightness 20 --negative --threshold 128
          --flip-horizontal --flip-vertical --rotate --time)
set_tests_properties(cli.photo
  PROPERTIES PASS_REGULAR_EXPRESSION
  "threshold 128: [0-9]+\\.[0-9]+ ms.*rotate 90: [0-9]+\\.[0-9]+ ms.*save: [0-9]+\\.[0-9]+ ms")

# The blur on the small photo; on the 4K one it would slow the tests down.
add_test(NAME cli.blur
  COMMAND pixelforge ${PROJECT_SOURCE_DIR}/docs/images/cat.jpg
          ${CMAKE_CURRENT_BINARY_DIR}/cat-blur.png --blur 2 --time)
set_tests_properties(cli.blur
  PROPERTIES PASS_REGULAR_EXPRESSION "blur 2: [0-9]+\\.[0-9]+ ms")

add_test(NAME cli.sharpen_sobel
  COMMAND pixelforge ${PROJECT_SOURCE_DIR}/docs/images/cat.jpg
          ${CMAKE_CURRENT_BINARY_DIR}/cat-edges.png --sharpen --sobel --time)
set_tests_properties(cli.sharpen_sobel
  PROPERTIES PASS_REGULAR_EXPRESSION "sharpen: [0-9]+\\.[0-9]+ ms.*sobel: [0-9]+\\.[0-9]+ ms")

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
  stb_smoke_test.cpp)
target_link_libraries(pixelforge_tests PRIVATE stb GTest::gtest_main pixelforge_warnings)
gtest_discover_tests(pixelforge_tests)

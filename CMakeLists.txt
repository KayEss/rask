cmake_minimum_required(VERSION 3.14)
project(rask LANGUAGES CXX)

# Aggregate target the vendored Felspar/planet test suites attach themselves to.
# Build it explicitly (`cmake --build build.tmp --target check`) to compile and
# run every dependency's tests; a normal build leaves them out.
if(NOT TARGET check)
    add_custom_target(check)
endif()

add_subdirectory(libs)
add_subdirectory(src)

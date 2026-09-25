cmake_minimum_required(VERSION 4.2)
project(TracingApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(TracingApp WIN32 src/app/main.cpp)
target_compile_definitions(TracingApp PRIVATE UNICODE _UNICODE)
target_compile_features(TracingApp PRIVATE cxx_std_23)

add_executable(TracingApp.BootstrapTests
    tests/unit/bootstrap_tests.cpp
    src/app/main.cpp)
target_compile_definitions(TracingApp.BootstrapTests PRIVATE
    UNICODE
    _UNICODE
    TRACING_APP_TESTING)
target_compile_features(TracingApp.BootstrapTests PRIVATE cxx_std_23)

enable_testing()
add_test(NAME TracingApp.BootstrapTests COMMAND TracingApp.BootstrapTests)

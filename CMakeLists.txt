cmake_minimum_required(VERSION 4.2)
project(TracingApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

find_package(GTest CONFIG REQUIRED)

function(tracing_app_apply_first_party_options target)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE)
    target_compile_features(${target} PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    endif()
endfunction()

add_executable(TracingApp WIN32 src/app/main.cpp)
tracing_app_apply_first_party_options(TracingApp)

add_executable(TracingApp.BootstrapTests
    tests/unit/bootstrap_tests.cpp
    src/app/main.cpp)
target_compile_definitions(TracingApp.BootstrapTests PRIVATE TRACING_APP_TESTING)
tracing_app_apply_first_party_options(TracingApp.BootstrapTests)
target_link_libraries(TracingApp.BootstrapTests PRIVATE GTest::gtest_main)

enable_testing()
add_test(NAME TracingApp.BootstrapTests COMMAND TracingApp.BootstrapTests)

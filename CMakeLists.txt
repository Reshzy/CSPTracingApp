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
    target_include_directories(${target} PRIVATE "${CMAKE_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    endif()
endfunction()

set(TRACING_APP_PLATFORM_SOURCES
    src/platform/TargetDiscovery.cpp)

add_executable(TracingApp WIN32
    src/app/main.cpp
    ${TRACING_APP_PLATFORM_SOURCES}
    src/platform/TargetGeometry.cpp
    src/graphics/DeviceResources.cpp
    src/graphics/OverlaySurface.cpp
    src/graphics/CapturePreview.cpp
    src/capture/CaptureSession.cpp
    resources/app.manifest)
tracing_app_apply_first_party_options(TracingApp)
target_link_libraries(TracingApp PRIVATE dwmapi d3d11 dxgi dxguid gdi32 WindowsApp)

add_executable(TracingApp.BootstrapTests
    tests/unit/bootstrap_tests.cpp
    src/app/main.cpp)
target_compile_definitions(TracingApp.BootstrapTests PRIVATE TRACING_APP_TESTING)
tracing_app_apply_first_party_options(TracingApp.BootstrapTests)
target_link_libraries(TracingApp.BootstrapTests PRIVATE GTest::gtest_main)

add_executable(TracingApp.TargetSelectionTests
    tests/unit/TargetSelectionTests.cpp
    ${TRACING_APP_PLATFORM_SOURCES})
tracing_app_apply_first_party_options(TracingApp.TargetSelectionTests)
target_link_libraries(TracingApp.TargetSelectionTests PRIVATE GTest::gtest_main)

add_executable(TracingApp.GraphicsPolicyTests
    tests/unit/GraphicsPolicyTests.cpp)
tracing_app_apply_first_party_options(TracingApp.GraphicsPolicyTests)
target_link_libraries(TracingApp.GraphicsPolicyTests PRIVATE GTest::gtest_main)

add_executable(TracingApp.OverlayPolicyTests
    tests/unit/OverlayPolicyTests.cpp)
tracing_app_apply_first_party_options(TracingApp.OverlayPolicyTests)
target_link_libraries(TracingApp.OverlayPolicyTests PRIVATE GTest::gtest_main)

add_executable(TracingApp.CaptureStateTests
    tests/unit/CaptureStateTests.cpp)
tracing_app_apply_first_party_options(TracingApp.CaptureStateTests)
target_link_libraries(TracingApp.CaptureStateTests PRIVATE GTest::gtest_main)

add_executable(TracingApp.InputProbe WIN32
    tests/harness/InputProbe.cpp
    resources/app.manifest)
tracing_app_apply_first_party_options(TracingApp.InputProbe)

enable_testing()
add_test(NAME TracingApp.BootstrapTests COMMAND TracingApp.BootstrapTests)
add_test(NAME TracingApp.TargetSelectionTests COMMAND TracingApp.TargetSelectionTests)
add_test(NAME TracingApp.GraphicsPolicyTests COMMAND TracingApp.GraphicsPolicyTests)
add_test(NAME TracingApp.OverlayPolicyTests COMMAND TracingApp.OverlayPolicyTests)
add_test(NAME TracingApp.CaptureStateTests COMMAND TracingApp.CaptureStateTests)
add_test(NAME TracingApp.InputProbe COMMAND TracingApp.InputProbe --self-test)

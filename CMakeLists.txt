cmake_minimum_required(VERSION 4.2)
project(TracingApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

find_package(GTest CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED COMPONENTS core imgproc features2d calib3d)
message(STATUS "TracingApp OpenCV: ${OpenCV_VERSION}")

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
    src/platform/InputObserver.cpp
    src/platform/AccessibilityObserver.cpp
    src/graphics/DeviceResources.cpp
    src/graphics/OverlaySurface.cpp
    src/graphics/CapturePreview.cpp
    src/graphics/ImageRenderer.cpp
    src/app/ReferenceWindow.cpp
    src/capture/CaptureSession.cpp
    src/capture/RoiReadback.cpp
    src/image/ImageLoader.cpp
    src/core/Transform2D.cpp
    src/core/Calibration.cpp
    src/tracking/VisualTracker.cpp
    src/tracking/TrackingSession.cpp
    src/tracking/TransformFusion.cpp
    src/tracking/NavigatorObserver.cpp
    src/diagnostics/Metrics.cpp
    resources/app.manifest)
tracing_app_apply_first_party_options(TracingApp)
target_link_libraries(TracingApp PRIVATE
    dwmapi
    d3d11
    dxgi
    dxguid
    gdi32
    comctl32
    WindowsApp
    windowscodecs
    ole32
    uuid
    shell32
    UIAutomationCore
    oleaut32
    opencv_core
    opencv_imgproc
    opencv_features2d
    opencv_calib3d)

set(_fxc_hints)
if(DEFINED ENV{WindowsSdkVerBinPath})
    list(APPEND _fxc_hints "$ENV{WindowsSdkVerBinPath}x64")
endif()
if(DEFINED ENV{WindowsSdkDir})
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        list(APPEND _fxc_hints
            "$ENV{WindowsSdkDir}bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
    endif()
    list(APPEND _fxc_hints "$ENV{WindowsSdkDir}bin/x64")
endif()
if(CMAKE_WINDOWS_KITS_10_DIR)
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        list(APPEND _fxc_hints
            "${CMAKE_WINDOWS_KITS_10_DIR}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
    endif()
    list(APPEND _fxc_hints "${CMAKE_WINDOWS_KITS_10_DIR}/bin/x64")
endif()
get_filename_component(_windows_kits_root
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
    ABSOLUTE)
if(_windows_kits_root AND EXISTS "${_windows_kits_root}")
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        list(APPEND _fxc_hints
            "${_windows_kits_root}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
    endif()
    list(APPEND _fxc_hints "${_windows_kits_root}/bin/x64")
endif()
if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    list(APPEND _fxc_hints
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
        "$ENV{ProgramFiles}/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
endif()
unset(FXC_EXECUTABLE CACHE)
find_program(FXC_EXECUTABLE NAMES fxc fxc.exe HINTS ${_fxc_hints})
if(NOT FXC_EXECUTABLE)
    set(_fxc_glob_roots)
    if(DEFINED ENV{WindowsSdkDir})
        list(APPEND _fxc_glob_roots "$ENV{WindowsSdkDir}")
    endif()
    if(_windows_kits_root)
        list(APPEND _fxc_glob_roots "${_windows_kits_root}")
    endif()
    if(CMAKE_WINDOWS_KITS_10_DIR)
        list(APPEND _fxc_glob_roots "${CMAKE_WINDOWS_KITS_10_DIR}")
    endif()
    foreach(_fxc_root IN LISTS _fxc_glob_roots)
        file(GLOB _fxc_glob "${_fxc_root}/bin/*/x64/fxc.exe")
        if(_fxc_glob)
            list(SORT _fxc_glob)
            list(GET _fxc_glob -1 FXC_EXECUTABLE)
            break()
        endif()
    endforeach()
endif()
if(NOT FXC_EXECUTABLE)
    message(FATAL_ERROR
        "fxc.exe not found (missing Windows SDK shader compiler, not a source compile failure). "
        "SDK version='${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}'. Searched HINTS: ${_fxc_hints}.")
endif()
message(STATUS "TracingApp fxc: ${FXC_EXECUTABLE}")

set(TRACING_SHADER_DIR "${CMAKE_BINARY_DIR}/shaders")
set(TRACING_IMAGE_VS "${TRACING_SHADER_DIR}/ImageVS.cso")
set(TRACING_IMAGE_PS "${TRACING_SHADER_DIR}/ImagePS.cso")
add_custom_command(
    OUTPUT "${TRACING_IMAGE_VS}" "${TRACING_IMAGE_PS}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${TRACING_SHADER_DIR}"
    COMMAND "${FXC_EXECUTABLE}" /nologo /T vs_5_0 /E VSMain /Fo "${TRACING_IMAGE_VS}"
            "${CMAKE_SOURCE_DIR}/shaders/Image.hlsl"
    COMMAND "${FXC_EXECUTABLE}" /nologo /T ps_5_0 /E PSMain /Fo "${TRACING_IMAGE_PS}"
            "${CMAKE_SOURCE_DIR}/shaders/Image.hlsl"
    DEPENDS "${CMAKE_SOURCE_DIR}/shaders/Image.hlsl"
    COMMENT "Compiling shaders/Image.hlsl with discovered fxc"
    VERBATIM)
add_custom_target(TracingAppShaders DEPENDS "${TRACING_IMAGE_VS}" "${TRACING_IMAGE_PS}")
add_dependencies(TracingApp TracingAppShaders)
add_custom_command(TARGET TracingApp POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${TRACING_IMAGE_VS}"
        "${TRACING_IMAGE_PS}"
        "$<TARGET_FILE_DIR:TracingApp>"
    VERBATIM)

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

add_executable(TracingApp.ImageLoaderTests
    tests/unit/ImageLoaderTests.cpp
    src/image/ImageLoader.cpp)
tracing_app_apply_first_party_options(TracingApp.ImageLoaderTests)
target_link_libraries(TracingApp.ImageLoaderTests PRIVATE GTest::gtest_main windowscodecs ole32 uuid)

add_executable(TracingApp.TransformTests
    tests/unit/TransformTests.cpp
    src/core/Transform2D.cpp)
tracing_app_apply_first_party_options(TracingApp.TransformTests)
target_link_libraries(TracingApp.TransformTests PRIVATE GTest::gtest_main)

add_executable(TracingApp.CalibrationTests
    tests/unit/CalibrationTests.cpp
    src/core/Calibration.cpp
    src/core/Transform2D.cpp)
tracing_app_apply_first_party_options(TracingApp.CalibrationTests)
target_link_libraries(TracingApp.CalibrationTests PRIVATE GTest::gtest_main)

add_executable(TracingApp.RoiTests
    tests/unit/RoiTests.cpp)
tracing_app_apply_first_party_options(TracingApp.RoiTests)
target_link_libraries(TracingApp.RoiTests PRIVATE GTest::gtest_main)

add_executable(TracingApp.VisualTrackerTests
    tests/unit/VisualTrackerTests.cpp
    src/tracking/VisualTracker.cpp
    src/core/Transform2D.cpp)
tracing_app_apply_first_party_options(TracingApp.VisualTrackerTests)
target_link_libraries(TracingApp.VisualTrackerTests PRIVATE
    GTest::gtest_main
    opencv_core
    opencv_imgproc
    opencv_features2d
    opencv_calib3d)

add_executable(TracingApp.TrackingSessionTests
    tests/unit/TrackingSessionTests.cpp
    src/tracking/TrackingSession.cpp
    src/tracking/TransformFusion.cpp
    src/tracking/VisualTracker.cpp
    src/core/Transform2D.cpp)
tracing_app_apply_first_party_options(TracingApp.TrackingSessionTests)
target_link_libraries(TracingApp.TrackingSessionTests PRIVATE
    GTest::gtest_main
    opencv_core
    opencv_imgproc
    opencv_features2d
    opencv_calib3d)

add_executable(TracingApp.NavigatorTests
    tests/unit/NavigatorTests.cpp
    src/tracking/NavigatorObserver.cpp
    src/core/Calibration.cpp
    src/core/Transform2D.cpp)
tracing_app_apply_first_party_options(TracingApp.NavigatorTests)
target_link_libraries(TracingApp.NavigatorTests PRIVATE
    GTest::gtest_main
    opencv_core
    opencv_imgproc)

add_executable(TracingApp.InputPredictionTests
    tests/unit/InputPredictionTests.cpp
    src/platform/InputObserver.cpp)
tracing_app_apply_first_party_options(TracingApp.InputPredictionTests)
target_link_libraries(TracingApp.InputPredictionTests PRIVATE GTest::gtest_main)

add_executable(TracingApp.ObserverCapabilityTests
    tests/unit/ObserverCapabilityTests.cpp
    src/platform/AccessibilityObserver.cpp)
tracing_app_apply_first_party_options(TracingApp.ObserverCapabilityTests)
target_link_libraries(TracingApp.ObserverCapabilityTests PRIVATE
    GTest::gtest_main
    UIAutomationCore
    oleaut32
    ole32
    uuid)

add_executable(TracingApp.FusionTests
    tests/unit/FusionTests.cpp
    src/tracking/TransformFusion.cpp)
tracing_app_apply_first_party_options(TracingApp.FusionTests)
target_link_libraries(TracingApp.FusionTests PRIVATE GTest::gtest_main)

add_executable(TracingApp.MetricsTests
    tests/unit/MetricsTests.cpp
    src/diagnostics/Metrics.cpp)
tracing_app_apply_first_party_options(TracingApp.MetricsTests)
target_link_libraries(TracingApp.MetricsTests PRIVATE GTest::gtest_main)

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
add_test(NAME TracingApp.ImageLoaderTests COMMAND TracingApp.ImageLoaderTests)
add_test(NAME TracingApp.TransformTests COMMAND TracingApp.TransformTests)
add_test(NAME TracingApp.CalibrationTests COMMAND TracingApp.CalibrationTests)
add_test(NAME TracingApp.RoiTests COMMAND TracingApp.RoiTests)
add_test(NAME TracingApp.VisualTrackerTests COMMAND TracingApp.VisualTrackerTests)
add_test(NAME TracingApp.TrackingSessionTests COMMAND TracingApp.TrackingSessionTests)
add_test(NAME TracingApp.NavigatorTests COMMAND TracingApp.NavigatorTests)
add_test(NAME TracingApp.InputPredictionTests COMMAND TracingApp.InputPredictionTests)
add_test(NAME TracingApp.ObserverCapabilityTests COMMAND TracingApp.ObserverCapabilityTests)
add_test(NAME TracingApp.FusionTests COMMAND TracingApp.FusionTests)
add_test(NAME TracingApp.MetricsTests COMMAND TracingApp.MetricsTests)
add_test(NAME TracingApp.InputProbe COMMAND TracingApp.InputProbe --self-test)

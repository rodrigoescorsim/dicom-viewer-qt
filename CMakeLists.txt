cmake_minimum_required(VERSION 3.25)

project(DicomViewer VERSION 1.0.0 DESCRIPTION "DICOM image viewer" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DV_BUILD_TESTS "Build the automated tests" ON)
option(DV_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(DCMTK CONFIG REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
qt_standard_project_setup()
# Since Qt 6.10 the call above puts every executable in one folder on Windows. Keep CMake's default
# per-directory layout instead: the tests must NOT share the folder of the windeployqt-deployed app.
unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)

# Directory with the Qt tools and DLLs (used for windeployqt and for running the tests on Windows).
get_target_property(_dv_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(DV_QT_BIN_DIR "${_dv_qmake}" DIRECTORY)

include(ProjectOptions)
include(DeployQt)

add_subdirectory(src/core)
add_subdirectory(src/dicom)
add_subdirectory(src/app)

if(DV_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()

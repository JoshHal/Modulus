cmake_minimum_required(VERSION 3.16)
project(ModulusLite LANGUAGES CXX)

# ============================================================================
# GLOBAL SETTINGS
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt auto tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_definitions(
        -D_UNICODE 
        -DUNICODE       # AutoCAD is strictly Unicode
        -D_AFXDLL       # Required if using MFC/Shared DLLs
        -D_AC25_        # Replace 25 with your version (e.g., 24 for 2024)
        -D_WIN64        # Target 64-bit
    )
    
    # Ensure we use the Multi-threaded DLL runtime (/MD)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# ============================================================================
# DEPENDENCIES
# ============================================================================

# ObjectARX SDK (Windows version ONLY)
set(OBJECTARX_SDK_PATH
    "C:/CDROM1"
    CACHE PATH "Path to ObjectARX SDK"
)

message(STATUS "OBJECTARX_SDK_PATH = ${OBJECTARX_SDK_PATH}")

# Qt5
list(APPEND CMAKE_PREFIX_PATH "C:/Users/admin/Downloads/Modulus-main/5.15.2/msvc2019_64")
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

# ============================================================================
# CORE LIBRARY
# ============================================================================

add_library(ModulusLiteCore INTERFACE)

target_sources(ModulusLiteCore INTERFACE
    core.h
)

target_include_directories(ModulusLiteCore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================================
# CAD ADAPTER (ObjectARX)
# ============================================================================

add_library(ModulusLiteCAD STATIC
    cad_adapter.cpp
    cad_adapter.h
)

target_include_directories(ModulusLiteCAD PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OBJECTARX_SDK_PATH}/inc
    ${OBJECTARX_SDK_PATH}/inc-x64
)

target_link_directories(ModulusLiteCAD PUBLIC
    ${OBJECTARX_SDK_PATH}/lib
    ${OBJECTARX_SDK_PATH}/lib-x64
)

target_link_libraries(ModulusLiteCAD PUBLIC
    ModulusLiteCore
)

# ============================================================================
# UI LIBRARY (Qt)
# ============================================================================

add_library(ModulusLiteUI STATIC
    ui_panel.cpp
    ui_panel.h
)

target_include_directories(ModulusLiteUI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ModulusLiteUI PUBLIC
    ModulusLiteCore
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)

# ============================================================================
# PLUGIN (AutoCAD ARX)
# ============================================================================

add_library(ModulusLitePlugin MODULE
    plugin_main.cpp
    exports.def
)

target_include_directories(ModulusLitePlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OBJECTARX_SDK_PATH}/inc
)

target_link_directories(ModulusLitePlugin PRIVATE
    ${OBJECTARX_SDK_PATH}/lib
)

target_link_libraries(ModulusLitePlugin PRIVATE
    ModulusLiteCore
    ModulusLiteCAD
    ModulusLiteUI
)

set_target_properties(ModulusLitePlugin PROPERTIES
    PREFIX ""
    SUFFIX ".arx"
)

# Copy plugin after build
add_custom_command(TARGET ModulusLitePlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:ModulusLitePlugin>
        ${CMAKE_BINARY_DIR}/bin/ModulusLite.arx
)

# ============================================================================
# TESTS
# ============================================================================

enable_testing()

add_executable(ModulusLiteTests
    tests/test_core.cpp
    tests/test_routing.cpp
)

target_include_directories(ModulusLiteTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ModulusLiteTests PRIVATE
    ModulusLiteCore
)

add_test(NAME ModulusLiteTests COMMAND ModulusLiteTests)
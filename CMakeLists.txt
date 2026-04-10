cmake_minimum_required(VERSION 3.16)
project(ModulusLite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

# ============================================================================
# DEPENDENCIES
# ============================================================================

# ObjectARX SDK (set this to your AutoCAD 2027 Mac SDK path)
set(OBJECTARX_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../ObjectARX2027Mac" 
    CACHE PATH "Path to ObjectARX 2027 Mac SDK")

# Qt (required for UI)
# Add this line before find_package(Qt5 ...)
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt@5")
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

# ============================================================================
# CORE LIBRARY (Pure C++)
# ============================================================================

add_library(ModulusLiteCore STATIC
    core.h
)

target_include_directories(ModulusLiteCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(ModulusLiteCore PROPERTIES
    LINKER_LANGUAGE CXX
)

# ============================================================================
# CAD ADAPTER LIBRARY
# ============================================================================

add_library(ModulusLiteCAD STATIC
    cad_adapter.h
    cad_adapter.cpp
)

target_include_directories(ModulusLiteCAD PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OBJECTARX_SDK_PATH}/inc
    ${OBJECTARX_SDK_PATH}/inc/PAL/api
)

target_link_directories(ModulusLiteCAD PUBLIC
    ${OBJECTARX_SDK_PATH}/lib
)

# Link ObjectARX frameworks
target_link_libraries(ModulusLiteCAD PUBLIC
    ModulusLiteCore
    "-framework CoreFoundation"
    "-framework Cocoa"
)

# ============================================================================
# UI LIBRARY (Qt)
# ============================================================================

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(ModulusLiteUI STATIC
    ui_panel.h
    ui_panel.cpp
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
# PLUGIN DLL/BUNDLE
# ============================================================================

add_library(ModulusLitePlugin MODULE
    plugin_main.cpp
)

target_include_directories(ModulusLitePlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OBJECTARX_SDK_PATH}/inc
    ${OBJECTARX_SDK_PATH}/inc/PAL/api
)

target_link_directories(ModulusLitePlugin PRIVATE
    ${OBJECTARX_SDK_PATH}/lib
)

target_link_libraries(ModulusLitePlugin PRIVATE
    ModulusLiteCore
    ModulusLiteCAD
    ModulusLiteUI
    "-framework CoreFoundation"
    "-framework Cocoa"
)

# Set output extension for macOS
set_target_properties(ModulusLitePlugin PROPERTIES
    BUNDLE FALSE
    PREFIX ""
    SUFFIX ".bundle"
    MACOSX_BUNDLE FALSE
)

# Copy plugin to output directory
add_custom_command(TARGET ModulusLitePlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:ModulusLitePlugin>
    ${CMAKE_BINARY_DIR}/bin/ModulusLite.bundle
)

# ============================================================================
# UNIT TESTS (Optional)
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

# ============================================================================
# INSTALLATION
# ============================================================================

install(TARGETS ModulusLitePlugin
    LIBRARY DESTINATION "Library/Application Support/Autodesk/ApplicationPlugins"
)

install(FILES 
    README.md
    LICENSE
    DESTINATION ".")

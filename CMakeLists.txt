cmake_minimum_required(VERSION 3.24)
project(MyArxExtension LANGUAGES CXX)

# 1. Project & Standard Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)

# 2. Path to ObjectARX 2027 SDK
set(ARX_SDK_PATH "/Users/joshuahal/Developer/Autodesk/ObjectARX 2027")

# 3. Verify SDK exists
if(NOT EXISTS "${ARX_SDK_PATH}")
    message(FATAL_ERROR "ObjectARX SDK not found at: ${ARX_SDK_PATH}")
endif()

# 4. Create the Target
add_library(MyExtension MODULE 
    src/main.cpp
)

# 5. Include Directories
target_include_directories(MyExtension PRIVATE 
    "${ARX_SDK_PATH}/inc"
    "${ARX_SDK_PATH}/inc/PAL/api"
    "${ARX_SDK_PATH}/inc/PAL"
)

# 6. Compiler Flags for macOS/ARX
target_compile_options(MyExtension PRIVATE
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -fPIC
    # Suppress SDK header warnings (these are in the SDK, not our code)
    -Wno-nonportable-include-path
    -Wno-unused-value
    -Wno-format-security
)

# 7. Preprocessor Definitions
target_compile_definitions(MyExtension PRIVATE
    _ADESK_MAC_
    _AFXDLL
    _USRDLL
    _UNICODE
    UNICODE
)

# 8. Link Options
# Key: Use -undefined dynamic_lookup because ARX symbols are resolved at runtime
target_link_options(MyExtension PRIVATE
    -undefined dynamic_lookup
)

# 9. Link Frameworks
target_link_libraries(MyExtension PRIVATE
    "-framework Cocoa"
    "-framework CoreFoundation"
    "-framework AppKit"
)

# 10. Try to link against ARX libraries if they exist
# Search for libraries in common locations
find_library(ACCORE_LIB NAMES accore HINTS "${ARX_SDK_PATH}/lib" NO_DEFAULT_PATH)
find_library(ACDB_LIB NAMES acdb HINTS "${ARX_SDK_PATH}/lib" NO_DEFAULT_PATH)
find_library(ACGE_LIB NAMES acge HINTS "${ARX_SDK_PATH}/lib" NO_DEFAULT_PATH)

if(ACCORE_LIB)
    message(STATUS "Found accore: ${ACCORE_LIB}")
    target_link_libraries(MyExtension PRIVATE ${ACCORE_LIB})
endif()

if(ACDB_LIB)
    message(STATUS "Found acdb: ${ACDB_LIB}")
    target_link_libraries(MyExtension PRIVATE ${ACDB_LIB})
endif()

# 11. Bundle Configuration
set_target_properties(MyExtension PROPERTIES
    PREFIX ""
    SUFFIX ".bundle"
    BUNDLE TRUE
    BUNDLE_EXTENSION "bundle"
    MACOSX_BUNDLE_BUNDLE_NAME "MyExtension"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 12. Output configuration
message(STATUS "=== MyArxExtension Build Configuration ===")
message(STATUS "ARX SDK: ${ARX_SDK_PATH}")
message(STATUS "Output: ${CMAKE_BINARY_DIR}/bin/MyExtension.bundle")
message(STATUS "========================================")

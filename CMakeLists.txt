cmake_minimum_required(VERSION 3.24)
project(ModulusLite LANGUAGES CXX)

# ── 1. Project & Standard Settings ───────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)

# ── 2. Hardcoded paths (local machine) ───────────────────────────────────────
set(ARX_SDK_PATH "/Users/joshuahal/Developer/Autodesk/ObjectARX 2027")

# ── 3. Verify SDK exists ──────────────────────────────────────────────────────
if(NOT EXISTS "${ARX_SDK_PATH}")
    message(FATAL_ERROR "ObjectARX SDK not found at: ${ARX_SDK_PATH}")
endif()

# ── 4. Qt 6 ──────────────────────────────────────────────────────────────────
# Homebrew default path — adjust if Qt is installed elsewhere
set(Qt6_DIR "/opt/homebrew/opt/qt6/lib/cmake/Qt6")
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
qt6_standard_project_setup()

# ── 5. Source files ───────────────────────────────────────────────────────────
set(SOURCES
    ModulusLite.cpp
    ModulusController.cpp
    Core/FDGLayout.cpp
    Core/AStarRouter.cpp
    CADAdapter/CADAdapter.cpp
    UI/ModulusPanel.cpp
)

set(HEADERS
    ModulusController.h
    Core/Models.h
    Core/FDGLayout.h
    Core/AStarRouter.h
    Core/PipeProperties.h
    CADAdapter/CADAdapter.h
    UI/ModulusPanel.h
)

# ── 6. Create the target ──────────────────────────────────────────────────────
add_library(ModulusLite MODULE ${SOURCES} ${HEADERS})

# Qt MOC for QObject subclasses (ModulusPanel)
set_target_properties(ModulusLite PROPERTIES
    AUTOMOC ON
    PREFIX ""
    SUFFIX ".bundle"
    BUNDLE TRUE
    BUNDLE_EXTENSION "bundle"
    MACOSX_BUNDLE_BUNDLE_NAME "ModulusLite"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ── 7. Include directories ────────────────────────────────────────────────────
target_include_directories(ModulusLite PRIVATE
    "${ARX_SDK_PATH}/inc"
    "${ARX_SDK_PATH}/inc/PAL/api"
    "${ARX_SDK_PATH}/inc/PAL"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# ── 8. Compiler flags ─────────────────────────────────────────────────────────
target_compile_options(ModulusLite PRIVATE
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -fPIC
    -Wno-nonportable-include-path
    -Wno-unused-value
    -Wno-format-security
)

# ── 9. Preprocessor definitions ───────────────────────────────────────────────
target_compile_definitions(ModulusLite PRIVATE
    _ADESK_MAC_
    _AFXDLL
    _USRDLL
    _UNICODE
    UNICODE
    QT_NO_KEYWORDS
)

# ── 10. Link options ──────────────────────────────────────────────────────────
# ARX symbols are resolved at runtime by the host application
target_link_options(ModulusLite PRIVATE
    -undefined dynamic_lookup
)

# ── 11. Link libraries ────────────────────────────────────────────────────────
target_link_libraries(ModulusLite PRIVATE
    Qt6::Core
    Qt6::Widgets
    "-framework Cocoa"
    "-framework CoreFoundation"
    "-framework AppKit"
)

# ── 12. ARX libraries (optional — resolved at runtime if absent) ──────────────
find_library(ACCORE_LIB NAMES accore HINTS "${ARX_SDK_PATH}/lib" NO_DEFAULT_PATH)
find_library(ACDB_LIB   NAMES acdb   HINTS "${ARX_SDK_PATH}/lib" NO_DEFAULT_PATH)
find_library(ACGE_LIB   NAMES acge   HINTS "${ARX_SDK_PATH}/lib" NO_DEFAULT_PATH)

if(ACCORE_LIB)
    message(STATUS "Found accore: ${ACCORE_LIB}")
    target_link_libraries(ModulusLite PRIVATE ${ACCORE_LIB})
endif()
if(ACDB_LIB)
    message(STATUS "Found acdb: ${ACDB_LIB}")
    target_link_libraries(ModulusLite PRIVATE ${ACDB_LIB})
endif()
if(ACGE_LIB)
    message(STATUS "Found acge: ${ACGE_LIB}")
    target_link_libraries(ModulusLite PRIVATE ${ACGE_LIB})
endif()

# ── 13. Install ───────────────────────────────────────────────────────────────
install(TARGETS ModulusLite
    LIBRARY DESTINATION
    "$ENV{HOME}/Library/Application Support/Autodesk/AutoCAD 2027/R26.0/enu/Plug-ins/ModulusLite"
)

# ── 14. Output summary ────────────────────────────────────────────────────────
message(STATUS "=== ModulusLite Build Configuration ===")
message(STATUS "ARX SDK : ${ARX_SDK_PATH}")
message(STATUS "Output  : ${CMAKE_BINARY_DIR}/bin/ModulusLite.bundle")
message(STATUS "========================================")
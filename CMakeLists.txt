cmake_minimum_required(VERSION 3.10)
project(MyArxExtension)

# 1. Set the Bundle output
set(CMAKE_CXX_STANDARD 17)
set(OUTPUT_NAME "MyExtension")

# 2. Path to your ObjectARX SDK∏
set(ARX_SDK_PATH "/Users/joshuahal/Developer/Autodesk/ObjectARX 2027")

# 3. Include Directoriesb
include_directories(
    "/Users/joshuahal/Developer/Autodesk/ObjectARX 2027/inc"
    "/Users/joshuahal/Developer/Autodesk/ObjectARX 2027/inc-arm64" # or inc-arm64
)

# 4. Create the Bundle
add_library(MODULUS MODULE src/main.cpp)

set_target_properties(MODULUS PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION "bundle"
    OUTPUT_NAME ./bin
)

# 5. Link Libraries
target_link_directories(MODULUS PRIVATE "/Users/joshuahal/Developer/Autodesk/ObjectARX 2027/lib-arm64")
target_link_libraries( MODULUS
    "-framework Cocoa"
    # Add specific ARX libs here, e.g., acdb24, accore, etc.
    acge accore acdb
)
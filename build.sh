#!/bin/bash

# Build script for MyArxExtension Hello World project

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== MyArxExtension Build Script ===${NC}\n"

# Create build directory
if [ ! -d "build" ]; then
    echo -e "${YELLOW}Creating build directory...${NC}"
    mkdir -p build
fi

# Change to build directory
cd build

# Run CMake
echo -e "${YELLOW}Running CMake...${NC}"
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Build
echo -e "${YELLOW}Building project...${NC}"
cmake --build . --config Debug --parallel 4

# Success message
echo -e "\n${GREEN}=== Build Complete ===${NC}"
echo -e "${GREEN}Output: $(pwd)/bin/MyExtension.bundle${NC}\n"

# Display next steps
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Copy the bundle to AutoCAD's plug-in folder:"
echo "   cp -r bin/MyExtension.bundle ~/Library/Application\\ Support/Autodesk/AutoCAD\\ 2027/Plug-ins/"
echo ""
echo "2. Launch AutoCAD and load the plugin"
echo "3. Type 'HELLOWORLD' in the command line to test"

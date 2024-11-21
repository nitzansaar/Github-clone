#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

echo "Starting build process..."

# Create bin directory if it doesn't exist
mkdir -p bin

# Array of components to compile
declare -A components=(
    ["serverM.c"]="serverM"
    ["serverA.c"]="serverA"
    ["serverR.c"]="serverR"
    ["client.c"]="client"
)

# Compile each component
for source in "${!components[@]}"; do
    target="bin/${components[$source]}"
    echo -n "Compiling $source -> $target ... "
    
    if clang "$source" -o "$target"; then
        echo -e "${GREEN}SUCCESS${NC}"
    else
        echo -e "${RED}FAILED${NC}"
        echo "Build process failed. Please check the errors above."
        exit 1
    fi
done

echo -e "\n${GREEN}Build completed successfully!${NC}"
echo "Executables are in the bin/ directory"
echo -e "\nYou can run the servers and client using:"
echo "bin/serverM"
echo "bin/serverA"
echo "bin/serverR"
echo "bin/client"
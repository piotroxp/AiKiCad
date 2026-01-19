#!/bin/bash
# Force CMake reconfiguration to pick up plugins include directory

set -e

cd "$(dirname "$0")/../../build" || exit 1

echo "Removing CMake cache and eeschema build files..."
rm -rf CMakeCache.txt CMakeFiles/eeschema

echo "Reconfiguring CMake..."
cmake .. 2>&1 | tail -5

echo ""
echo "Building eeschema_kiface_objects..."
make -j$(nproc) eeschema_kiface_objects 2>&1 | tail -20

echo ""
echo "Checking for errors..."
if make eeschema_kiface_objects 2>&1 | grep -q "fatal error.*plugins/ai_chat"; then
    echo "ERROR: Still getting include path error!"
    echo "Checking compiler flags..."
    cat eeschema/CMakeFiles/eeschema_kiface_objects.dir/flags.make 2>/dev/null | grep "CXX_INCLUDES" | head -1 | tr ' ' '\n' | grep -i "plugins" || echo "plugins path NOT found in compiler flags!"
    exit 1
else
    echo "SUCCESS: Build completed without include errors!"
    exit 0
fi

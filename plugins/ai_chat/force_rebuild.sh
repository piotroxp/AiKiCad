#!/bin/bash
# Force rebuild script - cleans, reconfigures, and builds until success

set -euo pipefail

BUILD_DIR="/nvme3/KiCad/kicad-source-mirror/build"
SOURCE_DIR="/nvme3/KiCad/kicad-source-mirror"

cd "$BUILD_DIR" || exit 1

echo "=========================================="
echo "Force Rebuild Script"
echo "=========================================="
echo ""

# Step 1: Clean everything
echo "[1/4] Cleaning build directory..."
rm -rf CMakeCache.txt CMakeFiles/eeschema CMakeFiles/plugins
echo "✓ Cleaned"

# Step 2: Reconfigure CMake
echo ""
echo "[2/4] Reconfiguring CMake..."
if cmake "$SOURCE_DIR" 2>&1 | tee /tmp/cmake_force.log; then
    echo "✓ CMake configured successfully"
else
    echo "✗ CMake configuration failed!"
    tail -30 /tmp/cmake_force.log
    exit 1
fi

# Step 3: Build with -j12
echo ""
echo "[3/4] Building with make -j12..."
if make -j12 2>&1 | tee /tmp/build_force.log; then
    echo "✓ Build completed successfully"
else
    echo "✗ Build failed!"
    echo ""
    echo "Build errors:"
    grep -i "error\|fatal" /tmp/build_force.log | grep -v "note:" | tail -20
    exit 1
fi

# Step 4: Run tests
echo ""
echo "[4/4] Running tests..."
if ctest --output-on-failure 2>&1 | tee /tmp/test_force.log; then
    echo ""
    echo "=========================================="
    echo "✓✓✓ SUCCESS: Build and all tests passed! ✓✓✓"
    echo "=========================================="
    exit 0
else
    echo ""
    echo "✗ Some tests failed"
    echo "Test failures:"
    grep -i "fail\|error" /tmp/test_force.log | tail -20
    exit 1
fi

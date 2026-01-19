#!/bin/bash
# Build and test script - keeps trying until build succeeds and tests pass

set -e

BUILD_DIR="/nvme3/KiCad/kicad-source-mirror/build"
SOURCE_DIR="/nvme3/KiCad/kicad-source-mirror"
MAX_ATTEMPTS=10
ATTEMPT=1

cd "$BUILD_DIR" || exit 1

echo "=========================================="
echo "KiCad Build and Test Script"
echo "=========================================="
echo ""

while [ $ATTEMPT -le $MAX_ATTEMPTS ]; do
    echo "=== Attempt $ATTEMPT of $MAX_ATTEMPTS ==="
    echo ""
    
    # Clean build
    echo "Cleaning build directory..."
    rm -rf CMakeCache.txt CMakeFiles/eeschema CMakeFiles/plugins
    
    # Reconfigure CMake
    echo "Reconfiguring CMake..."
    if ! cmake "$SOURCE_DIR" 2>&1 | tee /tmp/cmake_${ATTEMPT}.log; then
        echo "ERROR: CMake configuration failed!"
        tail -20 /tmp/cmake_${ATTEMPT}.log
        ATTEMPT=$((ATTEMPT + 1))
        continue
    fi
    
    # Build with -j12
    echo ""
    echo "Building with make -j12..."
    if ! make -j12 2>&1 | tee /tmp/build_${ATTEMPT}.log; then
        echo "ERROR: Build failed!"
        echo "Checking for plugin-related errors..."
        grep -i "plugins/ai_chat\|fatal error.*plugins" /tmp/build_${ATTEMPT}.log | tail -5 || true
        echo ""
        echo "Last 20 lines of build log:"
        tail -20 /tmp/build_${ATTEMPT}.log
        ATTEMPT=$((ATTEMPT + 1))
        continue
    fi
    
    echo ""
    echo "✓ Build succeeded!"
    
    # Run tests
    echo ""
    echo "Running tests..."
    if ctest --output-on-failure 2>&1 | tee /tmp/test_${ATTEMPT}.log; then
        echo ""
        echo "=========================================="
        echo "✓ BUILD SUCCESSFUL AND ALL TESTS PASSED!"
        echo "=========================================="
        exit 0
    else
        echo ""
        echo "ERROR: Some tests failed!"
        echo "Test failures:"
        grep -i "fail\|error" /tmp/test_${ATTEMPT}.log | tail -10 || true
        ATTEMPT=$((ATTEMPT + 1))
        continue
    fi
done

echo ""
echo "=========================================="
echo "✗ FAILED after $MAX_ATTEMPTS attempts"
echo "=========================================="
exit 1

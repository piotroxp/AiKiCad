#!/bin/bash
# Fix CMake cache path issues and rebuild

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KICAD_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$KICAD_ROOT/build"

echo "=== Fixing CMake Cache ==="
echo "Source: $KICAD_ROOT"
echo "Build:  $BUILD_DIR"
echo ""

# Clean old cache if it exists
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Removing old CMakeCache.txt..."
    rm -f "$BUILD_DIR/CMakeCache.txt"
    rm -rf "$BUILD_DIR/CMakeFiles"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ""
echo "=== Configuring CMake ==="
cmake -DKICAD_BUILD_QA_TESTS=ON -DCMAKE_BUILD_TYPE=Debug "$KICAD_ROOT" 2>&1 | tee cmake.log | tail -20

echo ""
echo "=== Building Plugin ==="
make ai_chat_plugin 2>&1 | tee build_plugin.log | tail -30

echo ""
echo "=== Building Tests ==="
make qa_plugins 2>&1 | tee build_tests.log | tail -30

echo ""
echo "=== Running Tests ==="
if [ -f "qa/tests/plugins/qa_plugins" ]; then
    ./qa/tests/plugins/qa_plugins --log_level=test_suite 2>&1 | tee test_results.log
    echo ""
    echo "=== Test Summary ==="
    tail -20 test_results.log
else
    echo "❌ Test executable not found!"
    echo "Checking build directory..."
    ls -la qa/tests/plugins/ 2>&1 || echo "Directory doesn't exist"
fi

echo ""
echo "=== Build Complete ==="
echo "Logs saved to:"
echo "  - $BUILD_DIR/cmake.log"
echo "  - $BUILD_DIR/build_plugin.log"
echo "  - $BUILD_DIR/build_tests.log"
echo "  - $BUILD_DIR/test_results.log"

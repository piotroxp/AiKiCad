#!/bin/bash
# Clean build script for AI Chat Plugin

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KICAD_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$KICAD_ROOT/build"

echo "=========================================="
echo "Clean Build for AI Chat Plugin"
echo "=========================================="
echo "Source: $KICAD_ROOT"
echo "Build:  $BUILD_DIR"
echo ""

# Step 1: Clean build directory
echo "Step 1: Cleaning build directory..."
if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
    echo "  ✓ Removed old build directory"
fi
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Step 2: Configure CMake
echo ""
echo "Step 2: Configuring CMake..."
cmake -DKICAD_BUILD_QA_TESTS=ON -DCMAKE_BUILD_TYPE=Debug "$KICAD_ROOT" 2>&1 | tee cmake.log
CMAKE_EXIT=${PIPESTATUS[0]}

if [ $CMAKE_EXIT -ne 0 ]; then
    echo ""
    echo "❌ CMake configuration failed!"
    echo "Last 30 lines of cmake.log:"
    tail -30 cmake.log
    exit 1
fi

echo "  ✓ CMake configured successfully"

# Step 3: Build plugin
echo ""
echo "Step 3: Building plugin..."
make ai_chat_plugin 2>&1 | tee build_plugin.log
BUILD_EXIT=${PIPESTATUS[0]}

if [ $BUILD_EXIT -ne 0 ]; then
    echo ""
    echo "❌ Plugin build failed!"
    echo "Last 30 lines of build_plugin.log:"
    tail -30 build_plugin.log
    echo ""
    echo "Errors found:"
    grep -E "(error|Error|ERROR|fatal)" build_plugin.log | head -20
    exit 1
fi

# Check if plugin was built
if [ -f "plugins/ai_chat/libai_chat_plugin.a" ] || [ -f "plugins/ai_chat/libai_chat_plugin.so" ]; then
    echo "  ✓ Plugin built successfully"
    ls -lh plugins/ai_chat/libai_chat_plugin.* 2>/dev/null || true
else
    echo "  ⚠ Plugin library not found (may be in different location)"
fi

# Step 4: Build tests
echo ""
echo "Step 4: Building tests..."
make qa_plugins 2>&1 | tee build_tests.log
TEST_BUILD_EXIT=${PIPESTATUS[0]}

if [ $TEST_BUILD_EXIT -ne 0 ]; then
    echo ""
    echo "❌ Test build failed!"
    echo "Last 30 lines of build_tests.log:"
    tail -30 build_tests.log
    echo ""
    echo "Errors found:"
    grep -E "(error|Error|ERROR|fatal)" build_tests.log | head -20
    exit 1
fi

# Check if test executable was built
if [ -f "qa/tests/plugins/qa_plugins" ]; then
    echo "  ✓ Test executable built successfully"
    ls -lh qa/tests/plugins/qa_plugins
else
    echo "  ⚠ Test executable not found"
    exit 1
fi

# Step 5: Run tests
echo ""
echo "Step 5: Running tests..."
./qa/tests/plugins/qa_plugins --log_level=test_suite 2>&1 | tee test_results.log
TEST_EXIT=${PIPESTATUS[0]}

echo ""
echo "=========================================="
echo "Build Summary"
echo "=========================================="
if [ $TEST_EXIT -eq 0 ]; then
    echo "✅ All tests passed!"
else
    echo "❌ Some tests failed (exit code: $TEST_EXIT)"
    echo ""
    echo "Last 20 lines of test output:"
    tail -20 test_results.log
fi

echo ""
echo "Log files saved in: $BUILD_DIR"
echo "  - cmake.log"
echo "  - build_plugin.log"
echo "  - build_tests.log"
echo "  - test_results.log"

exit $TEST_EXIT

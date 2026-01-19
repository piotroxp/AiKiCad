#!/bin/bash
# Build and run tests for AI Chat Plugin

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KICAD_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$KICAD_ROOT/build"

echo "=========================================="
echo "AI Chat Plugin - Build and Test Script"
echo "=========================================="
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure CMake if needed
if [ ! -f "CMakeCache.txt" ]; then
    echo "Configuring CMake..."
    cmake -DKICAD_BUILD_QA_TESTS=ON \
          -DCMAKE_BUILD_TYPE=Debug \
          "$KICAD_ROOT" || {
        echo "CMake configuration failed!"
        exit 1
    }
    echo "CMake configured successfully"
    echo ""
fi

# Build the plugin and tests
echo "Building ai_chat_plugin and qa_plugins..."
make ai_chat_plugin qa_plugins 2>&1 | tee build.log || {
    echo "Build failed! Check build.log for details"
    exit 1
}
echo "Build completed successfully"
echo ""

# Check if test executable exists
TEST_EXE="$BUILD_DIR/qa/tests/plugins/qa_plugins"
if [ ! -f "$TEST_EXE" ]; then
    echo "ERROR: Test executable not found at $TEST_EXE"
    echo "Build may have failed. Check build.log"
    exit 1
fi

# Run tests
echo "=========================================="
echo "Running Tests"
echo "=========================================="
echo ""

"$TEST_EXE" --log_level=test_suite 2>&1 | tee test_results.log

TEST_EXIT_CODE=${PIPESTATUS[0]}

echo ""
echo "=========================================="
if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo "✅ All tests passed!"
else
    echo "❌ Some tests failed (exit code: $TEST_EXIT_CODE)"
fi
echo "=========================================="
echo ""
echo "Test results saved to: test_results.log"
echo "Build log saved to: build.log"

exit $TEST_EXIT_CODE

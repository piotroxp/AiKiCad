#!/bin/bash
# Keep building until success - runs in a loop

BUILD_DIR="/nvme3/KiCad/kicad-source-mirror/build"
SOURCE_DIR="/nvme3/KiCad/kicad-source-mirror"

cd "$BUILD_DIR" || exit 1

attempt=1
max_attempts=50

while [ $attempt -le $max_attempts ]; do
    echo ""
    echo "=========================================="
    echo "BUILD ATTEMPT $attempt of $max_attempts"
    echo "=========================================="
    date
    echo ""
    
    # Clean
    echo "[$attempt] Cleaning..."
    rm -rf CMakeCache.txt CMakeFiles/eeschema CMakeFiles/plugins
    
    # Configure
    echo "[$attempt] Configuring CMake..."
    if ! cmake "$SOURCE_DIR" > /tmp/cmake_${attempt}.log 2>&1; then
        echo "✗ CMake failed - attempt $attempt"
        tail -20 /tmp/cmake_${attempt}.log
        attempt=$((attempt + 1))
        sleep 2
        continue
    fi
    
    # Build
    echo "[$attempt] Building with make -j12..."
    if ! make -j12 > /tmp/build_${attempt}.log 2>&1; then
        echo "✗ Build failed - attempt $attempt"
        echo "Errors:"
        grep -i "error\|fatal" /tmp/build_${attempt}.log | grep -v "note:" | tail -10
        attempt=$((attempt + 1))
        sleep 2
        continue
    fi
    
    echo "✓ Build succeeded!"
    
    # Tests
    echo "[$attempt] Running tests..."
    if ctest --output-on-failure > /tmp/test_${attempt}.log 2>&1; then
        echo ""
        echo "=========================================="
        echo "✓✓✓ SUCCESS! Build and tests passed! ✓✓✓"
        echo "=========================================="
        echo "Attempt: $attempt"
        echo "Logs: /tmp/{cmake,build,test}_${attempt}.log"
        exit 0
    else
        echo "✗ Tests failed - attempt $attempt"
        grep -i "fail\|error" /tmp/test_${attempt}.log | tail -10
        attempt=$((attempt + 1))
        sleep 2
        continue
    fi
done

echo "Failed after $max_attempts attempts"
exit 1

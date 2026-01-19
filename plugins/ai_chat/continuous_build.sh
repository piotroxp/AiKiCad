#!/bin/bash
# Continuous build script - keeps building until success

BUILD_DIR="/nvme3/KiCad/kicad-source-mirror/build"
SOURCE_DIR="/nvme3/KiCad/kicad-source-mirror"
LOG_DIR="/tmp/kicad_build_logs"

mkdir -p "$LOG_DIR"

cd "$BUILD_DIR" || exit 1

attempt=1
max_attempts=20

while [ $attempt -le $max_attempts ]; do
    log_file="$LOG_DIR/build_attempt_${attempt}.log"
    echo "=== Attempt $attempt ===" | tee -a "$log_file"
    date | tee -a "$log_file"
    
    # Clean
    echo "Cleaning..." | tee -a "$log_file"
    rm -rf CMakeCache.txt CMakeFiles/eeschema CMakeFiles/plugins 2>&1 | tee -a "$log_file"
    
    # Configure
    echo "Configuring CMake..." | tee -a "$log_file"
    if cmake "$SOURCE_DIR" >> "$log_file" 2>&1; then
        echo "✓ CMake configured" | tee -a "$log_file"
    else
        echo "✗ CMake failed" | tee -a "$log_file"
        tail -30 "$log_file"
        attempt=$((attempt + 1))
        sleep 2
        continue
    fi
    
    # Build
    echo "Building with make -j12..." | tee -a "$log_file"
    if make -j12 >> "$log_file" 2>&1; then
        echo "✓ Build succeeded!" | tee -a "$log_file"
        
        # Run tests
        echo "Running tests..." | tee -a "$log_file"
        if ctest --output-on-failure >> "$log_file" 2>&1; then
            echo "✓✓✓ ALL TESTS PASSED! ✓✓✓" | tee -a "$log_file"
            echo "Build log: $log_file"
            exit 0
        else
            echo "✗ Some tests failed" | tee -a "$log_file"
            grep -i "fail\|error" "$log_file" | tail -20
        fi
    else
        echo "✗ Build failed" | tee -a "$log_file"
        echo "Errors:" | tee -a "$log_file"
        grep -i "error\|fatal" "$log_file" | tail -20 | tee -a "$log_file"
    fi
    
    attempt=$((attempt + 1))
    sleep 1
done

echo "Failed after $max_attempts attempts"
exit 1

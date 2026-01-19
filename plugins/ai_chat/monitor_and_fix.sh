#!/bin/bash
# Monitor build and fix issues - checks every 10 seconds, up to 25 times

BUILD_DIR="/nvme3/KiCad/kicad-source-mirror/build"
SOURCE_DIR="/nvme3/KiCad/kicad-source-mirror"
MAX_ITERATIONS=25
CHECK_INTERVAL=10

cd "$BUILD_DIR" || exit 1

echo "=========================================="
echo "Build Monitor and Auto-Fix"
echo "=========================================="
echo "Will check every ${CHECK_INTERVAL} seconds, up to ${MAX_ITERATIONS} times"
echo ""

iteration=1

while [ $iteration -le $MAX_ITERATIONS ]; do
    echo ""
    echo "=== Iteration $iteration of $MAX_ITERATIONS ==="
    echo "Waiting ${CHECK_INTERVAL} seconds..."
    sleep $CHECK_INTERVAL
    
    echo "Checking build status..."
    
    # Check if build is still running
    if ps aux | grep -E "make.*j12|cmake" | grep -v grep > /dev/null; then
        echo "✓ Build process still running..."
        iteration=$((iteration + 1))
        continue
    fi
    
    # Check for build errors
    if [ -f /tmp/build_force.log ]; then
        errors=$(grep -i "error\|fatal" /tmp/build_force.log 2>/dev/null | grep -v "note:" | wc -l)
        if [ "$errors" -gt 0 ]; then
            echo "✗ Found $errors build errors"
            echo "Recent errors:"
            grep -i "error\|fatal" /tmp/build_force.log 2>/dev/null | grep -v "note:" | tail -5
            
            # Check for specific plugin include error
            if grep -q "plugins/ai_chat.*Nie ma takiego pliku" /tmp/build_force.log 2>/dev/null; then
                echo ""
                echo ">>> Fixing plugin include path issue..."
                # Force CMake reconfiguration
                rm -rf CMakeCache.txt CMakeFiles/eeschema CMakeFiles/plugins
                cmake "$SOURCE_DIR" > /tmp/cmake_fix.log 2>&1
                echo "✓ CMake reconfigured"
            fi
            
            # Trigger rebuild
            echo ">>> Triggering rebuild..."
            make -j12 > /tmp/build_force.log 2>&1 &
        fi
    fi
    
    # Check if build succeeded
    if [ -f eeschema/CMakeFiles/eeschema_kiface_objects.dir/sch_edit_frame.cpp.o ] && \
       [ -f plugins/ai_chat/libai_chat_plugin.a ] 2>/dev/null || \
       [ -f plugins/ai_chat/libai_chat_plugin.so ] 2>/dev/null; then
        echo "✓ Build artifacts found!"
        
        # Check if tests pass
        echo "Running tests..."
        if ctest --output-on-failure > /tmp/test_check.log 2>&1; then
            echo ""
            echo "=========================================="
            echo "✓✓✓ SUCCESS! Build and tests passed! ✓✓✓"
            echo "=========================================="
            exit 0
        else
            echo "✗ Some tests failed"
            grep -i "fail\|error" /tmp/test_check.log | tail -5
        fi
    fi
    
    iteration=$((iteration + 1))
done

echo ""
echo "Reached maximum iterations ($MAX_ITERATIONS)"
echo "Final status check..."
./plugins/ai_chat/check_build_status.sh

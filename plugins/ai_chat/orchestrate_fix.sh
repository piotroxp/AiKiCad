#!/bin/bash
# Active orchestration - monitors and fixes until build succeeds

set -euo pipefail

BUILD_DIR="/nvme3/KiCad/kicad-source-mirror/build"
SOURCE_DIR="/nvme3/KiCad/kicad-source-mirror"
LOG_FILE="/tmp/orchestrate_main.log"

cd "$BUILD_DIR" || exit 1

echo "==========================================" | tee -a "$LOG_FILE"
echo "ACTIVE BUILD ORCHESTRATION" | tee -a "$LOG_FILE"
echo "==========================================" | tee -a "$LOG_FILE"
echo "Started at: $(date)" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

iteration=1
max_iterations=25

while [ $iteration -le $max_iterations ]; do
    echo "" | tee -a "$LOG_FILE"
    echo "=== ORCHESTRATION ITERATION $iteration/$max_iterations ===" | tee -a "$LOG_FILE"
    echo "Time: $(date +%H:%M:%S)" | tee -a "$LOG_FILE"
    
    # Wait 10 seconds
    sleep 10
    
    # Check for plugin include errors in all logs
    echo "Checking for errors..." | tee -a "$LOG_FILE"
    has_error=false
    error_file=""
    
    for log in /tmp/*.log /tmp/orchestrate*.log /tmp/build*.log /tmp/direct*.log /tmp/final*.log 2>/dev/null; do
        if [ -f "$log" ] && grep -qi "plugins/ai_chat.*nie ma\|fatal.*plugins/ai_chat" "$log" 2>/dev/null; then
            has_error=true
            error_file="$log"
            echo "✗ Plugin include error found in: $log" | tee -a "$LOG_FILE"
            break
        fi
    done
    
    # Fix if error detected
    if [ "$has_error" = true ]; then
        echo ">>> FIXING: Plugin include path issue" | tee -a "$LOG_FILE"
        echo "Removing CMake cache..." | tee -a "$LOG_FILE"
        rm -rf CMakeCache.txt CMakeFiles/eeschema CMakeFiles/plugins
        
        echo "Reconfiguring CMake..." | tee -a "$LOG_FILE"
        if cmake "$SOURCE_DIR" > /tmp/orchestrate_cmake_${iteration}.log 2>&1; then
            echo "✓ CMake reconfigured successfully" | tee -a "$LOG_FILE"
        else
            echo "✗ CMake reconfiguration failed" | tee -a "$LOG_FILE"
            tail -10 /tmp/orchestrate_cmake_${iteration}.log | tee -a "$LOG_FILE"
        fi
    fi
    
    # Build
    echo ">>> Building eeschema_kiface_objects..." | tee -a "$LOG_FILE"
    if make -j12 eeschema_kiface_objects > /tmp/orchestrate_build_${iteration}.log 2>&1; then
        echo "✓ Build command completed" | tee -a "$LOG_FILE"
    else
        echo "✗ Build command failed" | tee -a "$LOG_FILE"
        tail -10 /tmp/orchestrate_build_${iteration}.log | tee -a "$LOG_FILE"
    fi
    
    # Check for errors in this build
    if grep -qi "plugins/ai_chat.*nie ma\|fatal.*plugins/ai_chat" /tmp/orchestrate_build_${iteration}.log 2>/dev/null; then
        echo "✗ Plugin errors still present in build" | tee -a "$LOG_FILE"
        grep -i "plugins/ai_chat.*nie ma\|fatal.*plugins" /tmp/orchestrate_build_${iteration}.log | tail -3 | tee -a "$LOG_FILE"
    else
        echo "✓ No plugin errors in this build" | tee -a "$LOG_FILE"
    fi
    
    # Check if object file exists
    if [ -f eeschema/CMakeFiles/eeschema_kiface_objects.dir/sch_edit_frame.cpp.o ]; then
        echo "✓✓ Object file created successfully!" | tee -a "$LOG_FILE"
        
        # Build plugin library
        echo ">>> Building ai_chat_plugin..." | tee -a "$LOG_FILE"
        if make -j12 ai_chat_plugin > /tmp/orchestrate_plugin_${iteration}.log 2>&1; then
            echo "✓ Plugin build completed" | tee -a "$LOG_FILE"
        fi
        
        # Check if plugin library exists
        if [ -f plugins/ai_chat/libai_chat_plugin.a ] || [ -f plugins/ai_chat/libai_chat_plugin.so ]; then
            echo "✓✓ Plugin library exists!" | tee -a "$LOG_FILE"
            
            # Build tests
            echo ">>> Building tests..." | tee -a "$LOG_FILE"
            if make -j12 qa_plugins > /tmp/orchestrate_tests_build_${iteration}.log 2>&1; then
                echo "✓ Tests built" | tee -a "$LOG_FILE"
            fi
            
            # Run tests
            echo ">>> Running tests..." | tee -a "$LOG_FILE"
            if ctest --output-on-failure -R qa_plugins > /tmp/orchestrate_test_${iteration}.log 2>&1; then
                echo "" | tee -a "$LOG_FILE"
                echo "==========================================" | tee -a "$LOG_FILE"
                echo "✓✓✓ SUCCESS! BUILD AND ALL TESTS PASSED! ✓✓✓" | tee -a "$LOG_FILE"
                echo "==========================================" | tee -a "$LOG_FILE"
                echo "Iteration: $iteration" | tee -a "$LOG_FILE"
                echo "Log file: $LOG_FILE" | tee -a "$LOG_FILE"
                exit 0
            else
                echo "✗ Some tests failed" | tee -a "$LOG_FILE"
                grep -i "fail\|error" /tmp/orchestrate_test_${iteration}.log | tail -5 | tee -a "$LOG_FILE"
            fi
        else
            echo "✗ Plugin library not found" | tee -a "$LOG_FILE"
        fi
    else
        echo "✗ Object file missing" | tee -a "$LOG_FILE"
        echo "Build errors:" | tee -a "$LOG_FILE"
        grep -i "error\|fatal" /tmp/orchestrate_build_${iteration}.log 2>/dev/null | grep -v "note:" | tail -5 | tee -a "$LOG_FILE" || true
    fi
    
    iteration=$((iteration + 1))
done

echo "" | tee -a "$LOG_FILE"
echo "Completed $max_iterations iterations" | tee -a "$LOG_FILE"
echo "Final status:" | tee -a "$LOG_FILE"
ls -lh eeschema/CMakeFiles/eeschema_kiface_objects.dir/sch_edit_frame.cpp.o plugins/ai_chat/libai_chat_plugin.* 2>/dev/null | tee -a "$LOG_FILE" || echo "Build artifacts missing" | tee -a "$LOG_FILE"
exit 1

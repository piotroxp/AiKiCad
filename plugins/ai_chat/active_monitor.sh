#!/bin/bash
# Active monitoring - checks every 10 seconds, fixes issues, runs 25 times

BUILD_DIR="/nvme3/KiCad/kicad-source-mirror/build"
SOURCE_DIR="/nvme3/KiCad/kicad-source-mirror"

cd "$BUILD_DIR" || exit 1

for iteration in {1..25}; do
    echo ""
    echo "=========================================="
    echo "ACTIVE MONITOR - Iteration $iteration/25"
    echo "Time: $(date +%H:%M:%S)"
    echo "=========================================="
    
    # Wait 10 seconds
    echo "Waiting 10 seconds..."
    sleep 10
    
    # Check for build errors
    echo "Checking for build errors..."
    has_plugin_error=false
    
    # Check recent build logs
    for log in /tmp/build_*.log /tmp/active_build.log /tmp/make_build.log; do
        if [ -f "$log" ]; then
            if grep -qi "plugins/ai_chat.*nie ma takiego pliku\|fatal error.*plugins/ai_chat" "$log" 2>/dev/null; then
                echo "✗ Found plugin include path error in $log"
                has_plugin_error=true
                break
            fi
        fi
    done
    
    # Fix include path issue if found
    if [ "$has_plugin_error" = true ]; then
        echo ""
        echo ">>> FIXING: Plugin include path issue detected"
        echo "Removing CMake cache..."
        rm -rf CMakeCache.txt CMakeFiles/eeschema CMakeFiles/plugins
        echo "Reconfiguring CMake..."
        if cmake "$SOURCE_DIR" > /tmp/cmake_fix_iter_${iteration}.log 2>&1; then
            echo "✓ CMake reconfigured successfully"
        else
            echo "✗ CMake reconfiguration failed"
            tail -10 /tmp/cmake_fix_iter_${iteration}.log
        fi
    fi
    
    # Check if critical object file exists
    if [ ! -f eeschema/CMakeFiles/eeschema_kiface_objects.dir/sch_edit_frame.cpp.o ]; then
        echo "✗ sch_edit_frame.cpp.o missing"
        echo ">>> Triggering rebuild of eeschema_kiface_objects..."
        make -j12 eeschema_kiface_objects > /tmp/rebuild_iter_${iteration}.log 2>&1 &
        echo "Build started in background"
    else
        echo "✓ sch_edit_frame.cpp.o exists"
    fi
    
    # Check if plugin library exists
    if [ -f plugins/ai_chat/libai_chat_plugin.a ] || [ -f plugins/ai_chat/libai_chat_plugin.so ]; then
        echo "✓ Plugin library exists"
        
        # Run tests
        echo "Running tests..."
        if ctest --output-on-failure -R qa_plugins > /tmp/test_iter_${iteration}.log 2>&1; then
            echo ""
            echo "=========================================="
            echo "✓✓✓ SUCCESS! Build and tests passed! ✓✓✓"
            echo "=========================================="
            echo "Iteration: $iteration"
            exit 0
        else
            echo "✗ Some tests failed"
            grep -i "fail\|error" /tmp/test_iter_${iteration}.log | tail -5
        fi
    else
        echo "✗ Plugin library not found"
    fi
    
    # Check if full build is needed
    if ! ps aux | grep -E "make.*j12" | grep -v grep > /dev/null; then
        echo "No active build process - checking if rebuild needed..."
        if [ ! -f eeschema/CMakeFiles/eeschema_kiface_objects.dir/sch_edit_frame.cpp.o ]; then
            echo ">>> Starting full build..."
            make -j12 > /tmp/full_build_iter_${iteration}.log 2>&1 &
        fi
    fi
    
    echo "Iteration $iteration complete. Next check in 10 seconds..."
done

echo ""
echo "Completed 25 monitoring iterations"
echo "Final status:"
ls -lh eeschema/CMakeFiles/eeschema_kiface_objects.dir/sch_edit_frame.cpp.o plugins/ai_chat/libai_chat_plugin.* 2>/dev/null

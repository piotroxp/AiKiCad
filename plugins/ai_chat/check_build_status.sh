#!/bin/bash
# Check build status and show progress

echo "=========================================="
echo "Build Status Check"
echo "=========================================="
echo ""

# Check if build processes are running
BUILD_PROCS=$(ps aux | grep -E "keep_building|force_rebuild|make.*j12" | grep -v grep | wc -l)
echo "Active build processes: $BUILD_PROCS"
echo ""

# Check latest build log
if [ -f /tmp/keep_building.out ]; then
    echo "Latest build output (last 20 lines):"
    tail -20 /tmp/keep_building.out
    echo ""
fi

# Check for build errors
if [ -f /tmp/build_force.log ]; then
    echo "Recent build errors:"
    grep -i "error\|fatal" /tmp/build_force.log | tail -5 || echo "No errors found"
    echo ""
fi

# Check if object files exist
if [ -f /nvme3/KiCad/kicad-source-mirror/build/eeschema/CMakeFiles/eeschema_kiface_objects.dir/sch_edit_frame.cpp.o ]; then
    echo "✓ sch_edit_frame.cpp.o exists"
else
    echo "✗ sch_edit_frame.cpp.o missing"
fi

if [ -f /nvme3/KiCad/kicad-source-mirror/build/plugins/ai_chat/libai_chat_plugin.a ] || \
   [ -f /nvme3/KiCad/kicad-source-mirror/build/plugins/ai_chat/libai_chat_plugin.so ]; then
    echo "✓ ai_chat_plugin library exists"
else
    echo "✗ ai_chat_plugin library missing"
fi

echo ""
echo "To view full build log: tail -f /tmp/keep_building.out"
echo "To stop builds: pkill -f keep_building"

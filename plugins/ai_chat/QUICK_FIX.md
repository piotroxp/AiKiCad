# Quick Fix for CMake Path Error

## Problem
CMake cache was created with a different source path (`/prizm/Prizm/...`) but workspace is now at `/nvme3/KiCad/...`

## Solution

Run these commands in your terminal:

```bash
cd /nvme3/KiCad/kicad-source-mirror

# Remove old CMake cache
rm -rf build/CMakeCache.txt build/CMakeFiles

# Reconfigure CMake
cd build
cmake -DKICAD_BUILD_QA_TESTS=ON -DCMAKE_BUILD_TYPE=Debug ..

# Build plugin
make ai_chat_plugin

# Build tests
make qa_plugins

# Run tests
./qa/tests/plugins/qa_plugins --log_level=test_suite
```

## Or Use the Automated Script

```bash
cd /nvme3/KiCad/kicad-source-mirror
./plugins/ai_chat/fix_build.sh
```

This script will:
1. Clean the old CMake cache
2. Reconfigure CMake with correct paths
3. Build the plugin
4. Build the tests
5. Run the tests
6. Save all output to log files

## Verify Build Success

After running, check:

```bash
# Plugin library should exist
ls -lh build/plugins/ai_chat/libai_chat_plugin.*

# Test executable should exist
ls -lh build/qa/tests/plugins/qa_plugins

# Check logs if issues occur
tail -50 build/cmake.log
tail -50 build/build_plugin.log
tail -50 build/build_tests.log
```

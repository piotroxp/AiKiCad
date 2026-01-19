# Build Instructions

## Problem: Old Path in Generated Files

If you see errors like:
```
fatal error: /prizm/Prizm/KiCad/kicad-source-mirror/build/...: Nie ma takiego pliku ani katalogu
```

This means generated files still have hardcoded paths from a previous build location.

## Solution: Clean Build

**Run these commands in your terminal:**

```bash
cd /nvme3/KiCad/kicad-source-mirror

# Remove entire build directory
rm -rf build

# Create fresh build directory
mkdir -p build
cd build

# Configure CMake (this will regenerate all files with correct paths)
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
./plugins/ai_chat/clean_build.sh
```

This script will:
1. Clean the build directory completely
2. Configure CMake
3. Build the plugin
4. Build the tests
5. Run the tests
6. Save all output to log files

## Verify Build Success

After building, check:

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

## Common Issues

### Issue: "Target eeschema/pcbnew may not be linked"
**Fixed:** Removed executable targets from CMakeLists.txt, added include directories instead.

### Issue: Old paths in generated files
**Solution:** Clean build directory completely (`rm -rf build`) and reconfigure.

### Issue: Missing headers
**Solution:** Ensure include directories are set in CMakeLists.txt:
- `${CMAKE_SOURCE_DIR}/include`
- `${CMAKE_SOURCE_DIR}/eeschema`
- `${CMAKE_SOURCE_DIR}/pcbnew`

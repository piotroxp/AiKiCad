# Build Commands for AI Chat Plugin

## Quick Build and Test

```bash
cd /nvme3/KiCad/kicad-source-mirror
mkdir -p build && cd build

# Configure (if not already done)
cmake -DKICAD_BUILD_QA_TESTS=ON -DCMAKE_BUILD_TYPE=Debug ..

# Build plugin and tests
make ai_chat_plugin qa_plugins

# Run tests
./qa/tests/plugins/qa_plugins --log_level=test_suite
```

## Step-by-Step

### 1. Configure CMake
```bash
cd /nvme3/KiCad/kicad-source-mirror/build
cmake -DKICAD_BUILD_QA_TESTS=ON -DCMAKE_BUILD_TYPE=Debug ..
```

### 2. Build Plugin Library
```bash
make ai_chat_plugin
```

Expected output: `libai_chat_plugin.a` (or `.so`) in `build/plugins/ai_chat/`

### 3. Build Test Executable
```bash
make qa_plugins
```

Expected output: `qa_plugins` executable in `build/qa/tests/plugins/`

### 4. Run Tests
```bash
./qa/tests/plugins/qa_plugins --log_level=test_suite
```

Expected: 7 tests passing

## Using the Automated Script

```bash
cd /nvme3/KiCad/kicad-source-mirror
./plugins/ai_chat/run_tests.sh
```

This script will:
1. Configure CMake if needed
2. Build the plugin and tests
3. Run the tests
4. Display results

## Verification

After build, verify:

```bash
# Check plugin library
ls -lh build/plugins/ai_chat/libai_chat_plugin.*

# Check test executable
ls -lh build/qa/tests/plugins/qa_plugins

# Run tests
./build/qa/tests/plugins/qa_plugins
```

## Troubleshooting

### Build Fails: Missing Dependencies
- Ensure KiCad core libraries are built first: `make kicommon common`
- Check that `kicad_curl` is available

### Test Executable Not Found
- Verify CMake configuration: `grep qa_plugins build/CMakeCache.txt`
- Check build log: `make qa_plugins 2>&1 | tail -50`

### Tests Fail
- Run with verbose output: `./qa/tests/plugins/qa_plugins --log_level=all`
- Check for missing libraries: `ldd qa/tests/plugins/qa_plugins` (Linux)

# Build and Test Instructions

## Quick Start

```bash
# 1. Configure CMake with tests enabled
cd /nvme3/KiCad/kicad-source-mirror
mkdir -p build && cd build
cmake -DKICAD_BUILD_QA_TESTS=ON -DCMAKE_BUILD_TYPE=Debug ..

# 2. Build the plugin and tests
make ai_chat_plugin qa_plugins

# 3. Run tests
./qa/tests/plugins/qa_plugins --log_level=test_suite

# Or use CTest
ctest -R qa_plugins -V
```

## Detailed Build Steps

### Prerequisites
- CMake 3.16+
- C++17 compatible compiler
- wxWidgets 3.0+
- Boost libraries (unit_test_framework)
- KiCad dependencies (kicommon, kicad_curl, etc.)

### Configuration

```bash
cmake -DKICAD_BUILD_QA_TESTS=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      -DKICAD_USE_OCE=OFF \
      ..
```

### Build Targets

- `ai_chat_plugin` - The plugin library
- `qa_plugins` - Test executable

### Running Tests

#### Basic Test Run
```bash
./qa/tests/plugins/qa_plugins
```

#### Verbose Output
```bash
./qa/tests/plugins/qa_plugins --log_level=all
```

#### Specific Test Suite
```bash
./qa/tests/plugins/qa_plugins --run_test=AI_Chat_Plugin_Tests
```

#### Via CTest
```bash
ctest -R qa_plugins -V
```

## Test Coverage

The test suite includes:

1. **TestCommandProcessorBasicParsing** - Basic command parsing
2. **TestCommandProcessorIdempotentOperations** - Idempotency verification  
3. **TestCommandProcessorTraceParsing** - Trace command parsing
4. **TestCommandProcessorContext** - Context detection
5. **TestAIServiceMock** - Mock AI service
6. **TestAIServiceStreaming** - Streaming responses
7. **TestFileOperationsMock** - Mock file operations

## Expected Test Results

All tests should pass:
```
Running 7 test cases...

*** No errors detected
```

## Troubleshooting

### Build Errors

**Error: Cannot find ai_chat_plugin**
- Ensure `plugins/CMakeLists.txt` includes `add_subdirectory( ai_chat )`
- Check that `plugins/ai_chat/CMakeLists.txt` exists

**Error: Missing dependencies**
- Verify all KiCad dependencies are built
- Check that `kicad_curl` is available

### Test Failures

**Tests fail with "No frame available"**
- Mock frame initialization issue
- Check MOCK_EDA_FRAME implementation

**AI service tests fail**
- Expected - these use mocks, not real Ollama
- Verify MOCK_AI_SERVICE is working correctly

## Coverage Report Generation

```bash
# Build with coverage
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DKICAD_BUILD_QA_TESTS=ON \
      -DCMAKE_CXX_FLAGS="--coverage -g -O0" \
      -DCMAKE_C_FLAGS="--coverage -g -O0" ..

make qa_plugins

# Run tests
./qa/tests/plugins/qa_plugins

# Generate report (requires lcov)
lcov --capture --directory . --output-file coverage.info \
     --include "*/plugins/ai_chat/*"
genhtml coverage.info --output-directory coverage_html
```

## Continuous Integration

For CI/CD pipelines:

```yaml
- name: Build and Test
  run: |
    cmake -DKICAD_BUILD_QA_TESTS=ON ..
    make qa_plugins
    ./qa/tests/plugins/qa_plugins --log_level=test_suite
```

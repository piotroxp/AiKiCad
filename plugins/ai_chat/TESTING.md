# Testing Guide for AI Chat Plugin

## Running Tests

### Prerequisites

1. Build KiCad with QA tests enabled:
   ```bash
   cmake -DKICAD_BUILD_QA_TESTS=ON ..
   make
   ```

2. Run the plugin tests:
   ```bash
   cd build/qa/tests/plugins
   ./qa_plugins
   ```

   Or use CTest:
   ```bash
   cd build
   ctest -R qa_plugins -V
   ```

### Test Coverage

The test suite covers:

1. **Command Processor Tests**:
   - `TestCommandProcessorBasicParsing` - Tests basic command parsing
   - `TestCommandProcessorIdempotentOperations` - Verifies idempotent behavior
   - `TestCommandProcessorTraceParsing` - Tests trace command parsing
   - `TestCommandProcessorContext` - Tests context detection

2. **AI Service Tests**:
   - `TestAIServiceMock` - Tests mock AI service
   - `TestAIServiceStreaming` - Tests streaming response handling

3. **File Operations Tests**:
   - `TestFileOperationsMock` - Tests mockable file operations

### Coverage Report

To generate coverage reports:

```bash
# Build with coverage flags
cmake -DCMAKE_BUILD_TYPE=Debug -DKICAD_BUILD_QA_TESTS=ON \
      -DCMAKE_CXX_FLAGS="--coverage" \
      -DCMAKE_C_FLAGS="--coverage" ..

make

# Run tests
cd build/qa/tests/plugins
./qa_plugins

# Generate coverage report (requires lcov)
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

### Expected Coverage

The tests should cover:
- ✅ Command parsing logic
- ✅ AI service interface (via mocks)
- ✅ File operations (via mocks)
- ✅ Idempotent operations
- ✅ Context detection
- ✅ Error handling

### Model Configuration

The plugin defaults to `qwen2.5-coder:32b` but will auto-detect and prefer code-focused models:
- `qwen2.5-coder:32b` (preferred)
- `hf.co/bigatuna/NousCoder-14B-GGUF:Q4_K_M` (fallback)
- First available model if no code models found

To test with your models:
```bash
# Ensure Ollama is running
ollama serve

# The plugin will auto-detect available models
```

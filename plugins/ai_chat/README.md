# AI Chat Plugin for KiCad

A contextual AI-assisted design plugin that embeds a chat window into KiCad's main interfaces (Eeschema, Pcbnew, and Footprint Editor). Enables natural language commands for design actions directly on open files.

## Features

- **Dockable Chat Panel**: Embedded chat window using wxAuiManager, dockable and resizable
- **AI-Powered**: Integration with Ollama/llama.cpp for intelligent command understanding
- **Context-Aware Commands**: Understands current editor context (schematic, board, footprint)
- **Natural Language Processing**: Parse commands like "add component R1 at 100,200" or ask questions
- **Streaming Responses**: Real-time AI response streaming (when supported)
- **Idempotent Operations**: Repeated commands yield consistent results
- **Mockable APIs**: File operations and AI service abstracted for comprehensive testing

## Prerequisites

### Ollama Installation

The plugin uses Ollama for AI model orchestration. Install Ollama:

1. **Download Ollama**: https://ollama.ai
2. **Install and start Ollama service**
3. **Pull a model** (e.g., `ollama pull llama3.2`)

The plugin defaults to `llama3.2` but can use any Ollama-compatible model.

## Architecture

### Components

1. **PANEL_AI_CHAT**: Main UI component with chat history and input field
2. **AI_COMMAND_PROCESSOR**: Processes natural language commands and executes design actions
3. **I_AI_SERVICE / OLLAMA_AI_SERVICE**: AI service interface with Ollama implementation
4. **AI_CHAT_PLUGIN**: Plugin manager that handles registration and lifecycle
5. **I_FILE_OPERATIONS**: Abstract interface for file operations (mockable for tests)

### AI Service Integration

The plugin uses Ollama's REST API to communicate with local AI models:

- **Base URL**: `http://localhost:11434` (default Ollama port)
- **API Endpoints**:
  - `/api/generate` - Generate responses (supports streaming)
  - `/api/tags` - List available models
- **Models**: Any Ollama-compatible model (llama3.2, mistral, codellama, etc.)

### Integration

The plugin integrates into KiCad frames via `RegisterAIChatPlugin()` function. Call this after AUI initialization in frame constructors:

```cpp
#include <plugins/ai_chat/ai_chat_integration.h>

// In frame constructor, after FinishAUIInitialization()
RegisterAIChatPlugin( this );
```

## Usage

### Starting Ollama

Before using the plugin, ensure Ollama is running:

```bash
# Start Ollama service (usually runs automatically after installation)
ollama serve

# Pull a model if needed
ollama pull llama3.2
```

### Supported Commands

The plugin supports both direct commands and natural language queries:

#### Direct Commands (Parsed)
- `add component <name> [at <x>,<y>]` - Add a component to the schematic
- `modify component <refdes>` - Modify component properties
- `add trace from <x1>,<y1> to <x2>,<y2> [width <w>]` - Add a trace between points
- `modify footprint <name>` - Modify footprint properties

#### Natural Language (AI-Powered)
- "How do I add a resistor?"
- "What's the best way to route this trace?"
- "Explain this schematic"
- "Help me design a power supply circuit"

#### Generic
- `help` or `?` - Show available commands

### Example Interactions

```
User: add component R1 at 100,200
AI: Would add component 'R1' at (100, 200)

User: How do I add a capacitor?
AI: To add a capacitor in KiCad, you can use the Place Symbol tool...
[AI provides detailed instructions]

User: add trace from 0,0 to 100,100 width 10
AI: Would add trace from (0, 0) to (100, 100) width 10
```

## Configuration

### Model Selection

The plugin defaults to `llama3.2`. To use a different model:

1. Ensure the model is available in Ollama: `ollama list`
2. The plugin will auto-detect available models
3. Model selection can be configured in plugin settings (future enhancement)

### Ollama URL

Default: `http://localhost:11434`

To use a remote Ollama instance, modify the service initialization (future: settings dialog).

## Building

The plugin is built as part of KiCad's build system. Ensure the `ai_chat` subdirectory is included in `plugins/CMakeLists.txt`.

### Dependencies

- wxWidgets (HTTP support)
- nlohmann/json (JSON parsing)
- Ollama service (runtime dependency)

## Testing

Unit tests are located in `qa/tests/plugins/test_ai_chat_plugin.cpp`. Tests cover:

- Command parsing
- Idempotent operations
- Mock file operations
- Mock AI service
- Integration with frames

Run tests with:
```bash
cd qa
./tests/plugins/test_ai_chat_plugin
```

### Mock AI Service

Tests use `MOCK_AI_SERVICE` to avoid requiring Ollama during testing. The mock service provides predictable responses for testing command processing logic.

## Design Principles

1. **Idempotency**: All operations are idempotent - repeated commands produce consistent results
2. **Testability**: File operations and AI service are abstracted via interfaces for mocking
3. **Context Awareness**: Commands are routed based on current editor context
4. **UI Integration**: Follows KiCad's wxWidgets and AUI patterns
5. **AI-First**: AI service is primary, with direct command parsing as fallback

## Future Enhancements

- [ ] Settings dialog for Ollama URL and model selection
- [ ] Full component library integration for actual component placement
- [ ] Trace routing with actual board connectivity
- [ ] Footprint modification with real footprint editing
- [ ] Command history and undo/redo support
- [ ] Context extraction from open files (component lists, net names, etc.)
- [ ] Multi-model support with model switching
- [ ] Response caching for common queries
- [ ] Code generation for custom scripts

## Troubleshooting

### "Ollama service is not available"

1. Check if Ollama is running: `ollama list`
2. Verify Ollama is accessible at `http://localhost:11434`
3. Check firewall settings if using remote Ollama
4. Review KiCad console/logs for connection errors

### Slow Responses

- Use smaller/faster models (e.g., `llama3.2:1b` instead of `llama3.2`)
- Ensure Ollama has sufficient resources
- Check network latency if using remote Ollama

### Model Not Found

- Pull the model: `ollama pull <model-name>`
- Check available models: `ollama list`
- Verify model name matches exactly

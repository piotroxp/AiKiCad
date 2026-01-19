# AI Chat Design System for KiCad

## Executive Summary
This document defines the requirements, architecture, and implementation guidelines for an AI-powered chat assistant integrated into KiCad's EDA tools (Eeschema, Pcbnew, and Footprint Editor).

## Design Goals
1. **Reliability**: Every AI-generated design action must be verifiable and reversible
2. **Safety**: No action without explicit user confirmation for destructive operations
3. **Context Awareness**: AI understands the current design state, libraries, and project context
4. **Transparency**: Every AI decision is logged and explained
5. **Extensibility**: Support for multiple AI backends (Ollama, OpenAI, Claude, local models)

## Architecture Principles

### 1. Component Library Access
**Requirement**: AI must correctly load and place symbols from project and global libraries.

**Implementation**:
- Use `SYMBOL_LIBRARY_ADAPTER` to access symbol libraries
- Verify library existence before attempting load
- Cache library metadata for faster access
- Log all library access attempts for debugging

### 2. Command Parsing and Execution
**Requirement**: Natural language commands map to deterministic design actions.

**Supported Commands**:
```
- "add component <lib>:<symbol> at <x>,<y>"
- "connect <ref1>.<pin1> to <ref2>.<pin2>"
- "add trace from <x1>,<y1> to <x2>,<y2> width <w>"
- "list components" - Show placed components
- "list libraries" - Show available symbol libraries
- "query <search_term>" - Search components/footprints
- "help" - Show available commands
```

### 3. Design Safety
**Requirement**: Destructive actions require explicit confirmation.

**Implementation**:
- All wire/component additions are safe (additive)
- Modifications (value changes) require confirmation dialog
- Deletions require explicit user approval
- All actions logged with timestamp and context

### 4. Context Gathering
**Requirement**: AI has complete view of design state.

**Context Provided to AI**:
- Current editor type (schematic/board/footprint)
- Placed components with reference designators
- Available symbol and footprint libraries
- Current net connections (schematic)
- Track/via placements (PCB)
- Project file path and settings

## Technical Requirements

### 1. Plugin Integration
- Lazy panel creation (defer until first show)
- Proper AUI pane management
- Clean frame lifecycle handling
- Memory management without leaks

### 2. AI Service Abstraction
```cpp
class I_AI_SERVICE {
    virtual AI_RESPONSE ProcessPrompt(const wxString& prompt, 
                                       const AI_CONTEXT& context) = 0;
    virtual bool IsAvailable() const = 0;
    virtual std::vector<wxString> GetAvailableModels() const = 0;
};
```

### 3. Ollama Integration
- Default backend: Ollama with qwen2.5-coder model
- Streaming response support
- Connection health checks
- Automatic model selection

## Implementation Checklist

### Phase 1: Core Infrastructure [ ]
- [ ] AI Chat panel with chat history and input
- [ ] Command processor with regex-based parsing
- [ ] Ollama AI service implementation
- [ ] Plugin integration framework

### Phase 2: Eeschema Integration [ ]
- [ ] Register AI Chat in SCH_EDIT_FRAME
- [ ] Menu item in View → Panels
- [ ] Keyboard shortcut (Ctrl+Shift+A)
- [ ] Symbol library access
- [ ] Component placement
- [ ] Wire connection

### Phase 3: Pcbnew Integration [ ]
- [ ] Register AI Chat in PCB_EDIT_FRAME
- [ ] Menu item in View → Panels
- [ ] Keyboard shortcut (Ctrl+Shift+A)
- [ ] Footprint library access
- [ ] Track placement (stub implementation)

### Phase 4: Footprint Editor Integration [ ]
- [ ] Register AI Chat in FOOTPRINT_EDIT_FRAME
- [ ] Menu item in View → Panels
- [ ] Footprint library access

### Phase 5: Testing and Safety [ ]
- [ ] Unit tests for command parsing
- [ ] Integration tests for all editors
- [ ] Safety confirmation dialogs
- [ ] Error handling and recovery
- [ ] Memory leak testing

## Code Quality Standards

### 1. Error Handling
- No silent failures - log all errors
- User-friendly error messages
- Graceful degradation when AI unavailable
- Fallback to direct command parsing

### 2. Logging
```cpp
wxLogDebug(wxT("AI Chat: Loading symbol '%s' from library '%s'"), 
           symbolName, libName);
```

### 3. Testing Requirements
- Minimum 80% code coverage for command processor
- Mock AI service for deterministic testing
- Integration tests for each editor type
- Memory leak detection with Valgrind

## Review Checklist

### Code Review [ ]
- [ ] All public methods have documentation
- [ ] No memory leaks (RAII patterns)
- [ ] Thread-safe where required
- [ ] Consistent with KiCad coding style
- [ ] No hardcoded paths or magic numbers

### Design Review [ ]
- [ ] User confirmation for destructive actions
- [ ] Clear visual feedback for AI actions
- [ ] Accessible UI (keyboard navigation)
- [ ] Internationalization support
- [ ] Performance with large designs

### Security Review [ ]
- [ ] No network calls without user consent
- [ ] Input sanitization for prompts
- [ ] No sensitive data in AI context
- [ ] Local model support (privacy)

## User Documentation

### Getting Started
1. Install Ollama: `curl -s https://ollama.ai/install.sh | sh`
2. Pull model: `ollama pull qwen2.5-coder:32b`
3. Start Ollama: `ollama serve`
4. Open KiCad and use Ctrl+Shift+A to show AI Chat

### Commands
See "Supported Commands" section above for complete reference.

## Performance Requirements
- Panel creation: < 100ms
- Command parsing: < 10ms
- AI response (local Ollama): < 5s
- Memory footprint: < 10MB additional

## Future Enhancements
- Voice input support
- Design rule checking via AI
- Automatic BOM generation
- PCB routing suggestions
- Multi-language support
- Cloud AI backend integration

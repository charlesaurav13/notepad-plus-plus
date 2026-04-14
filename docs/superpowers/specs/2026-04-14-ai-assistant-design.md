# AI Assistant Integration — Design Spec
**Date:** 2026-04-14  
**Branch:** claude-develop  
**Phase:** 1

---

## Overview

Integrate a local Ollama LLM into Notepad++ as a first-class AI assistant for Windows. The assistant runs entirely offline using a locally downloaded model (default: `deepseek-r1:7b`), communicates with Ollama's REST API at `http://localhost:11434`, and provides three features in Phase 1:

- **A) Context Menu AI Actions** — right-click selected text to explain, fix, refactor, summarize
- **B) Side Panel Chat** — persistent dockable chat window with full conversation history
- **C) Inline Completions** — ghost text suggestions as you type, accepted with Tab

Platform: **Windows only**, Win32 C++, no cross-platform abstractions.

---

## Architecture

### New Module: `PowerEditor/src/AIAssistant/`

```
AIAssistant/
├── OllamaClient.h/.cpp      — WinHTTP-based HTTP client for Ollama REST API
├── AIAssistant.h/.cpp       — Main coordinator, owns all sub-components
├── AIPanel.h/.cpp           — Dockable side chat panel (Win32 dialog)
├── InlineCompleter.h/.cpp   — Inline completion logic and ghost text rendering
└── AISettings.h/.cpp        — Config: model name, endpoint, hotkeys, delays
```

### Hooks into Existing Core Files

| File | Change |
|------|--------|
| `Notepad_plus.h` | Add `AIAssistant* _pAIAssistant` member |
| `Notepad_plus.cpp` | Init/destroy AIAssistant, wire context menu items |
| `NppNotification.cpp` | Forward `SCN_CHARADDED` to `InlineCompleter` |
| `NppCommands.cpp` | Add command handlers for AI context menu actions |
| `Parameters.cpp` | Persist AI settings in npp config XML |
| `menuCmdID.h` | Add AI command IDs |

All AI logic stays inside `AIAssistant/`. Core files get minimal, targeted changes only.

---

## Feature A: Context Menu AI Actions

### User Flow
1. User selects text in editor → right-clicks
2. Context menu shows **AI Assistant ▶** submenu with actions:
   - Explain Selection
   - Fix / Debug
   - Refactor
   - Summarize
   - Custom Prompt...
3. Handler in `NppCommands.cpp` grabs selected text via `SCI_GETSELTEXT`
4. Builds a prompt and calls `OllamaClient::generate()` on a background thread
5. Response posted back to UI thread via `PostMessage`
6. Floating result dialog appears — resizable, copyable, with **Insert at Cursor** and **Replace Selection** buttons

### Ollama API Call
```
POST http://localhost:11434/api/generate
{
  "model": "deepseek-r1:7b",
  "prompt": "Explain this code:\n<selection>",
  "stream": false
}
```

---

## Feature B: Side Panel Chat

### UI Layout
```
┌─────────────────────────────┐
│  AI Assistant    [Model: ▼] │
│─────────────────────────────│
│  (chat history - RichEdit)  │
│                             │
│  You: explain this func     │
│  AI: This function sorts... │
│─────────────────────────────│
│ [✓] Include selection       │
│ [✓] Include current file    │
│─────────────────────────────│
│  Type a message...    [Send]│
└─────────────────────────────┘
```

### Implementation Details
- `AIPanel` extends `DockingDlgInterface` (same system as Function List, Doc Map)
- Chat history: Win32 `RichEdit` control (scrollable, formatted text)
- Input: multiline `Edit` control; `Enter` sends, `Shift+Enter` inserts newline
- Model dropdown: populated at startup via `GET /api/tags` from Ollama
- Context checkboxes append selection or full file content to the prompt
- Streaming responses (`"stream": true`): text appears word-by-word using WinHTTP streaming
- Conversation history stored in-memory as `vector<pair<string, string>>` (role, content)

---

## Feature C: Inline Completions

### User Flow
1. User types → `SCN_CHARADDED` fires → forwarded to `InlineCompleter`
2. **Auto mode:** 500ms debounce timer; on expiry, grab ~10 lines of context around cursor, call Ollama
3. **Manual mode:** `Ctrl+Space` bypasses debounce, fires immediately
4. Response arrives on background thread → ghost text rendered via `SCI_ANNOTATIONSETTEXT`
5. **Tab** — accepts completion: inserts text at cursor, clears annotation
6. **Escape** — dismisses ghost text without inserting
7. Any other keystroke — cancels pending request and clears ghost text

### Settings

| Setting | Default |
|---------|---------|
| Model | `deepseek-r1:7b` |
| Ollama endpoint | `http://localhost:11434` |
| Auto-complete delay | `500ms` |
| Max completion tokens | `100` |
| Enable auto-complete | `on` |

---

## OllamaClient Design

- Uses **WinHTTP** (Windows built-in, zero extra dependencies)
- Two modes:
  - `generate(prompt, callback)` — async, fires callback on UI thread when done
  - `generateStream(prompt, chunkCallback)` — streaming, fires callback per chunk
- Background thread pool (max 2 threads) to avoid blocking the UI
- Timeout: 30s for non-streaming, no timeout for streaming
- Error handling: if Ollama is not running, show a non-intrusive status bar message ("AI: Ollama not found — start Ollama to enable AI features")

---

## Settings & Configuration

AI settings stored in Notepad++ config XML (`config.xml`) under a new `<AIAssistant>` node:

```xml
<AIAssistant>
    <model>deepseek-r1:7b</model>
    <endpoint>http://localhost:11434</endpoint>
    <autoCompleteEnabled>true</autoCompleteEnabled>
    <autoCompleteDelay>500</autoCompleteDelay>
    <maxCompletionTokens>100</maxCompletionTokens>
</AIAssistant>
```

Configurable via **Settings → Preferences → AI Assistant** (new tab).

---

## Out of Scope (Phase 1)

- Multi-model simultaneous queries
- Cloud API fallback (OpenAI, Claude API)
- Code execution / sandboxing
- File-tree context (only current file/selection)
- macOS/Linux support

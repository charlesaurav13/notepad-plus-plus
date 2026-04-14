# AI Assistant Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate a local Ollama LLM into Notepad++ providing right-click AI actions, a dockable chat panel, and inline ghost-text completions — all via WinHTTP to localhost:11434.

**Architecture:** A self-contained `AIAssistant/` module under `PowerEditor/src/` owns all AI logic. Core files (`Notepad_plus.cpp`, `NppNotification.cpp`, `NppCommands.cpp`, `Parameters.cpp`) receive minimal targeted hooks only. Background threads handle HTTP; results are posted back to the UI thread via `PostMessage`.

**Tech Stack:** C++17, Win32 API, WinHTTP (system library), Scintilla API, nlohmann/json (`PowerEditor/src/json/json.hpp`), pugixml (existing in repo), Visual Studio 2022 / MSVC

> **Windows only.** Must be compiled with MSVC on Windows. CMake target: `notepad++` in `PowerEditor/src/CMakeLists.txt`.

---

## File Map

### New Files
| File | Responsibility |
|------|---------------|
| `PowerEditor/src/AIAssistant/OllamaClient.h` | WinHTTP client interface |
| `PowerEditor/src/AIAssistant/OllamaClient.cpp` | WinHTTP sync + streaming HTTP calls |
| `PowerEditor/src/AIAssistant/AISettings.h` | Settings struct |
| `PowerEditor/src/AIAssistant/AISettings.cpp` | Read/write from `config.xml` via pugixml |
| `PowerEditor/src/AIAssistant/AIAssistant.h` | Main coordinator interface |
| `PowerEditor/src/AIAssistant/AIAssistant.cpp` | Coordinator: owns OllamaClient, AIPanel, InlineCompleter |
| `PowerEditor/src/AIAssistant/AIResultDlg.h` | Floating result dialog interface (Feature A) |
| `PowerEditor/src/AIAssistant/AIResultDlg.cpp` | Floating result dialog implementation |
| `PowerEditor/src/AIAssistant/AIPanel.h` | Dockable chat panel interface (Feature B) |
| `PowerEditor/src/AIAssistant/AIPanel.cpp` | Dockable chat panel: RichEdit chat + streaming |
| `PowerEditor/src/AIAssistant/InlineCompleter.h` | Inline completion interface (Feature C) |
| `PowerEditor/src/AIAssistant/InlineCompleter.cpp` | Ghost text logic, debounce timer, Tab/Escape handling |

### Modified Files
| File | Change |
|------|--------|
| `PowerEditor/src/menuCmdID.h` | Add AI command IDs (`IDM + 9500` range) |
| `PowerEditor/src/resource.h` | Add `IDD_AI_RESULT`, `IDD_AI_PANEL`, `AI_USER` message base |
| `PowerEditor/src/Notepad_plus.h` | Add `AIAssistant* _pAIAssistant = nullptr` member |
| `PowerEditor/src/Notepad_plus.cpp` | Init/destroy AIAssistant, wire context menu |
| `PowerEditor/src/NppCommands.cpp` | Add AI command handlers |
| `PowerEditor/src/NppNotification.cpp` | Forward `SCN_CHARADDED` to `InlineCompleter` |
| `PowerEditor/src/Parameters.h` | Add `NppAISettings` struct inside `NppGUI` |
| `PowerEditor/src/Parameters.cpp` | Parse/save `<AIAssistant>` XML node |
| `PowerEditor/src/CMakeLists.txt` | Add all new `.cpp` files + `winhttp.lib` |

---

## Task 1: Scaffolding — IDs, CMakeLists, resource stubs

**Files:**
- Modify: `PowerEditor/src/menuCmdID.h`
- Modify: `PowerEditor/src/resource.h`
- Modify: `PowerEditor/src/CMakeLists.txt`
- Create: `PowerEditor/src/AIAssistant/` (directory)

- [ ] **Step 1: Add AI command IDs to `menuCmdID.h`**

Append before the final `#endif`:

```cpp
// AI Assistant commands
#define IDM_AI                   (IDM + 9500)
#define IDM_AI_PANEL             (IDM_AI + 1)   // toggle side panel
#define IDM_AI_EXPLAIN           (IDM_AI + 2)   // explain selection
#define IDM_AI_FIX               (IDM_AI + 3)   // fix/debug selection
#define IDM_AI_REFACTOR          (IDM_AI + 4)   // refactor selection
#define IDM_AI_SUMMARIZE         (IDM_AI + 5)   // summarize selection
#define IDM_AI_CUSTOMPROMPT      (IDM_AI + 6)   // custom prompt dialog
```

- [ ] **Step 2: Add resource and message IDs to `resource.h`**

Append before the final `#endif`:

```cpp
// AI Assistant dialogs
#define IDD_AI_RESULT_DLG        6500
#define IDD_AI_PANEL_DLG         6600

// AI Assistant controls
#define IDC_AI_RESULT_EDIT       6501
#define IDC_AI_INSERT_BTN        6502
#define IDC_AI_REPLACE_BTN       6503
#define IDC_AI_CLOSE_BTN         6504

#define IDC_AI_CHAT_HISTORY      6601
#define IDC_AI_INPUT_EDIT        6602
#define IDC_AI_SEND_BTN          6603
#define IDC_AI_MODEL_COMBO       6604
#define IDC_AI_INCL_SEL_CHK      6605
#define IDC_AI_INCL_FILE_CHK     6606

// AI Assistant WM_USER messages
#define AI_USER                  (WM_USER + 8000)
#define AI_MSG_RESULT            (AI_USER + 1)   // LPARAM = heap AIResult*
#define AI_MSG_STREAM_CHUNK      (AI_USER + 2)   // LPARAM = heap std::string*
#define AI_MSG_STREAM_DONE       (AI_USER + 3)
#define AI_MSG_ERROR             (AI_USER + 4)   // LPARAM = heap std::string* (error msg)
```

- [ ] **Step 3: Register new `.cpp` files in `CMakeLists.txt`**

In `PowerEditor/src/CMakeLists.txt`, find the block where `.cpp` files are listed (around line 68). Add these lines in the same block:

```cmake
./AIAssistant/OllamaClient.cpp
./AIAssistant/AISettings.cpp
./AIAssistant/AIAssistant.cpp
./AIAssistant/AIResultDlg.cpp
./AIAssistant/AIPanel.cpp
./AIAssistant/InlineCompleter.cpp
```

Then find the `target_link_libraries` call for the notepad++ target and add `winhttp`:

```cmake
target_link_libraries(notepad++ PRIVATE winhttp)
```

- [ ] **Step 4: Create the AIAssistant directory**

```
mkdir PowerEditor\src\AIAssistant
```

- [ ] **Step 5: Verify CMake configuration succeeds**

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

Expected: configuration completes without errors (link errors for missing `.cpp` bodies are fine at this stage).

- [ ] **Step 6: Commit**

```bash
git add PowerEditor/src/menuCmdID.h PowerEditor/src/resource.h PowerEditor/src/CMakeLists.txt
git commit -m "feat: scaffold AIAssistant module — IDs, CMakeLists, resource stubs"
```

---

## Task 2: AISettings — config struct + XML read/write

**Files:**
- Create: `PowerEditor/src/AIAssistant/AISettings.h`
- Create: `PowerEditor/src/AIAssistant/AISettings.cpp`
- Modify: `PowerEditor/src/Parameters.h`
- Modify: `PowerEditor/src/Parameters.cpp`

- [ ] **Step 1: Create `AISettings.h`**

```cpp
// PowerEditor/src/AIAssistant/AISettings.h
#pragma once
#include <string>

struct NppAISettings
{
    std::wstring model       = L"deepseek-r1:7b";
    std::wstring endpoint    = L"http://localhost:11434";
    bool  autoCompleteOn     = true;
    int   autoCompleteDelayMs = 500;
    int   maxTokens          = 100;
};
```

- [ ] **Step 2: Add `NppAISettings` to `NppGUI` in `Parameters.h`**

Open `PowerEditor/src/Parameters.h`. Find `struct NppGUI final` (line ~706). Add the include and member inside the struct:

```cpp
// At the top of Parameters.h with other includes
#include "AIAssistant/AISettings.h"
```

Then inside `struct NppGUI final { ... }`, add at the end before the closing `}`:

```cpp
    NppAISettings _aiSettings;
```

- [ ] **Step 3: Create `AISettings.cpp`**

```cpp
// PowerEditor/src/AIAssistant/AISettings.cpp
#include "AISettings.h"
// (no implementation needed — plain struct, XML handled in Parameters.cpp)
```

- [ ] **Step 4: Add XML read/write to `Parameters.cpp`**

Open `PowerEditor/src/Parameters.cpp`. Find the function that reads `NppGUI` settings from XML (search for `getNppGUI` or where `_nppGUI` is populated — look for `stylers`, `tabStatus` etc.). Add at the end of that reading block:

```cpp
// Read AI settings
pugi::xml_node aiNode = nppGuiRoot.child(L"AIAssistant");
if (aiNode)
{
    if (auto n = aiNode.child(L"model"); n)
        _nppGUI._aiSettings.model = n.text().as_string(L"deepseek-r1:7b");
    if (auto n = aiNode.child(L"endpoint"); n)
        _nppGUI._aiSettings.endpoint = n.text().as_string(L"http://localhost:11434");
    if (auto n = aiNode.child(L"autoCompleteEnabled"); n)
        _nppGUI._aiSettings.autoCompleteOn = wcscmp(n.text().as_string(L"true"), L"true") == 0;
    if (auto n = aiNode.child(L"autoCompleteDelay"); n)
        _nppGUI._aiSettings.autoCompleteDelayMs = n.text().as_int(500);
    if (auto n = aiNode.child(L"maxTokens"); n)
        _nppGUI._aiSettings.maxTokens = n.text().as_int(100);
}
```

Find the function that writes `NppGUI` settings (search for where `tabStatus` or `appPos` is written). Add:

```cpp
// Write AI settings
pugi::xml_node aiNode2 = nppGuiRoot.append_child(L"AIAssistant");
aiNode2.append_child(L"model").text().set(_nppGUI._aiSettings.model.c_str());
aiNode2.append_child(L"endpoint").text().set(_nppGUI._aiSettings.endpoint.c_str());
aiNode2.append_child(L"autoCompleteEnabled").text().set(_nppGUI._aiSettings.autoCompleteOn ? L"true" : L"false");
aiNode2.append_child(L"autoCompleteDelay").text().set(_nppGUI._aiSettings.autoCompleteDelayMs);
aiNode2.append_child(L"maxTokens").text().set(_nppGUI._aiSettings.maxTokens);
```

- [ ] **Step 5: Build to verify no compile errors**

```
cmake --build build --config Release --target notepad++ 2>&1 | findstr /i "error"
```

Expected: no errors.

- [ ] **Step 6: Commit**

```bash
git add PowerEditor/src/AIAssistant/AISettings.h PowerEditor/src/AIAssistant/AISettings.cpp PowerEditor/src/Parameters.h PowerEditor/src/Parameters.cpp
git commit -m "feat: add NppAISettings struct and config XML read/write"
```

---

## Task 3: OllamaClient — WinHTTP sync + streaming

**Files:**
- Create: `PowerEditor/src/AIAssistant/OllamaClient.h`
- Create: `PowerEditor/src/AIAssistant/OllamaClient.cpp`

- [ ] **Step 1: Create `OllamaClient.h`**

```cpp
// PowerEditor/src/AIAssistant/OllamaClient.h
#pragma once
#include <string>
#include <windows.h>

// Posted to hwndNotify when a generate() call completes.
// Caller must delete the AIResult* received in LPARAM.
struct AIResult {
    std::string  response;
    bool         success = false;
    std::string  error;
};

class OllamaClient
{
public:
    OllamaClient(const std::wstring& endpoint, const std::wstring& model);
    ~OllamaClient() = default;

    // Async: fires AI_MSG_RESULT or AI_MSG_ERROR to hwndNotify on completion.
    // LPARAM of AI_MSG_RESULT = heap-allocated AIResult* (caller must delete).
    void generate(const std::string& prompt, HWND hwndNotify);

    // Async streaming: fires AI_MSG_STREAM_CHUNK per token, then AI_MSG_STREAM_DONE.
    // LPARAM of AI_MSG_STREAM_CHUNK = heap-allocated std::string* (caller must delete).
    void generateStream(const std::string& prompt, HWND hwndNotify);

    // Synchronous — call only from a background thread.
    AIResult generateSync(const std::string& prompt);

    // Check if Ollama is reachable (synchronous, fast).
    bool isAvailable();

    void setModel(const std::wstring& model)    { _model = model; }
    void setEndpoint(const std::wstring& ep)    { _endpoint = ep; }
    const std::wstring& getModel() const        { return _model; }

    // Returns list of model names from GET /api/tags
    std::vector<std::wstring> listModels();

// Note: wstrToUtf8 and utf8ToWstr must be declared PUBLIC in the class
// so AIAssistant.cpp can call OllamaClient::wstrToUtf8(...) directly.

private:
    std::wstring _endpoint;  // e.g. L"http://localhost:11434"
    std::wstring _model;     // e.g. L"deepseek-r1:7b"

    struct ThreadParam {
        OllamaClient* client;
        std::string   prompt;
        HWND          hwndNotify;
        bool          stream;
    };

    static DWORD WINAPI workerThread(LPVOID param);

    // Parses _endpoint into host, port, base path
    void parseEndpoint(std::wstring& outHost, INTERNET_PORT& outPort) const;

    // Synchronous POST — returns raw response body
    std::string httpPost(const std::wstring& host, INTERNET_PORT port,
                         const std::wstring& path, const std::string& body,
                         bool streaming, HWND hwndNotify);

    // UTF-8 <-> wstring helpers (public so AIAssistant.cpp can use them)
    static std::string  wstrToUtf8(const std::wstring& w);
    static std::wstring utf8ToWstr(const std::string& s);
};
```

- [ ] **Step 2: Create `OllamaClient.cpp`**

```cpp
// PowerEditor/src/AIAssistant/OllamaClient.cpp
#include "OllamaClient.h"
#include "../resource.h"
#include "../json/json.hpp"
#include <winhttp.h>
#include <stdexcept>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

// ---------- helpers ----------

std::string OllamaClient::wstrToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(sz - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], sz, nullptr, nullptr);
    return s;
}

std::wstring OllamaClient::utf8ToWstr(const std::string& s)
{
    if (s.empty()) return {};
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(sz - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], sz);
    return w;
}

void OllamaClient::parseEndpoint(std::wstring& outHost, INTERNET_PORT& outPort) const
{
    // Strip "http://" or "https://"
    std::wstring ep = _endpoint;
    if (ep.substr(0, 7) == L"http://")  ep = ep.substr(7);
    if (ep.substr(0, 8) == L"https://") ep = ep.substr(8);

    auto colon = ep.find(L':');
    if (colon != std::wstring::npos) {
        outHost = ep.substr(0, colon);
        outPort = static_cast<INTERNET_PORT>(_wtoi(ep.substr(colon + 1).c_str()));
    } else {
        outHost = ep;
        outPort = 11434;
    }
}

// ---------- constructor ----------

OllamaClient::OllamaClient(const std::wstring& endpoint, const std::wstring& model)
    : _endpoint(endpoint), _model(model) {}

// ---------- availability check ----------

bool OllamaClient::isAvailable()
{
    std::wstring host; INTERNET_PORT port;
    parseEndpoint(host, port);

    HINTERNET hSess = WinHttpOpen(L"NppAI/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
    if (!hSess) return false;
    WinHttpSetTimeouts(hSess, 3000, 3000, 3000, 3000);

    HINTERNET hConn = WinHttpConnect(hSess, host.c_str(), port, 0);
    HINTERNET hReq  = hConn ? WinHttpOpenRequest(hConn, L"GET", L"/api/tags", nullptr, nullptr, nullptr, 0) : nullptr;
    bool ok = hReq && WinHttpSendRequest(hReq, nullptr, 0, nullptr, 0, 0, 0)
                   && WinHttpReceiveResponse(hReq, nullptr);

    if (hReq)  WinHttpCloseHandle(hReq);
    if (hConn) WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return ok;
}

// ---------- listModels ----------

std::vector<std::wstring> OllamaClient::listModels()
{
    std::vector<std::wstring> models;
    std::wstring host; INTERNET_PORT port;
    parseEndpoint(host, port);

    HINTERNET hSess = WinHttpOpen(L"NppAI/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
    if (!hSess) return models;
    WinHttpSetTimeouts(hSess, 5000, 5000, 5000, 5000);

    HINTERNET hConn = WinHttpConnect(hSess, host.c_str(), port, 0);
    HINTERNET hReq  = hConn ? WinHttpOpenRequest(hConn, L"GET", L"/api/tags", nullptr, nullptr, nullptr, 0) : nullptr;

    if (hReq && WinHttpSendRequest(hReq, nullptr, 0, nullptr, 0, 0, 0)
             && WinHttpReceiveResponse(hReq, nullptr))
    {
        std::string body;
        DWORD bytesRead = 0;
        char buf[4096];
        while (WinHttpReadData(hReq, buf, sizeof(buf), &bytesRead) && bytesRead > 0)
            body.append(buf, bytesRead);

        try {
            auto j = json::parse(body);
            for (auto& m : j["models"])
                models.push_back(utf8ToWstr(m["name"].get<std::string>()));
        } catch (...) {}
    }

    if (hReq)  WinHttpCloseHandle(hReq);
    if (hConn) WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return models;
}

// ---------- httpPost (sync, used by workerThread) ----------

std::string OllamaClient::httpPost(const std::wstring& host, INTERNET_PORT port,
                                    const std::wstring& path, const std::string& body,
                                    bool streaming, HWND hwndNotify)
{
    HINTERNET hSess = WinHttpOpen(L"NppAI/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
    if (!hSess) return {};
    WinHttpSetTimeouts(hSess, 10000, 10000, 30000, streaming ? 0 : 30000);

    HINTERNET hConn = WinHttpConnect(hSess, host.c_str(), port, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return {}; }

    HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", path.c_str(), nullptr, nullptr, nullptr, 0);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return {}; }

    std::wstring headers = L"Content-Type: application/json\r\n";
    WinHttpAddRequestHeaders(hReq, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL sent = WinHttpSendRequest(hReq, nullptr, 0,
                                   (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent || !WinHttpReceiveResponse(hReq, nullptr))
    {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
        return {};
    }

    std::string fullResponse;
    DWORD bytesRead = 0;
    char buf[4096];

    while (WinHttpReadData(hReq, buf, sizeof(buf), &bytesRead) && bytesRead > 0)
    {
        fullResponse.append(buf, bytesRead);

        if (streaming && hwndNotify)
        {
            // Parse NDJSON lines and post chunks
            std::istringstream ss(fullResponse);
            std::string line;
            std::string remaining;
            fullResponse.clear();

            while (std::getline(ss, line))
            {
                if (line.empty()) continue;
                try {
                    auto j = json::parse(line);
                    std::string chunk = j.value("response", "");
                    bool done = j.value("done", false);

                    if (!chunk.empty()) {
                        auto* pChunk = new std::string(chunk);
                        PostMessage(hwndNotify, AI_MSG_STREAM_CHUNK, 0, reinterpret_cast<LPARAM>(pChunk));
                    }
                    if (done) {
                        PostMessage(hwndNotify, AI_MSG_STREAM_DONE, 0, 0);
                        goto cleanup;
                    }
                } catch (...) {
                    remaining = line;
                }
            }
            fullResponse = remaining;
        }
    }

cleanup:
    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return fullResponse;
}

// ---------- generateSync ----------

AIResult OllamaClient::generateSync(const std::string& prompt)
{
    AIResult result;
    std::wstring host; INTERNET_PORT port;
    parseEndpoint(host, port);

    json req;
    req["model"]  = wstrToUtf8(_model);
    req["prompt"] = prompt;
    req["stream"] = false;

    std::string raw = httpPost(host, port, L"/api/generate", req.dump(), false, nullptr);
    if (raw.empty()) {
        result.error   = "No response from Ollama. Is it running?";
        result.success = false;
        return result;
    }

    try {
        auto j = json::parse(raw);
        result.response = j.value("response", "");
        result.success  = true;
    } catch (const std::exception& e) {
        result.error   = e.what();
        result.success = false;
    }
    return result;
}

// ---------- async worker thread ----------

DWORD WINAPI OllamaClient::workerThread(LPVOID param)
{
    auto* tp = reinterpret_cast<ThreadParam*>(param);
    OllamaClient* self = tp->client;
    std::string   prompt = tp->prompt;
    HWND          hwnd   = tp->hwndNotify;
    bool          stream = tp->stream;
    delete tp;

    std::wstring host; INTERNET_PORT port;
    self->parseEndpoint(host, port);

    json req;
    req["model"]  = wstrToUtf8(self->_model);
    req["prompt"] = prompt;
    req["stream"] = stream;

    if (stream) {
        self->httpPost(host, port, L"/api/generate", req.dump(), true, hwnd);
    } else {
        AIResult r = self->generateSync(prompt);
        auto* pResult = new AIResult(r);
        PostMessage(hwnd, r.success ? AI_MSG_RESULT : AI_MSG_ERROR, 0, reinterpret_cast<LPARAM>(pResult));
    }
    return 0;
}

void OllamaClient::generate(const std::string& prompt, HWND hwndNotify)
{
    auto* tp = new ThreadParam{ this, prompt, hwndNotify, false };
    HANDLE h = CreateThread(nullptr, 0, workerThread, tp, 0, nullptr);
    if (h) CloseHandle(h);
}

void OllamaClient::generateStream(const std::string& prompt, HWND hwndNotify)
{
    auto* tp = new ThreadParam{ this, prompt, hwndNotify, true };
    HANDLE h = CreateThread(nullptr, 0, workerThread, tp, 0, nullptr);
    if (h) CloseHandle(h);
}
```

- [ ] **Step 3: Build and verify no compile errors**

```
cmake --build build --config Release --target notepad++ 2>&1 | findstr /i "error"
```

Expected: no errors.

- [ ] **Step 4: Manual smoke test for OllamaClient**

With Ollama running locally and `deepseek-r1:7b` pulled:
1. Add a temporary test call in `winmain.cpp` `WinMain` (before the main message loop):

```cpp
OllamaClient client(L"http://localhost:11434", L"deepseek-r1:7b");
bool avail = client.isAvailable();  // should be true
AIResult r = client.generateSync("Say hello in one word.");
// r.success == true, r.response contains the model's reply
OutputDebugStringA(r.response.c_str());
```

2. Run in Debug build, attach debugger, verify `r.success == true`.
3. **Remove the test code** after verifying.

- [ ] **Step 5: Commit**

```bash
git add PowerEditor/src/AIAssistant/OllamaClient.h PowerEditor/src/AIAssistant/OllamaClient.cpp
git commit -m "feat: add OllamaClient with WinHTTP sync and streaming"
```

---

## Task 4: AIAssistant Coordinator + Core Init

**Files:**
- Create: `PowerEditor/src/AIAssistant/AIAssistant.h`
- Create: `PowerEditor/src/AIAssistant/AIAssistant.cpp`
- Modify: `PowerEditor/src/Notepad_plus.h`
- Modify: `PowerEditor/src/Notepad_plus.cpp`

- [ ] **Step 1: Create `AIAssistant.h`**

```cpp
// PowerEditor/src/AIAssistant/AIAssistant.h
#pragma once
#include "OllamaClient.h"
#include "AISettings.h"
#include <windows.h>
#include <memory>

class ScintillaEditView;
class AIPanel;
class InlineCompleter;

class AIAssistant
{
public:
    AIAssistant();
    ~AIAssistant();

    // Called once from Notepad_plus::init()
    void init(HINSTANCE hInst, HWND hNpp, ScintillaEditView** ppEditView);

    // Toggle dockable chat panel visibility
    void togglePanel();

    // Feature A: fire an AI action on the current selection
    // actionPromptPrefix e.g. "Explain this code:\n"
    void runSelectionAction(const std::wstring& actionPromptPrefix, HWND hwndParent);

    // Called by InlineCompleter to check if AI is enabled
    bool isAutoCompleteEnabled() const;

    // Refresh settings (called after Preferences dialog saves)
    void reloadSettings();

    OllamaClient* getClient() { return _client.get(); }
    AIPanel*      getPanel()  { return _panel.get(); }

private:
    std::unique_ptr<OllamaClient>    _client;
    std::unique_ptr<AIPanel>         _panel;
    std::unique_ptr<InlineCompleter> _completer;

    HINSTANCE           _hInst = nullptr;
    HWND                _hNpp  = nullptr;
    ScintillaEditView** _ppEditView = nullptr;
};
```

- [ ] **Step 2: Create `AIAssistant.cpp`**

```cpp
// PowerEditor/src/AIAssistant/AIAssistant.cpp
#include "AIAssistant.h"
#include "AIPanel.h"
#include "InlineCompleter.h"
#include "../Parameters.h"

AIAssistant::AIAssistant() = default;
AIAssistant::~AIAssistant() = default;

void AIAssistant::init(HINSTANCE hInst, HWND hNpp, ScintillaEditView** ppEditView)
{
    _hInst      = hInst;
    _hNpp       = hNpp;
    _ppEditView = ppEditView;

    const NppAISettings& s = NppParameters::getInstance().getNppGUI()._aiSettings;

    _client    = std::make_unique<OllamaClient>(s.endpoint, s.model);
    _panel     = std::make_unique<AIPanel>(_client.get());
    _completer = std::make_unique<InlineCompleter>(_client.get(), ppEditView);

    _panel->init(hInst, hNpp);
    _completer->init(hNpp);

    // Show non-intrusive status bar warning if Ollama is offline
    if (!_client->isAvailable())
        ::SendMessage(hNpp, WM_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"AI: Ollama not found — start Ollama to enable AI features"));
}

void AIAssistant::togglePanel()
{
    if (_panel) _panel->display(!_panel->isVisible());
}

void AIAssistant::runSelectionAction(const std::wstring& actionPromptPrefix, HWND hwndParent)
{
    if (!_ppEditView || !*_ppEditView) return;

    // Get selected text
    ScintillaEditView* view = *_ppEditView;
    auto selLen = view->execute(SCI_GETSELTEXT, 0, 0);
    if (selLen <= 1) {
        MessageBox(hwndParent, L"Please select some text first.", L"AI Assistant", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::string sel(selLen, '\0');
    view->execute(SCI_GETSELTEXT, 0, reinterpret_cast<LPARAM>(&sel[0]));
    sel.pop_back(); // remove null terminator

    // Build prompt
    std::string prompt = OllamaClient::wstrToUtf8(actionPromptPrefix) + sel;

    // Show result dialog (it registers itself as the notification target)
    // AIResultDlg is created per-call (modal-ish floating dialog)
    // See Task 5 for AIResultDlg implementation
    extern void showAIResultDlg(HINSTANCE, HWND, OllamaClient*, const std::string&, ScintillaEditView*);
    showAIResultDlg(_hInst, hwndParent, _client.get(), prompt, view);
}

bool AIAssistant::isAutoCompleteEnabled() const
{
    return NppParameters::getInstance().getNppGUI()._aiSettings.autoCompleteOn;
}

void AIAssistant::reloadSettings()
{
    const NppAISettings& s = NppParameters::getInstance().getNppGUI()._aiSettings;
    if (_client) {
        _client->setModel(s.model);
        _client->setEndpoint(s.endpoint);
    }
    if (_completer) _completer->reloadSettings();
}
```

- [ ] **Step 3: Add `AIAssistant*` member to `Notepad_plus.h`**

Open `PowerEditor/src/Notepad_plus.h`. At the top with other forward declarations (around line ~146 where `FunctionListPanel` is forward-declared), add:

```cpp
class AIAssistant;
```

Find the private members section where `_pFuncList` is declared (around line 421). Add:

```cpp
AIAssistant* _pAIAssistant = nullptr;
```

Find the private methods section and add:

```cpp
void launchAIPanel();
void runAIAction(const std::wstring& promptPrefix);
```

- [ ] **Step 4: Wire AIAssistant into `Notepad_plus.cpp`**

Open `PowerEditor/src/Notepad_plus.cpp`.

**4a. Add include near the top with other includes:**
```cpp
#include "AIAssistant/AIAssistant.h"
```

**4b. Find the destructor (search for `delete _pFuncList`). Add:**
```cpp
delete _pAIAssistant;
```

**4c. Find `Notepad_plus::init()` — after `_pFuncList` related init or near end of init. Add:**
```cpp
// Init AI Assistant
_pAIAssistant = new AIAssistant();
_pAIAssistant->init(_pPublicInterface->getHinst(), _pPublicInterface->getHSelf(), &_pEditView);
```

**4d. Add `launchAIPanel()` at the bottom of the file:**
```cpp
void Notepad_plus::launchAIPanel()
{
    if (_pAIAssistant)
        _pAIAssistant->togglePanel();
}
```

**4e. Add `runAIAction()` at the bottom:**
```cpp
void Notepad_plus::runAIAction(const std::wstring& promptPrefix)
{
    if (_pAIAssistant)
        _pAIAssistant->runSelectionAction(promptPrefix, _pPublicInterface->getHSelf());
}
```

- [ ] **Step 5: Build to verify no compile errors**

```
cmake --build build --config Release --target notepad++ 2>&1 | findstr /i "error"
```

- [ ] **Step 6: Commit**

```bash
git add PowerEditor/src/AIAssistant/AIAssistant.h PowerEditor/src/AIAssistant/AIAssistant.cpp PowerEditor/src/Notepad_plus.h PowerEditor/src/Notepad_plus.cpp
git commit -m "feat: add AIAssistant coordinator and wire into Notepad_plus init"
```

---

## Task 5: Feature A — Context Menu + Floating Result Dialog

**Files:**
- Create: `PowerEditor/src/AIAssistant/AIResultDlg.h`
- Create: `PowerEditor/src/AIAssistant/AIResultDlg.cpp`
- Modify: `PowerEditor/src/NppCommands.cpp`
- Modify: `PowerEditor/src/Notepad_plus.cpp` (context menu)

- [ ] **Step 1: Create `AIResultDlg.h`**

```cpp
// PowerEditor/src/AIAssistant/AIResultDlg.h
#pragma once
#include "OllamaClient.h"
#include <windows.h>

class ScintillaEditView;

// Floating resizable result dialog.
// Shows AI response with "Insert at Cursor" and "Replace Selection" buttons.
class AIResultDlg
{
public:
    AIResultDlg(HINSTANCE hInst, HWND hParent, OllamaClient* client,
                const std::string& prompt, ScintillaEditView* pView);
    ~AIResultDlg();

    void show(); // Creates and shows the dialog, fires async generate()

private:
    static INT_PTR CALLBACK dlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
    INT_PTR handleMsg(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

    void onResult(AIResult* pResult);
    void insertAtCursor(HWND hDlg);
    void replaceSelection(HWND hDlg);

    HINSTANCE          _hInst;
    HWND               _hParent;
    OllamaClient*      _client;
    std::string        _prompt;
    ScintillaEditView* _pView;
    HWND               _hDlg = nullptr;
    std::wstring       _resultText;
};

// Free function — used by AIAssistant::runSelectionAction
void showAIResultDlg(HINSTANCE hInst, HWND hParent, OllamaClient* client,
                     const std::string& prompt, ScintillaEditView* pView);
```

- [ ] **Step 2: Create `AIResultDlg.cpp`**

```cpp
// PowerEditor/src/AIAssistant/AIResultDlg.cpp
#include "AIResultDlg.h"
#include "../resource.h"
#include "../../ScintillaComponent/ScintillaEditView.h"
#include <commctrl.h>

// Helper: UTF-8 -> wstring
static std::wstring u8w(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

// Helper: wstring -> UTF-8
static std::string wu8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

AIResultDlg::AIResultDlg(HINSTANCE hInst, HWND hParent, OllamaClient* client,
                          const std::string& prompt, ScintillaEditView* pView)
    : _hInst(hInst), _hParent(hParent), _client(client), _prompt(prompt), _pView(pView) {}

AIResultDlg::~AIResultDlg()
{
    if (_hDlg) DestroyWindow(_hDlg);
}

void AIResultDlg::show()
{
    // Create dialog from template defined in resource script
    _hDlg = CreateDialogParam(_hInst, MAKEINTRESOURCE(IDD_AI_RESULT_DLG),
                               _hParent, dlgProc, reinterpret_cast<LPARAM>(this));
    if (!_hDlg) return;

    // Set "Loading..." placeholder
    SetDlgItemText(_hDlg, IDC_AI_RESULT_EDIT, L"Asking AI...");
    EnableWindow(GetDlgItem(_hDlg, IDC_AI_INSERT_BTN), FALSE);
    EnableWindow(GetDlgItem(_hDlg, IDC_AI_REPLACE_BTN), FALSE);

    ShowWindow(_hDlg, SW_SHOW);

    // Fire async request — result comes back as AI_MSG_RESULT / AI_MSG_ERROR
    _client->generate(_prompt, _hDlg);
}

INT_PTR CALLBACK AIResultDlg::dlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    AIResultDlg* self = nullptr;
    if (msg == WM_INITDIALOG) {
        self = reinterpret_cast<AIResultDlg*>(lp);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->_hDlg = hDlg;
    } else {
        self = reinterpret_cast<AIResultDlg*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
    }
    if (self) return self->handleMsg(hDlg, msg, wp, lp);
    return FALSE;
}

INT_PTR AIResultDlg::handleMsg(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case AI_MSG_RESULT:
    {
        auto* pResult = reinterpret_cast<AIResult*>(lp);
        if (pResult->success) {
            _resultText = u8w(pResult->response);
            SetDlgItemText(hDlg, IDC_AI_RESULT_EDIT, _resultText.c_str());
            EnableWindow(GetDlgItem(hDlg, IDC_AI_INSERT_BTN), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_AI_REPLACE_BTN), TRUE);
        } else {
            SetDlgItemText(hDlg, IDC_AI_RESULT_EDIT, u8w(pResult->error).c_str());
        }
        delete pResult;
        return TRUE;
    }
    case AI_MSG_ERROR:
    {
        auto* pErr = reinterpret_cast<AIResult*>(lp);
        SetDlgItemText(hDlg, IDC_AI_RESULT_EDIT, u8w(pErr->error).c_str());
        delete pErr;
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_AI_INSERT_BTN:   insertAtCursor(hDlg);   return TRUE;
        case IDC_AI_REPLACE_BTN:  replaceSelection(hDlg); return TRUE;
        case IDC_AI_CLOSE_BTN:
        case IDCANCEL:
            DestroyWindow(hDlg);
            _hDlg = nullptr;
            delete this; // self-owning
            return TRUE;
        }
        break;
    case WM_SIZE:
    {
        // Resize the edit control to fill the dialog
        RECT rc; GetClientRect(hDlg, &rc);
        HWND hEdit = GetDlgItem(hDlg, IDC_AI_RESULT_EDIT);
        // Leave 40px at bottom for buttons
        SetWindowPos(hEdit, nullptr, 5, 5, rc.right - 10, rc.bottom - 50, SWP_NOZORDER);
        return TRUE;
    }
    }
    return FALSE;
}

void AIResultDlg::insertAtCursor(HWND)
{
    if (!_pView || _resultText.empty()) return;
    std::string utf8 = wu8(_resultText);
    _pView->execute(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(utf8.c_str()));
}

void AIResultDlg::replaceSelection(HWND)
{
    if (!_pView || _resultText.empty()) return;
    std::string utf8 = wu8(_resultText);
    _pView->execute(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(utf8.c_str()));
}

void showAIResultDlg(HINSTANCE hInst, HWND hParent, OllamaClient* client,
                     const std::string& prompt, ScintillaEditView* pView)
{
    auto* dlg = new AIResultDlg(hInst, hParent, client, prompt, pView);
    dlg->show(); // self-deletes on close
}
```

- [ ] **Step 3: Add the dialog template to `Notepad_plus.rc`**

Open `PowerEditor/src/Notepad_plus.rc`. Find any existing `IDD_` dialog definition (e.g. search for `DIALOGEX`). Add after it:

```rc
IDD_AI_RESULT_DLG DIALOGEX 0, 0, 400, 300
STYLE DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME
CAPTION "AI Assistant"
FONT 9, "Segoe UI"
BEGIN
    EDITTEXT        IDC_AI_RESULT_EDIT, 5, 5, 390, 255, ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL
    PUSHBUTTON      "Insert at Cursor",  IDC_AI_INSERT_BTN,   5, 270, 90, 14
    PUSHBUTTON      "Replace Selection", IDC_AI_REPLACE_BTN, 100, 270, 90, 14
    PUSHBUTTON      "Close",             IDC_AI_CLOSE_BTN,   305, 270, 50, 14
END
```

- [ ] **Step 4: Wire AI context menu commands into `NppCommands.cpp`**

Open `PowerEditor/src/NppCommands.cpp`. Find the large `switch` on command ID (search for `case IDM_FILE_NEW:` or similar). Add new cases:

```cpp
case IDM_AI_EXPLAIN:
    runAIAction(L"Explain the following code in detail:\n\n");
    break;
case IDM_AI_FIX:
    runAIAction(L"Find and fix bugs in the following code. Show the corrected code and explain what you changed:\n\n");
    break;
case IDM_AI_REFACTOR:
    runAIAction(L"Refactor the following code for clarity and best practices:\n\n");
    break;
case IDM_AI_SUMMARIZE:
    runAIAction(L"Summarize what the following code does in 2-3 sentences:\n\n");
    break;
case IDM_AI_PANEL:
    launchAIPanel();
    break;
```

- [ ] **Step 5: Add AI submenu to context menu in `Notepad_plus.cpp`**

Search for the context menu creation in `Notepad_plus.cpp` (search for `TrackPopupMenu` or `CreatePopupMenu`). After the existing items are appended, add:

```cpp
// AI Assistant submenu
HMENU hAiMenu = CreatePopupMenu();
AppendMenu(hAiMenu, MF_STRING, IDM_AI_EXPLAIN,    L"Explain Selection");
AppendMenu(hAiMenu, MF_STRING, IDM_AI_FIX,        L"Fix / Debug");
AppendMenu(hAiMenu, MF_STRING, IDM_AI_REFACTOR,   L"Refactor");
AppendMenu(hAiMenu, MF_STRING, IDM_AI_SUMMARIZE,  L"Summarize");
AppendMenu(hAiMenu, MF_SEPARATOR, 0, nullptr);
AppendMenu(hAiMenu, MF_STRING, IDM_AI_CUSTOMPROMPT, L"Custom Prompt...");
AppendMenu(hContextMenu, MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(hAiMenu), L"AI Assistant");
```

- [ ] **Step 6: Build and manually test Feature A**

```
cmake --build build --config Release --target notepad++ 2>&1 | findstr /i "error"
```

Manual test:
1. Open Notepad++, paste any function
2. Select 3-5 lines → right-click → AI Assistant → Explain Selection
3. Wait ~2-5s → floating dialog appears with explanation
4. Click "Insert at Cursor" — text inserts at cursor position
5. Verify "Replace Selection" replaces the selected code

- [ ] **Step 7: Commit**

```bash
git add PowerEditor/src/AIAssistant/AIResultDlg.h PowerEditor/src/AIAssistant/AIResultDlg.cpp PowerEditor/src/NppCommands.cpp PowerEditor/src/Notepad_plus.cpp PowerEditor/src/Notepad_plus.rc
git commit -m "feat: Feature A — AI context menu actions and floating result dialog"
```

---

## Task 6: Feature B — Dockable Chat Panel

**Files:**
- Create: `PowerEditor/src/AIAssistant/AIPanel.h`
- Create: `PowerEditor/src/AIAssistant/AIPanel.cpp`
- Modify: `PowerEditor/src/Notepad_plus.rc` (add IDD_AI_PANEL_DLG)

- [ ] **Step 1: Create `AIPanel.h`**

```cpp
// PowerEditor/src/AIAssistant/AIPanel.h
#pragma once
#include "OllamaClient.h"
#include "DockingDlgInterface.h"
#include <string>
#include <vector>
#include <utility>

#define AI_PANEL_TITLE L"AI Assistant"

class AIPanel : public DockingDlgInterface
{
public:
    explicit AIPanel(OllamaClient* client);
    virtual ~AIPanel() = default;

    void init(HINSTANCE hInst, HWND hParent);

protected:
    virtual INT_PTR CALLBACK run_dlgProc(UINT msg, WPARAM wp, LPARAM lp) override;

private:
    void onInitDialog();
    void onSendMessage();
    void onStreamChunk(const std::string& chunk);
    void onStreamDone();
    void appendToChat(const std::wstring& sender, const std::wstring& text);
    void populateModelCombo();

    OllamaClient* _client;
    std::vector<std::pair<std::wstring, std::wstring>> _history; // {role, content}
    std::wstring  _pendingResponse;
    bool          _streaming = false;
};
```

- [ ] **Step 2: Create `AIPanel.cpp`**

```cpp
// PowerEditor/src/AIAssistant/AIPanel.cpp
#include "AIPanel.h"
#include "../resource.h"
#include <richedit.h>
#include <sstream>

static std::wstring u8w(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static std::string wu8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

AIPanel::AIPanel(OllamaClient* client)
    : DockingDlgInterface(IDD_AI_PANEL_DLG), _client(client) {}

void AIPanel::init(HINSTANCE hInst, HWND hParent)
{
    LoadLibrary(L"Msftedit.dll"); // load RichEdit 4.1
    DockingDlgInterface::init(hInst, hParent);
}

INT_PTR CALLBACK AIPanel::run_dlgProc(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        onInitDialog();
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_AI_SEND_BTN ||
           (LOWORD(wp) == IDC_AI_INPUT_EDIT && HIWORD(wp) == EN_SETFOCUS))
        {
            // handled by WM_KEYDOWN on input edit — see subclassing note below
        }
        if (LOWORD(wp) == IDC_AI_SEND_BTN)
            onSendMessage();
        return TRUE;

    case AI_MSG_STREAM_CHUNK:
    {
        auto* pChunk = reinterpret_cast<std::string*>(lp);
        onStreamChunk(*pChunk);
        delete pChunk;
        return TRUE;
    }
    case AI_MSG_STREAM_DONE:
        onStreamDone();
        return TRUE;

    case AI_MSG_ERROR:
    {
        auto* pResult = reinterpret_cast<AIResult*>(lp);
        appendToChat(L"Error", u8w(pResult->error));
        delete pResult;
        _streaming = false;
        return TRUE;
    }

    case WM_SIZE:
    {
        RECT rc; GetClientRect(_hSelf, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        // Chat history: top 70% of panel
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY),
                     nullptr, 4, 4, w - 8, h * 70 / 100 - 8, SWP_NOZORDER);
        // Checkboxes
        int chkY = h * 70 / 100 + 2;
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_INCL_SEL_CHK),
                     nullptr, 4, chkY, 120, 16, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_INCL_FILE_CHK),
                     nullptr, 130, chkY, 130, 16, SWP_NOZORDER);
        // Input + send button at bottom
        int inputY = chkY + 20;
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_INPUT_EDIT),
                     nullptr, 4, inputY, w - 70, h - inputY - 4, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_SEND_BTN),
                     nullptr, w - 62, inputY, 58, h - inputY - 4, SWP_NOZORDER);
        return TRUE;
    }
    }
    return DockingDlgInterface::run_dlgProc(msg, wp, lp);
}

void AIPanel::onInitDialog()
{
    // Use RICHEDIT50W for Rich Edit 4.1 (Msftedit.dll must be loaded)
    // The dialog template uses "RichEdit50W" as the class name for IDC_AI_CHAT_HISTORY
    // Set read-only on chat history
    HWND hChat = GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY);
    SendMessage(hChat, EM_SETREADONLY, TRUE, 0);

    // Populate model combo
    populateModelCombo();
}

void AIPanel::populateModelCombo()
{
    HWND hCombo = GetDlgItem(_hSelf, IDC_AI_MODEL_COMBO);
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);

    auto models = _client->listModels();
    if (models.empty())
        models.push_back(_client->getModel()); // fallback: current model

    for (auto& m : models)
        SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(m.c_str()));

    // Select current model
    int idx = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT, (WPARAM)-1,
                               reinterpret_cast<LPARAM>(_client->getModel().c_str()));
    SendMessage(hCombo, CB_SETCURSEL, idx >= 0 ? idx : 0, 0);
}

void AIPanel::onSendMessage()
{
    if (_streaming) return; // don't allow concurrent requests

    HWND hInput = GetDlgItem(_hSelf, IDC_AI_INPUT_EDIT);
    int len = GetWindowTextLength(hInput);
    if (len == 0) return;

    std::wstring wInput(len + 1, L'\0');
    GetWindowText(hInput, &wInput[0], len + 1);
    wInput.resize(len);
    SetWindowText(hInput, L""); // clear input

    // Update model from combo
    HWND hCombo = GetDlgItem(_hSelf, IDC_AI_MODEL_COMBO);
    int sel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
    if (sel >= 0) {
        int mlen = (int)SendMessage(hCombo, CB_GETLBTEXTLEN, sel, 0);
        std::wstring model(mlen, L'\0');
        SendMessage(hCombo, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(&model[0]));
        _client->setModel(model);
    }

    appendToChat(L"You", wInput);

    // Build prompt from history
    std::string prompt;
    for (auto& [role, text] : _history)
        prompt += wu8(role) + ": " + wu8(text) + "\n";
    prompt += "Assistant:";

    _streaming = true;
    _pendingResponse.clear();

    // Append "AI: " placeholder
    HWND hChat = GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY);
    SendMessage(hChat, EM_SETREADONLY, FALSE, 0);
    SendMessage(hChat, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessage(hChat, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L"\r\nAI: "));
    SendMessage(hChat, EM_SETREADONLY, TRUE, 0);

    _client->generateStream(prompt, _hSelf);
}

void AIPanel::onStreamChunk(const std::string& chunk)
{
    std::wstring wchunk = u8w(chunk);
    _pendingResponse += wchunk;

    HWND hChat = GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY);
    SendMessage(hChat, EM_SETREADONLY, FALSE, 0);
    SendMessage(hChat, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessage(hChat, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(wchunk.c_str()));
    SendMessage(hChat, EM_SETREADONLY, TRUE, 0);
    // Auto-scroll
    SendMessage(hChat, WM_VSCROLL, SB_BOTTOM, 0);
}

void AIPanel::onStreamDone()
{
    _streaming = false;
    _history.push_back({ L"Assistant", _pendingResponse });
    _pendingResponse.clear();
    // Add newline separator
    HWND hChat = GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY);
    SendMessage(hChat, EM_SETREADONLY, FALSE, 0);
    SendMessage(hChat, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessage(hChat, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L"\r\n"));
    SendMessage(hChat, EM_SETREADONLY, TRUE, 0);
}

void AIPanel::appendToChat(const std::wstring& sender, const std::wstring& text)
{
    _history.push_back({ sender, text });
    std::wstring line = L"\r\n" + sender + L": " + text;
    HWND hChat = GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY);
    SendMessage(hChat, EM_SETREADONLY, FALSE, 0);
    SendMessage(hChat, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessage(hChat, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
    SendMessage(hChat, EM_SETREADONLY, TRUE, 0);
    SendMessage(hChat, WM_VSCROLL, SB_BOTTOM, 0);
}
```

- [ ] **Step 3: Add dialog template to `Notepad_plus.rc`**

```rc
IDD_AI_PANEL_DLG DIALOGEX 0, 0, 220, 400
STYLE DS_SETFONT | DS_FIXEDSYS | WS_CHILD
FONT 9, "Segoe UI"
BEGIN
    CONTROL "", IDC_AI_CHAT_HISTORY, "RichEdit50W",
            ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_BORDER, 4, 4, 212, 270
    COMBOBOX IDC_AI_MODEL_COMBO, 4, 278, 212, 80, CBS_DROPDOWNLIST | WS_VSCROLL
    CONTROL "Include selection", IDC_AI_INCL_SEL_CHK,
            "Button", BS_AUTOCHECKBOX | WS_TABSTOP, 4, 296, 100, 12
    CONTROL "Include current file", IDC_AI_INCL_FILE_CHK,
            "Button", BS_AUTOCHECKBOX | WS_TABSTOP, 110, 296, 106, 12
    EDITTEXT IDC_AI_INPUT_EDIT, 4, 312, 152, 80, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL
    PUSHBUTTON "Send", IDC_AI_SEND_BTN, 160, 312, 56, 80
END
```

- [ ] **Step 4: Wire panel into `Notepad_plus::launchAIPanel()`**

Update `launchAIPanel()` in `Notepad_plus.cpp` — replace the stub from Task 4 with the full docking setup (matching the FunctionList pattern from line 7727):

```cpp
void Notepad_plus::launchAIPanel()
{
    if (!_pAIAssistant) return;
    AIPanel* panel = _pAIAssistant->getPanel();
    if (!panel) return;

    if (!panel->isCreated())
    {
        DockedWidgetData data{};
        data.uMask       = DWS_DF_CONT_RIGHT | DWS_ICONTAB | DWS_USEOWNDARKMODE;
        data.pszModuleName = NPP_INTERNAL_FUNCTION_STR;
        data.dlgID       = IDM_AI_PANEL;

        static wchar_t title[] = L"AI Assistant";
        data.pszName = title;

        panel->create(&data, {});
        ::SendMessage(_pPublicInterface->getHSelf(), NPPM_MODELESSDIALOG,
                      MODELESSDIALOGREMOVE, reinterpret_cast<LPARAM>(panel->getHSelf()));
    }
    panel->display(!panel->isVisible());
}
```

- [ ] **Step 5: Build and manually test Feature B**

```
cmake --build build --config Release --target notepad++ 2>&1 | findstr /i "error"
```

Manual test:
1. Open Notepad++
2. View menu → AI Assistant (or trigger IDM_AI_PANEL command)
3. Panel docks on the right side
4. Type "Hello, can you help me?" in the input box → click Send
5. Verify response streams in word-by-word
6. Model dropdown lists available Ollama models
7. Close and reopen panel — verify it docks again correctly

- [ ] **Step 6: Commit**

```bash
git add PowerEditor/src/AIAssistant/AIPanel.h PowerEditor/src/AIAssistant/AIPanel.cpp PowerEditor/src/Notepad_plus.rc PowerEditor/src/Notepad_plus.cpp
git commit -m "feat: Feature B — dockable AI chat panel with streaming responses"
```

---

## Task 7: Feature C — Inline Completer

**Files:**
- Create: `PowerEditor/src/AIAssistant/InlineCompleter.h`
- Create: `PowerEditor/src/AIAssistant/InlineCompleter.cpp`
- Modify: `PowerEditor/src/NppNotification.cpp`
- Modify: `PowerEditor/src/Notepad_plus.h` (key intercept)

- [ ] **Step 1: Create `InlineCompleter.h`**

```cpp
// PowerEditor/src/AIAssistant/InlineCompleter.h
#pragma once
#include "OllamaClient.h"
#include <windows.h>
#include <string>

class ScintillaEditView;

class InlineCompleter
{
public:
    InlineCompleter(OllamaClient* client, ScintillaEditView** ppView);
    ~InlineCompleter();

    void init(HWND hNpp);
    void reloadSettings();

    // Called from SCN_CHARADDED handler
    void onCharAdded(wchar_t ch);

    // Called when Tab is pressed — returns true if completion was accepted
    bool onTab();

    // Called when Escape is pressed — returns true if ghost text was cleared
    bool onEscape();

    // Manual trigger (Ctrl+Space)
    void triggerNow();

    bool hasGhostText() const { return !_ghostText.empty(); }

private:
    static VOID CALLBACK debounceTimer(HWND, UINT, UINT_PTR id, DWORD);
    void requestCompletion();
    void showGhostText(const std::string& completion);
    void clearGhostText();
    std::string buildPrompt();

    OllamaClient*       _client;
    ScintillaEditView** _ppView;
    HWND                _hNpp = nullptr;
    UINT_PTR            _timerId = 0;
    std::wstring        _ghostText;
    bool                _waiting = false; // request in flight

    static InlineCompleter* _instance; // for timer callback
};
```

- [ ] **Step 2: Create `InlineCompleter.cpp`**

```cpp
// PowerEditor/src/AIAssistant/InlineCompleter.cpp
#include "InlineCompleter.h"
#include "../resource.h"
#include "../Parameters.h"
#include "../../ScintillaComponent/ScintillaEditView.h"

InlineCompleter* InlineCompleter::_instance = nullptr;

static std::wstring u8w(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n-1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static std::string wu8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n-1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

InlineCompleter::InlineCompleter(OllamaClient* client, ScintillaEditView** ppView)
    : _client(client), _ppView(ppView)
{
    _instance = this;
}

InlineCompleter::~InlineCompleter()
{
    if (_timerId) KillTimer(nullptr, _timerId);
    _instance = nullptr;
}

void InlineCompleter::init(HWND hNpp)
{
    _hNpp = hNpp;
}

void InlineCompleter::reloadSettings() { /* settings re-read from NppParameters on each use */ }

void InlineCompleter::onCharAdded(wchar_t)
{
    const NppAISettings& s = NppParameters::getInstance().getNppGUI()._aiSettings;
    if (!s.autoCompleteOn) return;

    clearGhostText();
    _waiting = false;

    // Reset debounce timer
    if (_timerId) KillTimer(_hNpp, _timerId);
    _timerId = SetTimer(_hNpp, 0xAI01, s.autoCompleteDelayMs, nullptr);
    // Timer fires debounceTimer (registered via subclass or WM_TIMER in Notepad_plus)
    // See Step 4 for WM_TIMER hook
}

VOID CALLBACK InlineCompleter::debounceTimer(HWND, UINT, UINT_PTR, DWORD)
{
    if (_instance) _instance->requestCompletion();
}

void InlineCompleter::triggerNow()
{
    if (_timerId) { KillTimer(_hNpp, _timerId); _timerId = 0; }
    clearGhostText();
    requestCompletion();
}

void InlineCompleter::requestCompletion()
{
    if (_waiting) return;
    _waiting = true;

    std::string prompt = buildPrompt();
    if (prompt.empty()) { _waiting = false; return; }

    // Async: result comes back as AI_MSG_RESULT to _hNpp
    _client->generate(prompt, _hNpp);
}

std::string InlineCompleter::buildPrompt()
{
    if (!_ppView || !*_ppView) return {};
    ScintillaEditView* view = *_ppView;

    intptr_t curPos  = view->execute(SCI_GETCURRENTPOS);
    intptr_t curLine = view->execute(SCI_LINEFROMPOSITION, curPos);

    // Grab 10 lines of context before cursor
    intptr_t startLine = max((intptr_t)0, curLine - 10);
    intptr_t startPos  = view->execute(SCI_POSITIONFROMLINE, startLine);

    intptr_t contextLen = curPos - startPos;
    if (contextLen <= 0) return {};

    std::string context(contextLen + 1, '\0');
    Sci_TextRangeFull tr{};
    tr.chrg.cpMin = static_cast<Sci_Position>(startPos);
    tr.chrg.cpMax = static_cast<Sci_Position>(curPos);
    tr.lpstrText  = &context[0];
    view->execute(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<LPARAM>(&tr));
    context.resize(contextLen);

    const NppAISettings& s = NppParameters::getInstance().getNppGUI()._aiSettings;

    return "Continue the following code. Output ONLY the completion text with no explanation, "
           "no markdown, no code fences. Max " + std::to_string(s.maxTokens) + " tokens.\n\n"
           + context;
}

void InlineCompleter::showGhostText(const std::string& completion)
{
    if (!_ppView || !*_ppView || completion.empty()) return;
    ScintillaEditView* view = *_ppView;

    _ghostText = u8w(completion);

    intptr_t curLine = view->execute(SCI_LINEFROMPOSITION, view->execute(SCI_GETCURRENTPOS));

    // Show as annotation below current line
    std::string annotText = wu8(_ghostText);
    view->execute(SCI_ANNOTATIONSETTEXT, curLine, reinterpret_cast<LPARAM>(annotText.c_str()));
    view->execute(SCI_ANNOTATIONSETSTYLE, curLine, STYLE_DEFAULT);
    view->execute(SCI_ANNOTATIONSETVISIBLE, ANNOTATION_STANDARD);
}

void InlineCompleter::clearGhostText()
{
    if (_ghostText.empty()) return;
    if (_ppView && *_ppView) {
        ScintillaEditView* view = *_ppView;
        intptr_t curLine = view->execute(SCI_LINEFROMPOSITION, view->execute(SCI_GETCURRENTPOS));
        view->execute(SCI_ANNOTATIONSETTEXT, curLine, reinterpret_cast<LPARAM>(""));
        view->execute(SCI_ANNOTATIONSETVISIBLE, ANNOTATION_HIDDEN);
    }
    _ghostText.clear();
}

bool InlineCompleter::onTab()
{
    if (_ghostText.empty()) return false;
    if (!_ppView || !*_ppView) return false;

    ScintillaEditView* view = *_ppView;
    std::string utf8 = wu8(_ghostText);
    clearGhostText();
    view->execute(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(utf8.c_str()));
    return true;
}

bool InlineCompleter::onEscape()
{
    if (_ghostText.empty()) return false;
    clearGhostText();
    _waiting = false;
    if (_timerId) { KillTimer(_hNpp, _timerId); _timerId = 0; }
    return true;
}
```

- [ ] **Step 3: Forward `SCN_CHARADDED` to InlineCompleter in `NppNotification.cpp`**

Open `PowerEditor/src/NppNotification.cpp`. Find the `SCN_CHARADDED` case (line ~196). At the **end** of the case block (before `break`), add:

```cpp
// Forward to AI inline completer
if (_pAIAssistant && _pAIAssistant->isAutoCompleteEnabled())
{
    extern void nppAIOnCharAdded(wchar_t);
    // Call via AIAssistant to avoid circular includes
    _pAIAssistant->onCharAdded(static_cast<wchar_t>(notification->ch));
}
```

Then add `onCharAdded` to `AIAssistant.h` and `AIAssistant.cpp`:

In `AIAssistant.h`, add:
```cpp
void onCharAdded(wchar_t ch);
```

In `AIAssistant.cpp`, add:
```cpp
void AIAssistant::onCharAdded(wchar_t ch)
{
    if (_completer) _completer->onCharAdded(ch);
}
```

- [ ] **Step 4: Handle WM_TIMER + Tab/Escape + Ctrl+Space in `Notepad_plus.cpp`**

In `Notepad_plus.cpp`, find `WM_TIMER` handling (or where key messages are processed). Add:

```cpp
case WM_TIMER:
    if (wp == 0xAI01 && _pAIAssistant)
    {
        // Debounce timer fired — request inline completion
        if (InlineCompleter* ic = _pAIAssistant->getCompleter())
            ic->requestCompletion();
        KillTimer(_hSelf, 0xAI01);
    }
    break;
```

For Tab/Escape, find where `WM_KEYDOWN` or `SCN_KEY` is handled. Add:

```cpp
// In key handler (WM_KEYDOWN or equivalent):
if (wParam == VK_TAB && _pAIAssistant)
{
    InlineCompleter* ic = _pAIAssistant->getCompleter();
    if (ic && ic->onTab()) return 0; // consumed
}
if (wParam == VK_ESCAPE && _pAIAssistant)
{
    InlineCompleter* ic = _pAIAssistant->getCompleter();
    if (ic && ic->onEscape()) return 0; // consumed
}
// Ctrl+Space
if (wParam == VK_SPACE && (GetKeyState(VK_CONTROL) & 0x8000) && _pAIAssistant)
{
    InlineCompleter* ic = _pAIAssistant->getCompleter();
    if (ic) ic->triggerNow();
    return 0;
}
```

Also add `getCompleter()` to `AIAssistant.h`:
```cpp
InlineCompleter* getCompleter() { return _completer.get(); }
```

Also, hook `AI_MSG_RESULT` in the main window proc to feed completions back:

In the message handling section of `Notepad_plus.cpp` (find `WM_COMMAND` dispatch), add:

```cpp
case AI_MSG_RESULT:
{
    auto* pResult = reinterpret_cast<AIResult*>(lp);
    if (pResult->success && _pAIAssistant)
    {
        InlineCompleter* ic = _pAIAssistant->getCompleter();
        if (ic) ic->showGhostText(pResult->response);  // make showGhostText public
    }
    delete pResult;
    return 0;
}
```

Make `showGhostText` public in `InlineCompleter.h`.

- [ ] **Step 5: Build and manually test Feature C**

```
cmake --build build --config Release --target notepad++ 2>&1 | findstr /i "error"
```

Manual test:
1. Open a `.py` or `.cpp` file
2. Type a partial function signature, pause 500ms
3. Ghost text annotation appears below the current line
4. Press **Tab** → text is inserted at cursor
5. Press **Escape** → ghost text disappears without inserting
6. Press **Ctrl+Space** at any time → immediately fires completion without waiting

- [ ] **Step 6: Commit**

```bash
git add PowerEditor/src/AIAssistant/InlineCompleter.h PowerEditor/src/AIAssistant/InlineCompleter.cpp PowerEditor/src/NppNotification.cpp PowerEditor/src/Notepad_plus.cpp PowerEditor/src/AIAssistant/AIAssistant.h PowerEditor/src/AIAssistant/AIAssistant.cpp
git commit -m "feat: Feature C — inline AI completions with ghost text, Tab/Escape/Ctrl+Space"
```

---

## Task 8: Preferences Tab

**Files:**
- Modify: `PowerEditor/src/WinControls/Preference/preference.h`
- Modify: `PowerEditor/src/WinControls/Preference/preference.cpp`
- Modify: `PowerEditor/src/Notepad_plus.rc` (add preference page template)
- Modify: `PowerEditor/src/resource.h` (add preference page IDs)

- [ ] **Step 1: Add resource IDs to `resource.h`**

```cpp
// AI Preferences page
#define IDD_PREFERENCE_AI_PAGE   6700
#define IDC_AI_PREF_ENDPOINT     6701
#define IDC_AI_PREF_MODEL        6702
#define IDC_AI_PREF_AUTOCOMPLETE 6703
#define IDC_AI_PREF_DELAY        6704
#define IDC_AI_PREF_MAXTOKENS    6705
```

- [ ] **Step 2: Add dialog template to `Notepad_plus.rc`**

```rc
IDD_PREFERENCE_AI_PAGE DIALOGEX 0, 0, 290, 220
STYLE DS_SETFONT | DS_FIXEDSYS | WS_CHILD
FONT 9, "Segoe UI"
BEGIN
    GROUPBOX        "Ollama Connection", IDC_STATIC, 4, 4, 282, 50
    LTEXT           "Endpoint:", IDC_STATIC, 12, 20, 45, 9
    EDITTEXT        IDC_AI_PREF_ENDPOINT, 60, 18, 218, 12
    LTEXT           "Default model:", IDC_STATIC, 12, 36, 55, 9
    EDITTEXT        IDC_AI_PREF_MODEL, 70, 34, 208, 12

    GROUPBOX        "Inline Completions", IDC_STATIC, 4, 60, 282, 80
    CONTROL         "Enable inline completions", IDC_AI_PREF_AUTOCOMPLETE,
                    "Button", BS_AUTOCHECKBOX | WS_TABSTOP, 12, 76, 150, 12
    LTEXT           "Delay (ms):", IDC_STATIC, 12, 94, 55, 9
    EDITTEXT        IDC_AI_PREF_DELAY, 70, 92, 60, 12
    LTEXT           "Max tokens:", IDC_STATIC, 12, 110, 55, 9
    EDITTEXT        IDC_AI_PREF_MAXTOKENS, 70, 108, 60, 12
END
```

- [ ] **Step 3: Add the AI page to the Preference dialog**

Open `PowerEditor/src/WinControls/Preference/preference.h`. Find where other page IDs/indices are defined (e.g. `PREFERENCE_DARKMODE`, `PREFERENCE_GENERAL`). Add:

```cpp
#define PREFERENCE_AI_PAGE_INDEX  <next available index>
```

Open `PowerEditor/src/WinControls/Preference/preference.cpp`. Find where `addPage` or `PropSheet_AddPage` is called for each tab. Following the same pattern, add:

```cpp
// AI Assistant page
PROPSHEETPAGE pspAI{};
pspAI.dwSize      = sizeof(PROPSHEETPAGE);
pspAI.dwFlags     = PSP_DEFAULT;
pspAI.hInstance   = _hInst;
pspAI.pszTemplate = MAKEINTRESOURCE(IDD_PREFERENCE_AI_PAGE);
pspAI.pfnDlgProc  = preferenceAIPageDlgProc;
pspAI.lParam      = reinterpret_cast<LPARAM>(this);
_aiPageIndex      = (int)PropSheet_GetPageCount(hPropSheet);
PropSheet_AddPage(hPropSheet, CreatePropertySheetPage(&pspAI));
```

- [ ] **Step 4: Implement the AI preference page dialog proc**

In `preference.cpp`, add:

```cpp
INT_PTR CALLBACK preferenceAIPageDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        const NppAISettings& s = NppParameters::getInstance().getNppGUI()._aiSettings;
        SetDlgItemText(hDlg, IDC_AI_PREF_ENDPOINT, s.endpoint.c_str());
        SetDlgItemText(hDlg, IDC_AI_PREF_MODEL,    s.model.c_str());
        CheckDlgButton(hDlg, IDC_AI_PREF_AUTOCOMPLETE, s.autoCompleteOn ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(hDlg,  IDC_AI_PREF_DELAY,     s.autoCompleteDelayMs, FALSE);
        SetDlgItemInt(hDlg,  IDC_AI_PREF_MAXTOKENS, s.maxTokens,           FALSE);
        return TRUE;
    }
    case WM_NOTIFY:
    {
        LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lp);
        if (nmhdr->code == PSN_APPLY)
        {
            NppAISettings& s = NppParameters::getInstance().getNppGUI()._aiSettings;
            wchar_t buf[256];
            GetDlgItemText(hDlg, IDC_AI_PREF_ENDPOINT, buf, 256); s.endpoint = buf;
            GetDlgItemText(hDlg, IDC_AI_PREF_MODEL,    buf, 256); s.model    = buf;
            s.autoCompleteOn      = IsDlgButtonChecked(hDlg, IDC_AI_PREF_AUTOCOMPLETE) == BST_CHECKED;
            s.autoCompleteDelayMs = GetDlgItemInt(hDlg, IDC_AI_PREF_DELAY,     nullptr, FALSE);
            s.maxTokens           = GetDlgItemInt(hDlg, IDC_AI_PREF_MAXTOKENS, nullptr, FALSE);
            NppParameters::getInstance().saveConfig();
            // Tell AIAssistant to reload
            // (access via global or message — use PostMessage to main window)
            // PostMessage(hwndNpp, NPPM_INTERNAL_RELOADAI, 0, 0); — add this message if needed
        }
        return TRUE;
    }
    }
    return FALSE;
}
```

- [ ] **Step 5: Final build — all features**

```
cmake --build build --config Release --target notepad++ 2>&1 | findstr /i "error"
```

Expected: clean build, zero errors.

- [ ] **Step 6: Full integration smoke test**

1. Open Notepad++ (Release build)
2. **Feature A:** Select a code snippet → right-click → AI Assistant → Explain Selection → verify response dialog opens, Insert/Replace work
3. **Feature B:** View → AI Assistant panel → type a message → verify streaming response → change model in dropdown
4. **Feature C:** Open a .cpp file, type a partial function → wait 500ms → verify annotation appears → Tab accepts → Escape dismisses → Ctrl+Space triggers immediately
5. **Preferences:** Settings → Preferences → AI Assistant tab → change model/endpoint → save → verify changes persist after restart

- [ ] **Step 7: Commit**

```bash
git add PowerEditor/src/WinControls/Preference/ PowerEditor/src/Notepad_plus.rc PowerEditor/src/resource.h
git commit -m "feat: add AI Assistant preferences tab to Settings dialog"
```

- [ ] **Step 8: Push branch**

```bash
git push origin claude-develop
```

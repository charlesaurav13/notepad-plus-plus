// PowerEditor/src/AIAssistant/OllamaClient.h
#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>

// Posted to hwndNotify when generate() completes.
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

    // Async non-streaming: posts AI_MSG_RESULT or AI_MSG_ERROR to hwndNotify.
    // LPARAM = heap-allocated AIResult* (receiver must delete).
    void generate(const std::string& prompt, HWND hwndNotify);

    // Async streaming: posts AI_MSG_STREAM_CHUNK per token, then AI_MSG_STREAM_DONE.
    // LPARAM of AI_MSG_STREAM_CHUNK = heap-allocated std::string* (receiver must delete).
    void generateStream(const std::string& prompt, HWND hwndNotify);

    // Synchronous — call only from a background thread.
    AIResult generateSync(const std::string& prompt);

    // Check if Ollama is reachable (synchronous, fast HEAD/GET to /api/tags).
    bool isAvailable();

    // Returns list of model names from GET /api/tags.
    std::vector<std::wstring> listModels();

    void setModel(const std::wstring& model)    { _model    = model; }
    void setEndpoint(const std::wstring& ep)    { _endpoint = ep; }
    const std::wstring& getModel()    const     { return _model; }
    const std::wstring& getEndpoint() const     { return _endpoint; }

    // Public helpers used by AIAssistant.cpp
    static std::string  wstrToUtf8(const std::wstring& w);
    static std::wstring utf8ToWstr(const std::string& s);

private:
    std::wstring _endpoint;
    std::wstring _model;

    struct ThreadParam {
        OllamaClient* client;
        std::string   prompt;
        HWND          hwndNotify;
        bool          stream;
    };

    static DWORD WINAPI workerThread(LPVOID param);

    // Parse _endpoint into host and port (strips http://)
    void parseEndpoint(std::wstring& outHost, INTERNET_PORT& outPort) const;

    // Synchronous HTTP POST — returns raw response body
    // If streaming=true, posts AI_MSG_STREAM_CHUNK messages per line to hwndNotify
    std::string httpPost(const std::wstring& host, INTERNET_PORT port,
                         const std::wstring& path, const std::string& body,
                         bool streaming, HWND hwndNotify);
};

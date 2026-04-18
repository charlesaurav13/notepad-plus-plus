// PowerEditor/src/AIAssistant/OllamaClient.cpp
#include <windows.h>
#include <winhttp.h>
#include "OllamaClient.h"
#include "../resource.h"
#include "../json/json.hpp"

#include <stdexcept>
#include <sstream>

// -------------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------------
OllamaClient::OllamaClient(const std::wstring& endpoint, const std::wstring& model)
    : _endpoint(endpoint), _model(model)
{
}

// -------------------------------------------------------------------------
// Encoding helpers
// -------------------------------------------------------------------------
std::string OllamaClient::wstrToUtf8(const std::wstring& w)
{
    if (w.empty())
        return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
        return {};
    std::string result(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        result.data(), needed, nullptr, nullptr);
    return result;
}

std::wstring OllamaClient::utf8ToWstr(const std::string& s)
{
    if (s.empty())
        return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                     nullptr, 0);
    if (needed <= 0)
        return {};
    std::wstring result(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                        result.data(), needed);
    return result;
}

// -------------------------------------------------------------------------
// parseEndpoint
// -------------------------------------------------------------------------
/*static*/
void OllamaClient::parseEndpointStr(const std::wstring& endpoint,
                                     std::wstring& outHost, INTERNET_PORT& outPort)
{
    std::wstring ep = endpoint;

    // Strip scheme
    auto stripScheme = [&](const std::wstring& scheme) {
        if (ep.size() >= scheme.size() &&
            ep.substr(0, scheme.size()) == scheme)
        {
            ep = ep.substr(scheme.size());
        }
    };
    stripScheme(L"https://");
    stripScheme(L"http://");

    // Strip trailing slash
    while (!ep.empty() && ep.back() == L'/')
        ep.pop_back();

    // Split host:port
    auto colonPos = ep.rfind(L':');
    if (colonPos != std::wstring::npos)
    {
        outHost = ep.substr(0, colonPos);
        std::wstring portStr = ep.substr(colonPos + 1);
        outPort = static_cast<INTERNET_PORT>(_wtoi(portStr.c_str()));
        if (outPort == 0)
            outPort = 11434;
    }
    else
    {
        outHost = ep;
        outPort = 11434;
    }
}

void OllamaClient::parseEndpoint(std::wstring& outHost, INTERNET_PORT& outPort) const
{
    parseEndpointStr(_endpoint, outHost, outPort);
}

// -------------------------------------------------------------------------
// httpPost — core WinHTTP request
// -------------------------------------------------------------------------
std::string OllamaClient::httpPost(const std::wstring& host, INTERNET_PORT port,
                                   const std::wstring& path, const std::string& body,
                                   bool streaming, HWND hwndNotify)
{
    std::string accumulated;

    HINTERNET hSess = nullptr;
    HINTERNET hConn = nullptr;
    HINTERNET hReq  = nullptr;

    auto cleanup = [&]() {
        if (hReq)  { WinHttpCloseHandle(hReq);  hReq  = nullptr; }
        if (hConn) { WinHttpCloseHandle(hConn); hConn = nullptr; }
        if (hSess) { WinHttpCloseHandle(hSess); hSess = nullptr; }
    };

    hSess = WinHttpOpen(L"NppAI/1.0",
                        WINHTTP_ACCESS_TYPE_NO_PROXY,
                        nullptr, nullptr, 0);
    if (!hSess)
    {
        cleanup();
        throw std::runtime_error("WinHttpOpen failed");
    }

    // Set timeouts: resolve, connect, send, receive
    // For streaming, use 120s max to avoid hanging indefinitely
    DWORD recvTimeout = streaming ? 120000 : 30000;
    WinHttpSetTimeouts(hSess, 10000, 10000, 30000, recvTimeout);

    hConn = WinHttpConnect(hSess, host.c_str(), port, 0);
    if (!hConn)
    {
        cleanup();
        throw std::runtime_error("WinHttpConnect failed");
    }

    hReq = WinHttpOpenRequest(hConn, L"POST", path.c_str(),
                              nullptr,                      // HTTP/1.1
                              WINHTTP_NO_REFERER,
                              WINHTTP_DEFAULT_ACCEPT_TYPES,
                              0);                           // no HTTPS flag
    if (!hReq)
    {
        cleanup();
        throw std::runtime_error("WinHttpOpenRequest failed");
    }

    // Add Content-Type header
    WinHttpAddRequestHeaders(hReq,
        L"Content-Type: application/json",
        static_cast<DWORD>(-1L),
        WINHTTP_ADDREQ_FLAG_ADD);

    // Send request with body
    BOOL sent = WinHttpSendRequest(hReq,
                                   WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   const_cast<char*>(body.data()),
                                   static_cast<DWORD>(body.size()),
                                   static_cast<DWORD>(body.size()),
                                   0);
    if (!sent)
    {
        cleanup();
        throw std::runtime_error("WinHttpSendRequest failed");
    }

    if (!WinHttpReceiveResponse(hReq, nullptr))
    {
        cleanup();
        throw std::runtime_error("WinHttpReceiveResponse failed");
    }

    // Check HTTP status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                        WINHTTP_NO_HEADER_INDEX);
    if (statusCode >= 400) {
        // Read error body for context
        std::string errBody;
        DWORD bytesAvail = 0;
        char buf[1024];
        while (WinHttpQueryDataAvailable(hReq, &bytesAvail) && bytesAvail > 0) {
            DWORD toRead = std::min(bytesAvail, (DWORD)sizeof(buf));
            DWORD bytesRead = 0;
            if (!WinHttpReadData(hReq, buf, toRead, &bytesRead) || bytesRead == 0) break;
            errBody.append(buf, bytesRead);
        }
        cleanup();
        return "__HTTP_ERROR__:" + std::to_string(statusCode) + ":" + errBody;
    }

    // ---- Read data in 4096-byte chunks ----
    std::string lineBuffer; // used only when streaming to buffer partial lines
    bool doneSent = false;  // guard to ensure AI_MSG_STREAM_DONE is always posted

    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(hReq, &available))
            break;
        if (available == 0)
            break;

        DWORD toRead = (available > 4096) ? 4096 : available;
        std::string chunk(toRead, '\0');
        DWORD bytesRead = 0;

        if (!WinHttpReadData(hReq, chunk.data(), toRead, &bytesRead))
            break;
        if (bytesRead == 0)
            break;

        chunk.resize(bytesRead);

        if (!streaming)
        {
            accumulated += chunk;
        }
        else
        {
            // Parse NDJSON line by line
            lineBuffer += chunk;

            std::string::size_type start = 0;
            while (true)
            {
                auto nl = lineBuffer.find('\n', start);
                if (nl == std::string::npos)
                    break;

                std::string line = lineBuffer.substr(start, nl - start);
                start = nl + 1;

                // Trim \r if present
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                if (line.empty())
                    continue;

                try
                {
                    auto j = nlohmann::json::parse(line);

                    // Extract "response" token
                    if (j.contains("response") && j["response"].is_string())
                    {
                        std::string token = j["response"].get<std::string>();
                        if (!token.empty() && hwndNotify)
                        {
                            auto* s = new std::string(std::move(token));
                            if (!PostMessage(hwndNotify, AI_MSG_STREAM_CHUNK,
                                             0, reinterpret_cast<LPARAM>(s)))
                                delete s;
                        }
                    }

                    // Check "done"
                    if (j.contains("done") && j["done"].is_boolean() && j["done"].get<bool>())
                    {
                        if (hwndNotify)
                        {
                            PostMessage(hwndNotify, AI_MSG_STREAM_DONE, 0, 0);
                            doneSent = true;
                        }
                        cleanup();
                        return {};
                    }
                }
                catch (const nlohmann::json::exception&)
                {
                    // Malformed line — skip
                }
            }

            // Keep the unfinished tail for the next iteration
            lineBuffer = lineBuffer.substr(start);
        }
    }

    // Ensure AI_MSG_STREAM_DONE is always sent when streaming
    if (streaming && hwndNotify && !doneSent) {
        PostMessage(hwndNotify, AI_MSG_STREAM_DONE, 0, 0);
    }

    cleanup();
    return accumulated;
}

// -------------------------------------------------------------------------
// isAvailable
// -------------------------------------------------------------------------
bool OllamaClient::isAvailable()
{
    std::wstring host;
    INTERNET_PORT port = 11434;
    parseEndpoint(host, port);

    HINTERNET hSess = nullptr;
    HINTERNET hConn = nullptr;
    HINTERNET hReq  = nullptr;

    auto cleanup = [&]() {
        if (hReq)  { WinHttpCloseHandle(hReq);  hReq  = nullptr; }
        if (hConn) { WinHttpCloseHandle(hConn); hConn = nullptr; }
        if (hSess) { WinHttpCloseHandle(hSess); hSess = nullptr; }
    };

    hSess = WinHttpOpen(L"NppAI/1.0",
                        WINHTTP_ACCESS_TYPE_NO_PROXY,
                        nullptr, nullptr, 0);
    if (!hSess) { cleanup(); return false; }

    // Short 3-second timeouts for availability check
    WinHttpSetTimeouts(hSess, 3000, 3000, 3000, 3000);

    hConn = WinHttpConnect(hSess, host.c_str(), port, 0);
    if (!hConn) { cleanup(); return false; }

    hReq = WinHttpOpenRequest(hConn, L"GET", L"/api/tags",
                              nullptr,
                              WINHTTP_NO_REFERER,
                              WINHTTP_DEFAULT_ACCEPT_TYPES,
                              0);
    if (!hReq) { cleanup(); return false; }

    BOOL sent = WinHttpSendRequest(hReq,
                                   WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent) { cleanup(); return false; }

    BOOL ok = WinHttpReceiveResponse(hReq, nullptr);
    cleanup();
    return ok == TRUE;
}

// -------------------------------------------------------------------------
// listModels
// -------------------------------------------------------------------------
std::vector<std::wstring> OllamaClient::listModels()
{
    std::wstring host;
    INTERNET_PORT port = 11434;
    parseEndpoint(host, port);

    HINTERNET hSess = nullptr;
    HINTERNET hConn = nullptr;
    HINTERNET hReq  = nullptr;

    auto cleanup = [&]() {
        if (hReq)  { WinHttpCloseHandle(hReq);  hReq  = nullptr; }
        if (hConn) { WinHttpCloseHandle(hConn); hConn = nullptr; }
        if (hSess) { WinHttpCloseHandle(hSess); hSess = nullptr; }
    };

    auto fallback = [&]() -> std::vector<std::wstring> {
        cleanup();
        return { _model };
    };

    hSess = WinHttpOpen(L"NppAI/1.0",
                        WINHTTP_ACCESS_TYPE_NO_PROXY,
                        nullptr, nullptr, 0);
    if (!hSess) return fallback();

    WinHttpSetTimeouts(hSess, 5000, 5000, 10000, 10000);

    hConn = WinHttpConnect(hSess, host.c_str(), port, 0);
    if (!hConn) return fallback();

    hReq = WinHttpOpenRequest(hConn, L"GET", L"/api/tags",
                              nullptr,
                              WINHTTP_NO_REFERER,
                              WINHTTP_DEFAULT_ACCEPT_TYPES,
                              0);
    if (!hReq) return fallback();

    BOOL sent = WinHttpSendRequest(hReq,
                                   WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent) return fallback();

    if (!WinHttpReceiveResponse(hReq, nullptr)) return fallback();

    // Read full response
    std::string body;
    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(hReq, &available) || available == 0)
            break;
        DWORD toRead = (available > 4096) ? 4096 : available;
        std::string chunk(toRead, '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hReq, chunk.data(), toRead, &bytesRead) || bytesRead == 0)
            break;
        chunk.resize(bytesRead);
        body += chunk;
    }
    cleanup();

    // Parse JSON: {"models": [{"name": "..."}, ...]}
    try
    {
        auto j = nlohmann::json::parse(body);
        std::vector<std::wstring> names;
        if (j.contains("models") && j["models"].is_array())
        {
            for (const auto& m : j["models"])
            {
                if (m.contains("name") && m["name"].is_string())
                    names.push_back(utf8ToWstr(m["name"].get<std::string>()));
            }
        }
        if (!names.empty())
            return names;
    }
    catch (const nlohmann::json::exception&)
    {
        // Fall through to fallback
    }

    return { _model };
}

// -------------------------------------------------------------------------
// generateSync
// -------------------------------------------------------------------------
AIResult OllamaClient::generateSync(const std::string& prompt)
{
    AIResult result;
    try
    {
        std::wstring host;
        INTERNET_PORT port = 11434;
        parseEndpoint(host, port);

        // Build JSON body
        nlohmann::json body;
        body["model"]  = wstrToUtf8(_model);
        body["prompt"] = prompt;
        body["stream"] = false;

        std::string bodyStr = body.dump();
        std::string raw = httpPost(host, port, L"/api/generate", bodyStr, false, nullptr);

        // Check for HTTP-level error sentinel
        if (raw.substr(0, 14) == "__HTTP_ERROR__") {
            result.error = "Ollama HTTP error: " + raw.substr(14);
            result.success = false;
            return result;
        }

        // Parse response
        auto j = nlohmann::json::parse(raw);
        if (j.contains("response") && j["response"].is_string())
        {
            result.response = j["response"].get<std::string>();
            result.success  = true;
        }
        else
        {
            result.success = false;
            result.error   = "No 'response' field in Ollama reply";
        }
    }
    catch (const std::exception& ex)
    {
        result.success = false;
        result.error   = ex.what();
    }
    catch (...)
    {
        result.success = false;
        result.error   = "Unknown error in generateSync";
    }
    return result;
}

// -------------------------------------------------------------------------
// workerThread
// -------------------------------------------------------------------------
DWORD WINAPI OllamaClient::workerThread(LPVOID param)
{
    auto* tp = static_cast<ThreadParam*>(param);

    if (tp->stream)
    {
        // Streaming path — httpPost posts chunk/done messages directly
        try
        {
            std::wstring host;
            INTERNET_PORT port = 11434;
            OllamaClient::parseEndpointStr(tp->endpoint, host, port);

            nlohmann::json body;
            body["model"]  = wstrToUtf8(tp->model);
            body["prompt"] = tp->prompt;
            body["stream"] = true;

            std::string bodyStr = body.dump();
            tp->client->httpPost(host, port, L"/api/generate",
                                 bodyStr, true, tp->hwndNotify);
        }
        catch (const std::exception& ex)
        {
            // Post an error result
            auto* res = new AIResult();
            res->success = false;
            res->error   = ex.what();
            if (!PostMessage(tp->hwndNotify, AI_MSG_ERROR,
                             0, reinterpret_cast<LPARAM>(res)))
                delete res;
        }
        catch (...)
        {
            auto* res = new AIResult();
            res->success = false;
            res->error   = "Unknown streaming error";
            if (!PostMessage(tp->hwndNotify, AI_MSG_ERROR,
                             0, reinterpret_cast<LPARAM>(res)))
                delete res;
        }
    }
    else
    {
        // Non-streaming path — use snapshotted model/endpoint to avoid data race
        AIResult result;
        try
        {
            std::wstring host;
            INTERNET_PORT port = 11434;
            OllamaClient::parseEndpointStr(tp->endpoint, host, port);

            nlohmann::json body;
            body["model"]  = wstrToUtf8(tp->model);
            body["prompt"] = tp->prompt;
            body["stream"] = false;

            std::string bodyStr = body.dump();
            std::string raw = tp->client->httpPost(host, port, L"/api/generate",
                                                   bodyStr, false, nullptr);

            // Check for HTTP-level error sentinel
            if (raw.substr(0, 14) == "__HTTP_ERROR__") {
                result.error   = "Ollama HTTP error: " + raw.substr(14);
                result.success = false;
            }
            else
            {
                auto j = nlohmann::json::parse(raw);
                if (j.contains("response") && j["response"].is_string())
                {
                    result.response = j["response"].get<std::string>();
                    result.success  = true;
                }
                else
                {
                    result.success = false;
                    result.error   = "No 'response' field in Ollama reply";
                }
            }
        }
        catch (const std::exception& ex)
        {
            result.success = false;
            result.error   = ex.what();
        }
        catch (...)
        {
            result.success = false;
            result.error   = "Unknown error in worker thread";
        }

        if (result.success)
        {
            auto* pResult = new AIResult(std::move(result));
            if (!PostMessage(tp->hwndNotify, AI_MSG_RESULT,
                             0, reinterpret_cast<LPARAM>(pResult)))
                delete pResult;
        }
        else
        {
            auto* pErr = new AIResult(std::move(result));
            if (!PostMessage(tp->hwndNotify, AI_MSG_ERROR,
                             0, reinterpret_cast<LPARAM>(pErr)))
                delete pErr;
        }
    }

    delete tp;
    return 0;
}

// -------------------------------------------------------------------------
// generate  (async, non-streaming)
// -------------------------------------------------------------------------
void OllamaClient::generate(const std::string& prompt, HWND hwndNotify)
{
    auto* tp = new ThreadParam{ this, prompt, hwndNotify, false, _model, _endpoint };
    HANDLE h = CreateThread(nullptr, 0, workerThread, tp, 0, nullptr);
    if (h)
        CloseHandle(h);
    else
    {
        delete tp;
        auto* pErr = new AIResult{"", false, "Failed to create AI worker thread."};
        if (!PostMessage(hwndNotify, AI_MSG_ERROR, 0, reinterpret_cast<LPARAM>(pErr)))
            delete pErr;
    }
}

// -------------------------------------------------------------------------
// generateStream  (async, streaming)
// -------------------------------------------------------------------------
void OllamaClient::generateStream(const std::string& prompt, HWND hwndNotify)
{
    auto* tp = new ThreadParam{ this, prompt, hwndNotify, true, _model, _endpoint };
    HANDLE h = CreateThread(nullptr, 0, workerThread, tp, 0, nullptr);
    if (h)
        CloseHandle(h);
    else
    {
        delete tp;
        auto* pErr = new AIResult{"", false, "Failed to create AI worker thread."};
        if (!PostMessage(hwndNotify, AI_MSG_ERROR, 0, reinterpret_cast<LPARAM>(pErr)))
            delete pErr;
    }
}

#include "AIPanel.h"
#include "../resource.h"
#include <richedit.h>

#define WM_AI_MODELS_READY (WM_APP + 1702)

static std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}
static std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

AIPanel::AIPanel(OllamaClient* client)
    : DockingDlgInterface(IDD_AI_PANEL_DLG), _client(client) {}

void AIPanel::init(HINSTANCE hInst, HWND hParent) {
    LoadLibrary(L"Msftedit.dll");
    DockingDlgInterface::init(hInst, hParent);
}

INT_PTR CALLBACK AIPanel::run_dlgProc(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG:
        onInitDialog();
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_AI_SEND_BTN)
            onSendMessage();
        return TRUE;

    case AI_MSG_STREAM_CHUNK: {
        auto* pChunk = reinterpret_cast<std::string*>(lp);
        onStreamChunk(*pChunk);
        delete pChunk;
        return TRUE;
    }
    case AI_MSG_STREAM_DONE:
        onStreamDone();
        return TRUE;

    case AI_MSG_ERROR: {
        auto* pResult = reinterpret_cast<AIResult*>(lp);
        appendToHistory(L"Error", toWide(pResult->error));
        delete pResult;
        _streaming = false;
        return TRUE;
    }

    case WM_AI_MODELS_READY: {
        auto* models = reinterpret_cast<std::vector<std::wstring>*>(lp);
        HWND hCombo = GetDlgItem(_hSelf, IDC_AI_MODEL_COMBO);
        SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
        for (auto& m : *models)
            SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(m.c_str()));
        int idx = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT, (WPARAM)-1,
                                    reinterpret_cast<LPARAM>(_client->getModel().c_str()));
        SendMessage(hCombo, CB_SETCURSEL, max(0, idx), 0);
        delete models;
        return TRUE;
    }

    case WM_SIZE: {
        RECT rc; GetClientRect(_hSelf, &rc);
        int w = max(1, (int)rc.right);
        int h = max(1, (int)rc.bottom);
        int comboH = 22, chkH = 18, btnW = 56, inputH = max(40, h / 5);
        int histH  = max(1, h - inputH - comboH - chkH - 16);
        int inputW = max(1, w - btnW - 12);
        int y = 4;
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY), nullptr, 4, y, w-8, histH, SWP_NOZORDER);
        y += histH + 4;
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_MODEL_COMBO),  nullptr, 4, y, w-8, comboH, SWP_NOZORDER);
        y += comboH + 2;
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_INCL_SEL_CHK),  nullptr, 4,   y, 120, chkH, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_INCL_FILE_CHK), nullptr, 128, y, 130, chkH, SWP_NOZORDER);
        y += chkH + 2;
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_INPUT_EDIT), nullptr, 4,        y, inputW, inputH, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(_hSelf, IDC_AI_SEND_BTN),   nullptr, inputW+8, y, btnW,   inputH, SWP_NOZORDER);
        return TRUE;
    }
    }
    return DockingDlgInterface::run_dlgProc(msg, wp, lp);
}

void AIPanel::onInitDialog() {
    SendMessage(GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY), EM_SETREADONLY, TRUE, 0);
    populateModelCombo();
}

void AIPanel::populateModelCombo() {
    // Pre-fill with current model immediately (non-blocking)
    HWND hCombo = GetDlgItem(_hSelf, IDC_AI_MODEL_COMBO);
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(_client->getModel().c_str()));
    SendMessage(hCombo, CB_SETCURSEL, 0, 0);

    // Async fetch full model list
    auto* param = new ModelFetchParam{ _client, _hSelf };
    HANDLE h = CreateThread(nullptr, 0, modelFetchThread, param, 0, nullptr);
    if (h) CloseHandle(h);
    else delete param;
}

DWORD WINAPI AIPanel::modelFetchThread(LPVOID p) {
    auto* param = reinterpret_cast<ModelFetchParam*>(p);
    auto* models = new std::vector<std::wstring>(param->client->listModels());
    if (!PostMessage(param->hPanel, WM_AI_MODELS_READY, 0, reinterpret_cast<LPARAM>(models)))
        delete models;
    delete param;
    return 0;
}

std::string AIPanel::buildPrompt() {
    std::string prompt;
    for (auto& [role, text] : _history)
        prompt += toUtf8(role) + ": " + toUtf8(text) + "\n";
    prompt += "Assistant:";
    return prompt;
}

void AIPanel::onSendMessage() {
    if (_streaming) return;
    HWND hInput = GetDlgItem(_hSelf, IDC_AI_INPUT_EDIT);
    int len = GetWindowTextLength(hInput);
    if (len == 0) return;

    std::wstring input(len + 1, L'\0');
    GetWindowText(hInput, &input[0], len + 1);
    input.resize(len);
    SetWindowText(hInput, L"");

    HWND hCombo = GetDlgItem(_hSelf, IDC_AI_MODEL_COMBO);
    int sel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
    if (sel >= 0) {
        int mlen = (int)SendMessage(hCombo, CB_GETLBTEXTLEN, sel, 0);
        if (mlen > 0) {
            std::wstring model(mlen, L'\0');
            SendMessage(hCombo, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(&model[0]));
            _client->setModel(model);
        }
    }

    appendToHistory(L"You", input);
    std::string prompt = buildPrompt();
    _streaming = true;
    _pendingResponse.clear();

    HWND hChat = GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY);
    SendMessage(hChat, EM_SETREADONLY, FALSE, 0);
    SendMessage(hChat, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessage(hChat, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L"\r\nAI: "));
    SendMessage(hChat, EM_SETREADONLY, TRUE, 0);

    _client->generateStream(prompt, _hSelf);
}

void AIPanel::onStreamChunk(const std::string& chunk) {
    _pendingResponse += chunk;
    std::wstring wchunk = toWide(chunk);
    HWND hChat = GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY);
    SendMessage(hChat, EM_SETREADONLY, FALSE, 0);
    SendMessage(hChat, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessage(hChat, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(wchunk.c_str()));
    SendMessage(hChat, EM_SETREADONLY, TRUE, 0);
    SendMessage(hChat, WM_VSCROLL, SB_BOTTOM, 0);
}

void AIPanel::onStreamDone() {
    _history.push_back({ L"AI", toWide(_pendingResponse) });
    // Keep only last 20 history entries to prevent unbounded prompt growth
    if (_history.size() > 20)
        _history.erase(_history.begin(), _history.begin() + (_history.size() - 20));
    _pendingResponse.clear();
    _streaming = false;
    HWND hChat = GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY);
    SendMessage(hChat, EM_SETREADONLY, FALSE, 0);
    SendMessage(hChat, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessage(hChat, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L"\r\n"));
    SendMessage(hChat, EM_SETREADONLY, TRUE, 0);
}

void AIPanel::appendToHistory(const std::wstring& sender, const std::wstring& text) {
    _history.push_back({ sender, text });
    std::wstring line = L"\r\n" + sender + L": " + text;
    HWND hChat = GetDlgItem(_hSelf, IDC_AI_CHAT_HISTORY);
    SendMessage(hChat, EM_SETREADONLY, FALSE, 0);
    SendMessage(hChat, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessage(hChat, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
    SendMessage(hChat, EM_SETREADONLY, TRUE, 0);
    SendMessage(hChat, WM_VSCROLL, SB_BOTTOM, 0);
}

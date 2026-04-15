// PowerEditor/src/AIAssistant/AIResultDlg.cpp
#include "AIResultDlg.h"
#include "../resource.h"
#include "../../ScintillaComponent/ScintillaEditView.h"

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

AIResultDlg::AIResultDlg(HINSTANCE h, HWND p, OllamaClient* c, const std::string& pr, ScintillaEditView* v)
    : _hInst(h), _hParent(p), _client(c), _prompt(pr), _pView(v) {}

AIResultDlg::~AIResultDlg() { if (_hDlg) DestroyWindow(_hDlg); }

void AIResultDlg::show() {
    _hDlg = CreateDialogParam(_hInst, MAKEINTRESOURCE(IDD_AI_RESULT_DLG),
                               _hParent, dlgProc, reinterpret_cast<LPARAM>(this));
    if (!_hDlg) return;
    SetDlgItemText(_hDlg, IDC_AI_RESULT_EDIT, L"Asking AI...");
    EnableWindow(GetDlgItem(_hDlg, IDC_AI_INSERT_BTN), FALSE);
    EnableWindow(GetDlgItem(_hDlg, IDC_AI_REPLACE_BTN), FALSE);
    ShowWindow(_hDlg, SW_SHOW);
    _client->generate(_prompt, _hDlg);
}

INT_PTR CALLBACK AIResultDlg::dlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
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

INT_PTR AIResultDlg::handleMsg(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case AI_MSG_RESULT: {
        auto* r = reinterpret_cast<AIResult*>(lp);
        onResult(r); delete r; return TRUE;
    }
    case AI_MSG_ERROR: {
        auto* r = reinterpret_cast<AIResult*>(lp);
        SetDlgItemText(hDlg, IDC_AI_RESULT_EDIT, toWide(r->error).c_str());
        delete r; return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_AI_INSERT_BTN:  insertAtCursor();  return TRUE;
        case IDC_AI_REPLACE_BTN: replaceSelection(); return TRUE;
        case IDC_AI_CLOSE_BTN:
        case IDCANCEL:
            _hDlg = nullptr; DestroyWindow(hDlg); delete this; return TRUE;
        } break;
    case WM_SIZE: {
        RECT rc; GetClientRect(hDlg, &rc);
        int w = rc.right, h = rc.bottom;
        SetWindowPos(GetDlgItem(hDlg, IDC_AI_RESULT_EDIT), nullptr, 5, 5, w-10, h-50, SWP_NOZORDER);
        int by = h - 38;
        SetWindowPos(GetDlgItem(hDlg, IDC_AI_INSERT_BTN),  nullptr, 5,     by, 110, 24, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_AI_REPLACE_BTN), nullptr, 120,   by, 120, 24, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_AI_CLOSE_BTN),   nullptr, w-65,  by,  60, 24, SWP_NOZORDER);
        return TRUE;
    }
    }
    return FALSE;
}

void AIResultDlg::onResult(AIResult* r) {
    if (r->success) {
        _resultText = toWide(r->response);
        SetDlgItemText(_hDlg, IDC_AI_RESULT_EDIT, _resultText.c_str());
        EnableWindow(GetDlgItem(_hDlg, IDC_AI_INSERT_BTN), TRUE);
        EnableWindow(GetDlgItem(_hDlg, IDC_AI_REPLACE_BTN), TRUE);
    } else {
        SetDlgItemText(_hDlg, IDC_AI_RESULT_EDIT, toWide(r->error).c_str());
    }
}

void AIResultDlg::insertAtCursor() {
    if (!_pView || _resultText.empty()) return;
    _pView->execute(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(toUtf8(_resultText).c_str()));
}
void AIResultDlg::replaceSelection() { insertAtCursor(); }

void showAIResultDlg(HINSTANCE hInst, HWND hwndParent, OllamaClient* client,
                     const std::string& prompt, ScintillaEditView* pEditView) {
    (new AIResultDlg(hInst, hwndParent, client, prompt, pEditView))->show();
}

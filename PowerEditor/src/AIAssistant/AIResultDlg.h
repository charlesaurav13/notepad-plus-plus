// PowerEditor/src/AIAssistant/AIResultDlg.h
#pragma once
#include "OllamaClient.h"
#include <windows.h>
#include <string>

class ScintillaEditView;

// Floating resizable result dialog (Feature A).
// Self-owning: new'd on show, delete this on close.
class AIResultDlg
{
public:
    AIResultDlg(HINSTANCE hInst, HWND hParent, OllamaClient* client,
                const std::string& prompt, ScintillaEditView* pView);
    ~AIResultDlg();
    void show();

private:
    static INT_PTR CALLBACK dlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
    INT_PTR handleMsg(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
    void onResult(AIResult* r);
    void insertAtCursor();
    void replaceSelection();

    HINSTANCE          _hInst;
    HWND               _hParent;
    OllamaClient*      _client;
    std::string        _prompt;
    ScintillaEditView* _pView;
    HWND               _hDlg   = nullptr;
    std::wstring       _resultText;
};

void showAIResultDlg(HINSTANCE hInst, HWND hwndParent, OllamaClient* client,
                     const std::string& prompt, ScintillaEditView* pEditView);

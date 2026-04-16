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

    // Public so Notepad_plus can route AI_MSG_RESULT here
    void showGhostText(const std::string& completion);

    // Public so WM_TIMER handler can call it
    void requestCompletion();

private:
    static VOID CALLBACK debounceTimerProc(HWND, UINT, UINT_PTR id, DWORD);
    void clearGhostText();
    std::string buildPrompt();

    OllamaClient*       _client;
    ScintillaEditView** _ppView;
    HWND                _hNpp      = nullptr;
    UINT_PTR            _timerId   = 0;
    std::wstring        _ghostText;
    bool                _waiting   = false; // request in flight
    intptr_t            _ghostLine = -1;    // line where annotation was placed

    static InlineCompleter* _instance; // for timer callback
};

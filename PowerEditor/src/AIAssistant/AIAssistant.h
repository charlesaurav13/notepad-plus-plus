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

    // Called once from Notepad_plus init
    void init(HINSTANCE hInst, HWND hNpp, ScintillaEditView** ppEditView);

    // Toggle dockable chat panel visibility
    void togglePanel();

    // Feature A: fire an AI action on the current selection
    // actionPromptPrefix e.g. L"Explain this code:\n\n"
    void runSelectionAction(const std::wstring& actionPromptPrefix, HWND hwndParent);

    // Called from SCN_CHARADDED handler
    void onCharAdded(wchar_t ch);

    // Called after Preferences dialog saves
    void reloadSettings();

    bool isAutoCompleteEnabled() const;

    OllamaClient*    getClient()    { return _client.get(); }
    AIPanel*         getPanel()     { return _panel.get(); }
    InlineCompleter* getCompleter() { return _completer.get(); }

private:
    std::unique_ptr<OllamaClient>    _client;
    std::unique_ptr<AIPanel>         _panel;
    std::unique_ptr<InlineCompleter> _completer;

    HINSTANCE           _hInst      = nullptr;
    HWND                _hNpp       = nullptr;
    ScintillaEditView** _ppEditView = nullptr;
};

// PowerEditor/src/AIAssistant/AIAssistant.cpp
#include "AIAssistant.h"
#include "AIPanel.h"
#include "InlineCompleter.h"
#include "../Parameters.h"
#include "../ScintillaComponent/ScintillaEditView.h"
#include <string>

// Forward declaration — implemented in AIResultDlg.cpp (Task 5)
extern void showAIResultDlg(HINSTANCE hInst, HWND hwndParent, OllamaClient* client,
                             const std::string& prompt, ScintillaEditView* pEditView);

AIAssistant::AIAssistant() = default;
AIAssistant::~AIAssistant() = default;

void AIAssistant::init(HINSTANCE hInst, HWND hNpp, ScintillaEditView** ppEditView)
{
    _hInst      = hInst;
    _hNpp       = hNpp;
    _ppEditView = ppEditView;

    const NppAISettings& settings = NppParameters::getInstance().getNppGUI()._aiSettings;

    _client    = std::make_unique<OllamaClient>(settings.endpoint, settings.model);
    _panel     = std::make_unique<AIPanel>();
    _completer = std::make_unique<InlineCompleter>();

    if (_panel)
        _panel->init(hInst, hNpp);

    if (_completer)
        _completer->init(hNpp);

    // Check Ollama availability and report via status bar if not found
    if (!_client->isAvailable())
    {
        ::SendMessage(_hNpp, WM_SETTEXT, 0,
            reinterpret_cast<LPARAM>(L"AI: Ollama not found — start Ollama to enable AI features"));
    }
}

void AIAssistant::togglePanel()
{
    if (_panel)
        _panel->display(!_panel->isVisible());
}

void AIAssistant::runSelectionAction(const std::wstring& actionPromptPrefix, HWND hwndParent)
{
    if (!_ppEditView || !*_ppEditView)
        return;

    // Get selection length (includes null terminator)
    LRESULT len = (*_ppEditView)->execute(SCI_GETSELTEXT, 0, 0);
    if (len <= 1)
    {
        ::MessageBox(hwndParent,
            L"Please select some text first.",
            L"AI Assistant",
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Retrieve the selected text
    std::string selBuf(static_cast<size_t>(len), '\0');
    (*_ppEditView)->execute(SCI_GETSELTEXT, 0, reinterpret_cast<LPARAM>(selBuf.data()));
    // Remove the null terminator that Scintilla appended
    if (!selBuf.empty() && selBuf.back() == '\0')
        selBuf.pop_back();

    std::string prompt = OllamaClient::wstrToUtf8(actionPromptPrefix) + selBuf;

    showAIResultDlg(_hInst, hwndParent, _client.get(), prompt, *_ppEditView);
}

void AIAssistant::onCharAdded(wchar_t ch)
{
    if (_completer)
        _completer->onCharAdded(ch);
}

void AIAssistant::reloadSettings()
{
    const NppAISettings& settings = NppParameters::getInstance().getNppGUI()._aiSettings;

    if (_client)
    {
        _client->setModel(settings.model);
        _client->setEndpoint(settings.endpoint);
    }

    if (_completer)
        _completer->reloadSettings();
}

bool AIAssistant::isAutoCompleteEnabled() const
{
    return NppParameters::getInstance().getNppGUI()._aiSettings.autoCompleteOn;
}

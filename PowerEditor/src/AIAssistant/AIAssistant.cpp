// PowerEditor/src/AIAssistant/AIAssistant.cpp
#include "AIAssistant.h"
#include "AIPanel.h"
#include "AIResultDlg.h"
#include "InlineCompleter.h"
#include "../Parameters.h"
#include "../ScintillaComponent/ScintillaEditView.h"
#include <string>

struct AvailCheckParam { OllamaClient* client; HWND hNpp; };

static DWORD WINAPI availCheckThread(LPVOID p) {
    auto* param = reinterpret_cast<AvailCheckParam*>(p);
    if (!param->client->isAvailable())
        ::PostMessage(param->hNpp, WM_APP + 1701, 0, 0);
    delete param;
    return 0;
}

AIAssistant::AIAssistant() = default;
AIAssistant::~AIAssistant() = default;

void AIAssistant::init(HINSTANCE hInst, HWND hNpp, ScintillaEditView** ppEditView)
{
    _hInst      = hInst;
    _hNpp       = hNpp;
    _ppEditView = ppEditView;

    const NppAISettings& settings = NppParameters::getInstance().getNppGUI()._aiSettings;

    _client    = std::make_unique<OllamaClient>(settings.endpoint, settings.model);
    _panel     = std::make_unique<AIPanel>(_client.get());
    _completer = std::make_unique<InlineCompleter>(_client.get(), ppEditView);

    if (_panel)
        _panel->init(hInst, hNpp);

    if (_completer)
        _completer->init(hNpp);

    // Check Ollama availability asynchronously (off UI thread)
    auto* avp = new AvailCheckParam{ _client.get(), hNpp };
    HANDLE h = CreateThread(nullptr, 0, availCheckThread, avp, 0, nullptr);
    if (h) CloseHandle(h);
    else delete avp;
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

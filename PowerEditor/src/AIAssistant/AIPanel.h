#pragma once
#include "OllamaClient.h"
#include "../WinControls/DockingWnd/DockingDlgInterface.h"
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
    void appendToHistory(const std::wstring& sender, const std::wstring& text);
    void populateModelCombo();
    std::string buildPrompt();

    struct ModelFetchParam { OllamaClient* client; HWND hPanel; };
    static DWORD WINAPI modelFetchThread(LPVOID p);

    OllamaClient* _client;
    std::vector<std::pair<std::wstring, std::wstring>> _history;
    std::wstring  _pendingResponse;
    bool          _streaming = false;
};

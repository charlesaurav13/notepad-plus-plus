// PowerEditor/src/AIAssistant/AIResultDlg.h
#pragma once
#include "OllamaClient.h"
#include <windows.h>
#include <string>

class ScintillaEditView;

// Implemented in AIResultDlg.cpp (Task 5).
// Shows AI response in a floating dialog with Insert/Replace buttons.
void showAIResultDlg(HINSTANCE hInst, HWND hwndParent, OllamaClient* client,
                     const std::string& prompt, ScintillaEditView* pEditView);

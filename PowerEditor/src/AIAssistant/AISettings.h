// PowerEditor/src/AIAssistant/AISettings.h
#pragma once
#include <string>

struct NppAISettings
{
    std::wstring model             = L"deepseek-r1:7b";
    std::wstring endpoint          = L"http://localhost:11434";
    bool         autoCompleteOn    = true;
    int          autoCompleteDelayMs = 500;
    int          maxTokens         = 100;
};

// PowerEditor/src/AIAssistant/AISettings.h
#pragma once
#include <string>

struct NppAISettings
{
    std::wstring model             = L"deepseek-r1:7b";
    std::wstring endpoint          = L"http://192.168.1.25:11434";
    bool         autoCompleteOn    = true;
    int          autoCompleteDelayMs = 500;
    int          maxTokens         = 100;
};

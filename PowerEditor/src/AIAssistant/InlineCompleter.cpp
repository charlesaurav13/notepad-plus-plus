// PowerEditor/src/AIAssistant/InlineCompleter.cpp
#include "InlineCompleter.h"
#include "../resource.h"
#include "../Parameters.h"
#include "../ScintillaComponent/ScintillaEditView.h"

// Unique timer ID for the debounce timer
static constexpr UINT_PTR AI_DEBOUNCE_TIMER_ID = 0xA101;

InlineCompleter* InlineCompleter::_instance = nullptr;

static std::wstring u8w(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static std::string wu8(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

InlineCompleter::InlineCompleter(OllamaClient* client, ScintillaEditView** ppView)
    : _client(client), _ppView(ppView)
{
    _instance = this;
}

InlineCompleter::~InlineCompleter()
{
    if (_timerId && _hNpp)
        KillTimer(_hNpp, _timerId);
    if (_instance == this)
        _instance = nullptr;
}

void InlineCompleter::init(HWND hNpp)
{
    _hNpp = hNpp;
}

void InlineCompleter::reloadSettings()
{
    // Settings are re-read from NppParameters on each use — nothing to cache
}

void InlineCompleter::onCharAdded(wchar_t)
{
    const NppAISettings& s = NppParameters::getInstance().getNppGUI()._aiSettings;
    if (!s.autoCompleteOn) return;

    clearGhostText();
    _waiting = false;

    // Reset debounce timer
    if (_timerId && _hNpp)
        KillTimer(_hNpp, _timerId);
    _timerId = 0;

    // SetTimer with debounceTimerProc: fires on the main window's message loop.
    _timerId = SetTimer(_hNpp, AI_DEBOUNCE_TIMER_ID, static_cast<UINT>(s.autoCompleteDelayMs), debounceTimerProc);
}

VOID CALLBACK InlineCompleter::debounceTimerProc(HWND hWnd, UINT, UINT_PTR id, DWORD)
{
    KillTimer(hWnd, id);
    if (_instance)
    {
        _instance->_timerId = 0;
        _instance->requestCompletion();
    }
}

void InlineCompleter::triggerNow()
{
    if (_timerId && _hNpp) { KillTimer(_hNpp, _timerId); _timerId = 0; }
    clearGhostText();
    requestCompletion();
}

void InlineCompleter::requestCompletion()
{
    if (_waiting) return;
    _waiting = true;

    std::string prompt = buildPrompt();
    if (prompt.empty()) { _waiting = false; return; }

    // Async: result comes back as AI_MSG_RESULT posted to _hNpp
    _client->generate(prompt, _hNpp);
}

std::string InlineCompleter::buildPrompt()
{
    if (!_ppView || !*_ppView) return {};
    ScintillaEditView* view = *_ppView;

    intptr_t curPos  = view->execute(SCI_GETCURRENTPOS);
    intptr_t curLine = view->execute(SCI_LINEFROMPOSITION, curPos);

    // Grab up to 10 lines of context before cursor
    intptr_t startLine = (curLine > 10) ? (curLine - 10) : 0;
    intptr_t startPos  = view->execute(SCI_POSITIONFROMLINE, startLine);

    intptr_t contextLen = curPos - startPos;
    if (contextLen <= 0) return {};

    std::string context(static_cast<size_t>(contextLen) + 1, '\0');
    Sci_TextRangeFull tr{};
    tr.chrg.cpMin = static_cast<Sci_Position>(startPos);
    tr.chrg.cpMax = static_cast<Sci_Position>(curPos);
    tr.lpstrText  = &context[0];
    view->execute(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<LPARAM>(&tr));
    context.resize(static_cast<size_t>(contextLen));

    const NppAISettings& s = NppParameters::getInstance().getNppGUI()._aiSettings;

    return "Continue the following code. Output ONLY the completion text with no explanation, "
           "no markdown, no code fences. Max " + std::to_string(s.maxTokens) + " tokens.\n\n"
           + context;
}

void InlineCompleter::showGhostText(const std::string& completion)
{
    _waiting = false;

    if (!_ppView || !*_ppView || completion.empty()) return;

    ScintillaEditView* view = *_ppView;
    _ghostText = u8w(completion);

    intptr_t curLine = view->execute(SCI_LINEFROMPOSITION, view->execute(SCI_GETCURRENTPOS));

    std::string annotText = wu8(_ghostText);
    view->execute(SCI_ANNOTATIONSETTEXT,    curLine, reinterpret_cast<LPARAM>(annotText.c_str()));
    view->execute(SCI_ANNOTATIONSETSTYLE,   curLine, STYLE_DEFAULT);
    view->execute(SCI_ANNOTATIONSETVISIBLE, ANNOTATION_STANDARD);
}

void InlineCompleter::clearGhostText()
{
    if (_ghostText.empty()) return;
    if (_ppView && *_ppView)
    {
        ScintillaEditView* view = *_ppView;
        intptr_t curLine = view->execute(SCI_LINEFROMPOSITION, view->execute(SCI_GETCURRENTPOS));
        view->execute(SCI_ANNOTATIONSETTEXT,    curLine, reinterpret_cast<LPARAM>(""));
        view->execute(SCI_ANNOTATIONSETVISIBLE, ANNOTATION_HIDDEN);
    }
    _ghostText.clear();
}

bool InlineCompleter::onTab()
{
    if (_ghostText.empty()) return false;
    if (!_ppView || !*_ppView) return false;

    ScintillaEditView* view = *_ppView;
    std::string utf8 = wu8(_ghostText);
    clearGhostText();
    view->execute(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(utf8.c_str()));
    return true;
}

bool InlineCompleter::onEscape()
{
    if (_ghostText.empty() && !_waiting) return false;
    clearGhostText();
    _waiting = false;
    if (_timerId && _hNpp) { KillTimer(_hNpp, _timerId); _timerId = 0; }
    return true;
}

#include <windows.h>
#include <shellapi.h>
#include <UIAutomation.h>
#include <ole2.h>
#include <atlbase.h>
#include <atlcom.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <memory>

#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uiautomationcore.lib")
#pragma comment(lib, "shell32.lib")

// ============================================================================
// Logging and Utilities
// ============================================================================

void Log(const std::wstring& message, bool isError = false)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (isError)
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
    else
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

    std::wcout << (isError ? L"[ERROR] " : L"[INFO] ") << message << std::endl;

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

std::wstring ConvertToWString(const std::string& str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
}


namespace Keyboard
{
    void SendKey(WORD vk, bool down)
    {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;

        if (!down)
            input.ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(1, &input, sizeof(INPUT));
    }

    void PressKey(WORD vk)
    {
        SendKey(vk, true);
        SendKey(vk, false);
    }

    void PressCtrlL()
    {
        SendKey(VK_CONTROL, true);
        PressKey('L');
        SendKey(VK_CONTROL, false);
    }

    void TypeText(const std::wstring& text)
    {
        for (wchar_t ch : text)
        {
            INPUT inputs[2] = {};

            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
            inputs[0].ki.wScan = ch;

            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            inputs[1].ki.wScan = ch;

            SendInput(2, inputs, sizeof(INPUT));
        }
    }
}

bool NavigateEdgeToUrl(HWND edgeWindow, const std::wstring& url)
{
    if (!edgeWindow || !IsWindow(edgeWindow))
        return false;

    // Restore if minimized
    ShowWindow(edgeWindow, SW_RESTORE);

    // Bring to front
    SetForegroundWindow(edgeWindow);
    SetActiveWindow(edgeWindow);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Focus address bar
    Keyboard::PressCtrlL();

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Type URL
    Keyboard::TypeText(url);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Navigate
    Keyboard::PressKey(VK_RETURN);

    return true;
}


// ============================================================================
// Window Management
// ============================================================================

struct WindowInfo
{
    HWND handle;
    std::wstring title;
    std::wstring className;
};

std::vector<WindowInfo> EnumerateEdgeWindows()
{
    std::vector<WindowInfo> edgeWindows;
    
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowExW(nullptr, hwnd, L"Chrome_WidgetWin_1", nullptr)) != nullptr)
    {
        WindowInfo info;
        info.handle = hwnd;

        // Get window title
        wchar_t titleBuffer[256];
        GetWindowTextW(hwnd, titleBuffer, sizeof(titleBuffer) / sizeof(titleBuffer[0]));
        info.title = titleBuffer;

        // Get class name
        wchar_t classBuffer[256];
        GetClassNameW(hwnd, classBuffer, sizeof(classBuffer) / sizeof(classBuffer[0]));
        info.className = classBuffer;

        edgeWindows.push_back(info);
    }

    return edgeWindows;
}

HWND LaunchEdgeAndWait(const wchar_t* url, int maxWaitSeconds = 15)
{
    Log(L"Launching Microsoft Edge...");

    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.lpVerb = L"open";
    sei.lpFile = L"msedge.exe";
    sei.lpParameters = url;
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei))
    {
        Log(L"Failed to launch Edge", true);
        return nullptr;
    }

    Log(L"Waiting for Edge window to appear...");

    for (int i = 0; i < maxWaitSeconds * 10; i++)
    {
        std::vector<WindowInfo> windows = EnumerateEdgeWindows();
        if (!windows.empty())
        {
            Log(std::wstring(L"Found Edge window: ") + windows[0].title);
            return windows[0].handle;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    Log(L"Timeout waiting for Edge window", true);
    return nullptr;
}

void CloseEdge()
{
    Log(L"Closing Edge...");
    std::vector<WindowInfo> windows = EnumerateEdgeWindows();
    
    for (const auto& window : windows)
    {
        if (PostMessageW(window.handle, WM_CLOSE, 0, 0))
        {
            Log(std::wstring(L"Sent close message to: ") + window.title);
        }
    }

    // Wait for Edge to close
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

// ============================================================================
// UI Automation
// ============================================================================

class UIAutomationHelper
{
private:
    IUIAutomation* automation;
    IUIAutomationElement* rootElement;
    IUIAutomationTreeWalker* walker;

public:
    UIAutomationHelper() : automation(nullptr), rootElement(nullptr), walker(nullptr) {}

    ~UIAutomationHelper()
    {
        Cleanup();
    }

    bool Initialize(HWND targetWindow)
    {
        HRESULT hr = CoCreateInstance(
            CLSID_CUIAutomation,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&automation));

        if (FAILED(hr))
        {
            Log(L"Failed to create UI Automation instance", true);
            return false;
        }

        hr = automation->ElementFromHandle(targetWindow, &rootElement);
        if (FAILED(hr) || !rootElement)
        {
            Log(L"Failed to get root element from window", true);
            return false;
        }

        hr = automation->get_ContentViewWalker(&walker);
        if (FAILED(hr) || !walker)
        {
            Log(L"Failed to create tree walker", true);
            return false;
        }

        Log(L"UI Automation initialized successfully");
        return true;
    }

boolean SetSearchEngineAsDefault(const std::wstring& searchEngineName)
{
    IUIAutomationElement* root = nullptr;
    automation->GetRootElement(&root);

    if (!root)
        return false;

    // Find all DataItems (search engine rows)
    VARIANT var;
    var.vt = VT_I4;
    var.lVal = UIA_DataItemControlTypeId;

    IUIAutomationCondition* rowCondition = nullptr;
    automation->CreatePropertyCondition(
        UIA_ControlTypePropertyId,
        var,
        &rowCondition);

    IUIAutomationElementArray* rows = nullptr;
    root->FindAll(
        TreeScope_Descendants,
        rowCondition,
        &rows);

    rowCondition->Release();

    if (!rows)
    {
        root->Release();
        return false;
    }

    int rowCount = 0;
    rows->get_Length(&rowCount);
    bool isMakeDefaultFound = true;

    for (int i = 0; i < rowCount; i++)
    {
        IUIAutomationElement* row = nullptr;
        rows->GetElement(i, &row);

        if (!row)
            continue;

        BSTR rowName = nullptr;
        row->get_CurrentName(&rowName);

        bool isTarget =
            rowName &&
            wcsstr(rowName, searchEngineName.c_str());

        if (rowName)
            SysFreeString(rowName);

        if (!isTarget)
        {
            row->Release();
            continue;
        }

        std::wcout << L"Found row for "
                   << searchEngineName
                   << std::endl;

        //--------------------------------------------------
        // Find actual More Actions BUTTON
        //--------------------------------------------------

        VARIANT nameVar;
        nameVar.vt = VT_BSTR;
        nameVar.bstrVal = SysAllocString(L"More actions");

        IUIAutomationCondition* moreActionsCondition = nullptr;

        automation->CreatePropertyCondition(
            UIA_NamePropertyId,
            nameVar,
            &moreActionsCondition);

        SysFreeString(nameVar.bstrVal);

        IUIAutomationElement* moreActionsButton = nullptr;

        row->FindFirst(
            TreeScope_Descendants,
            moreActionsCondition,
            &moreActionsButton);

        moreActionsCondition->Release();

        if (!moreActionsButton)
        {
            std::wcout << L"More actions button not found"
                       << std::endl;

            row->Release();
            continue;
        }

        //--------------------------------------------------
        // Click actual 3-dot button
        //--------------------------------------------------

        RECT rect{};
        moreActionsButton->get_CurrentBoundingRectangle(&rect);

        int x = rect.right - 10; // near dots
        int y = (rect.top + rect.bottom) / 2;

        SetCursorPos(x, y);

        INPUT click[2]{};

        click[0].type = INPUT_MOUSE;
        click[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

        click[1].type = INPUT_MOUSE;
        click[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

        SendInput(2, click, sizeof(INPUT));

        //--------------------------------------------------
        // Find Make Default menu item
        //--------------------------------------------------

        VARIANT makeVar;
        makeVar.vt = VT_BSTR;
        makeVar.bstrVal = SysAllocString(L"Make default");

        IUIAutomationCondition* makeCondition = nullptr;

        automation->CreatePropertyCondition(
            UIA_NamePropertyId,
            makeVar,
            &makeCondition);

        SysFreeString(makeVar.bstrVal);

        IUIAutomationElement* makeDefault = nullptr;

        for (int retry = 0; retry < 10; retry++)
        {
            root->FindFirst(
                TreeScope_Descendants,
                makeCondition,
                &makeDefault);

            if (makeDefault)
                break;

            Sleep(300);
        }

        makeCondition->Release();

        if (!makeDefault)
        {
            std::wcout << L"Make default not found"
                       << std::endl;

            row->Release();
            isMakeDefaultFound = false;
            break;
        }

        std::wcout << L"Found Make default"
                   << std::endl;

        //--------------------------------------------------
        // Try InvokePattern
        //--------------------------------------------------

        IUIAutomationInvokePattern* invoke = nullptr;

        HRESULT hr =
            makeDefault->GetCurrentPattern(
                UIA_InvokePatternId,
                (IUnknown**)&invoke);

        if (SUCCEEDED(hr) && invoke)
        {
            invoke->Invoke();
            invoke->Release();
        }
        else
        {
            RECT r{};
            makeDefault->get_CurrentBoundingRectangle(&r);

            int mx = (r.left + r.right) / 2;
            int my = (r.top + r.bottom) / 2;

            SetCursorPos(mx, my);

            INPUT click2[2] = {};

            click2[0].type = INPUT_MOUSE;
            click2[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

            click2[1].type = INPUT_MOUSE;
            click2[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

            SendInput(2, click2, sizeof(INPUT));
        }

        makeDefault->Release();
        row->Release();

        break;
    }

    rows->Release();
    root->Release();
    return isMakeDefaultFound;
}

    void Cleanup()
    {
        if (walker)
        {
            walker->Release();
            walker = nullptr;
        }
        if (rootElement)
        {
            rootElement->Release();
            rootElement = nullptr;
        }
        if (automation)
        {
            automation->Release();
            automation = nullptr;
        }
    }
};

// ============================================================================
// Main Automation Workflow
// ============================================================================

bool AutomateSearchEngineChange(const std::wstring& searchEngineName)
{
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        Log(L"Failed to initialize COM", true);
        return false;
    }

    bool success = false;

    try
    {
        // Step 1: Launch Edge
        std::wstring url = L"edge://settings/privacy/services/search/searchEngines";
        HWND edgeWindow = LaunchEdgeAndWait(url.c_str());
        if (edgeWindow)
        {
            NavigateEdgeToUrl(edgeWindow, url);
        }

        if (!edgeWindow)
        {
            Log(L"Failed to launch Edge", true);
            throw std::runtime_error("Edge window not found");
        }

        UIAutomationHelper uiHelper;
        if (!uiHelper.Initialize(edgeWindow))
        {
            Log(L"Failed to initialize UI Automation", true);
            throw std::runtime_error("UI Automation initialization failed");
        }

        Log(L"Waiting for settings page to load...");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        uiHelper.SetSearchEngineAsDefault(searchEngineName);
        success = true;
    }
    catch (const std::exception& ex)
    {
        Log(std::wstring(L"Exception: ") + ConvertToWString(ex.what()), true);
        success = false;
    }
    catch (...)
    {
        Log(L"Unknown exception occurred", true);
        success = false;
    }

    CoUninitialize();
    return success;
}


// ============================================================================
// Entry Point
// ============================================================================

int main()
{
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"Edge Search Engine Automation Tool" << std::endl;
    std::wcout << L"========================================" << std::endl << std::endl;
    // std::wstring searchEngineName;

    // std::wcout << L"Enter Search Engine Name: ";
    // std::getline(std::wcin, searchEngineName);

    // if (searchEngineName.empty())
    // {
    //     std::wcerr << L"Search engine name cannot be empty." << std::endl;
    //     return false;
    // }
    //bool result = AutomateSearchEngineChange(searchEngineName);
    bool result = AutomateSearchEngineChange(L"Yahoo");

    std::wcout << std::endl << L"========================================" <<std::endl;
    if (result)
    {
        Log(L"Automation completed successfully!");
    }
    else
    {
        Log(L"Automation failed", true);
    }
    std::wcout << L"========================================" << std::endl;

    return result ? 0 : 1;
}
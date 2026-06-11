#include "pch.h"

#include <windows.h>
#include <shellapi.h>

#include <fstream>
#include <string>
#include <chrono>
#include <ctime>

void PressKey(WORD key)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    SendInput(1, &input, sizeof(INPUT));

    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void KeyDown(WORD key)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    SendInput(1, &input, sizeof(INPUT));
}

void KeyUp(WORD key)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void TypeText(const wchar_t* text)
{
    while (*text)
    {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.dwFlags = KEYEVENTF_UNICODE;
        input.ki.wScan = *text;
        SendInput(1, &input, sizeof(INPUT));

        input.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
        text++;
    }
}

std::string GetCurrentDateTime()
{
    time_t now = time(nullptr);
    char buffer[100];
    ctime_s(buffer, sizeof(buffer), &now);
    std::string result(buffer);
    if (!result.empty() && result.back() == '\n')
    {
        result.pop_back();
    }
    return result;
}

void WriteLog(const std::string& message)
{
    char* appData = nullptr;
    size_t len = 0;
    _dupenv_s(&appData, &len, "APPDATA");

    std::string folder = std::string(appData) + "\\EdgeAutomation";
    CreateDirectoryA(folder.c_str(), NULL);

    std::ofstream logFile(folder + "\\AutomationLog.txt", std::ios::app);
    logFile << message << std::endl;
    free(appData);
}

extern "C" __declspec(dllexport)
bool RunAutomation()
{
    auto startTime = std::chrono::steady_clock::now();

    WriteLog("================================");
    WriteLog("Automation Started : " + GetCurrentDateTime());

    try
    {
        // Validate Edge Installation
        DWORD edgeExists = GetFileAttributesW(L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe");
        if (edgeExists == INVALID_FILE_ATTRIBUTES)
        {
            WriteLog("FAILED : Edge browser not found");
            return false;
        }

        // Open Edge
        HINSTANCE result = ShellExecuteW(NULL, L"open", L"msedge.exe", NULL, NULL, SW_SHOW);
        if ((INT_PTR)result <= 32)
        {
            WriteLog("FAILED : Unable to launch Edge");
            return false;
        }

        Sleep(5000);

        // --- STEP 1: Focus Address Bar ---
        KeyDown(VK_CONTROL);
        PressKey('L');
        KeyUp(VK_CONTROL);
        Sleep(1000);

        // --- STEP 2: Navigate to target URL ---
        TypeText(L"edge://settings/privacy/services/search/searchEngines");
        Sleep(1000);
        PressKey(VK_RETURN);

        // Wait for settings UI DOM to load completely
        Sleep(6000);

        // --- STEP 3: Navigate to the inner search bar ---
        PressKey(VK_TAB);
        Sleep(400);
        PressKey(VK_TAB);
        Sleep(400);
        PressKey(VK_TAB);
        Sleep(400);
        PressKey(VK_TAB);
        Sleep(1000);

        // --- STEP 4: Type 'yahoo' in the target search box ---
        TypeText(L"yahoo");
        Sleep(2000);

        // --- STEP 5: Migrate focus out of the search box via Arrow Key ---
        // Pressing Arrow Down explicitly escapes the search box input field focus 
        // context and moves focus onto the active matching grid row underneath it.
        PressKey(VK_DOWN);
        Sleep(800);

        PressKey(VK_DOWN);
        Sleep(800);

        // --- STEP 6: Jump directly onto the circled "..." button ---
        // Now that the row is focused, a single TAB lands directly on the "..." action item.
        PressKey(VK_TAB);
        Sleep(800);
        PressKey(VK_TAB);
        Sleep(500);
        PressKey(VK_RIGHT);
        Sleep(800);
        PressKey(VK_RIGHT);
        Sleep(800);
        PressKey(VK_RIGHT);
        Sleep(800);

        // 6. Press Enter to open the context menu (Make default / Edit / Remove)
        PressKey(VK_RETURN);
        Sleep(800);

        // 7. "Make default" is the first item in the menu — press Enter to select it
        //    Confirmed from screenshot: Make default is highlighted at top
        PressKey(VK_RETURN);
        Sleep(2000);

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);

        WriteLog("Status : SUCCESS");
        WriteLog("Execution Time : " + std::to_string(duration.count()) + " seconds");
        WriteLog("Automation Completed : " + GetCurrentDateTime());

        return true;
    }
    catch (...)
    {
        WriteLog("Status : FAILED");
        WriteLog("Unexpected Exception : " + GetCurrentDateTime());
        return false;
    }
}
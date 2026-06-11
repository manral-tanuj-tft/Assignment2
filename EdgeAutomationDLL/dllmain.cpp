#include "pch.h"

#include <windows.h>
#include <shellapi.h>

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

        input.ki.dwFlags =
            KEYEVENTF_UNICODE |
            KEYEVENTF_KEYUP;

        SendInput(1, &input, sizeof(INPUT));

        text++;
    }
}

extern "C" __declspec(dllexport)
bool RunAutomation()
{
    try
    {
        DWORD edgeExists =
            GetFileAttributesW(
                L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe");

        if (edgeExists ==
            INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }

        HINSTANCE result =
            ShellExecuteW(
                NULL,
                L"open",
                L"msedge.exe",
                NULL,
                NULL,
                SW_SHOW);

        if ((INT_PTR)result <= 32)
        {
            return false;
        }

        Sleep(5000);

        
        KeyDown(VK_CONTROL);
        PressKey('L');
        KeyUp(VK_CONTROL);

        Sleep(1000);

        // Open Search Engines page
        TypeText(
            L"edge://settings/privacy/services/search/searchEngines");

        Sleep(500);

        PressKey(VK_RETURN);

        Sleep(5000);

        
        for (int i = 0; i < 4; i++)
        {
            PressKey(VK_TAB);
            Sleep(500);
        }

        
        TypeText(L"yahoo");

        Sleep(2000);

        
        for (int i = 0; i < 2; i++)
        {
            PressKey(VK_TAB);
            Sleep(500);
        }

        
        for (int i = 0; i < 3; i++)
        {
            PressKey(VK_RIGHT);
            Sleep(500);
        }

       
        PressKey(VK_RETURN);

        Sleep(1000);

        // Make Default
        PressKey(VK_RETURN);

        Sleep(3000);

        return true;
    }
    catch (...)
    {
        return false;
    }
}
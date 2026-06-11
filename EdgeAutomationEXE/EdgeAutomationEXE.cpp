#include <windows.h>

#include <fstream>
#include <string>
#include <chrono>
#include <ctime>

typedef bool(*RunAutomationFunc)();

std::string GetCurrentDateTime()
{
    time_t now = time(nullptr);

    char buffer[100];

    ctime_s(buffer, sizeof(buffer), &now);

    std::string result(buffer);

    if (!result.empty() &&
        result.back() == '\n')
    {
        result.pop_back();
    }

    return result;
}

void WriteLog(const std::string& message)
{
    char* appData = nullptr;
    size_t len = 0;

    _dupenv_s(
        &appData,
        &len,
        "APPDATA");

    std::string folder =
        std::string(appData) +
        "\\EdgeAutomation";

    CreateDirectoryA(
        folder.c_str(),
        NULL);

    std::ofstream logFile(
        folder + "\\AutomationLog.txt",
        std::ios::app);

    logFile << message << std::endl;

    free(appData);
}

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nShowCmd)
{
    auto startTime =
        std::chrono::steady_clock::now();

    WriteLog("================================");

    WriteLog(
        "Start Time : "
        + GetCurrentDateTime());

    HMODULE hDll =
        LoadLibraryW(
            L"EdgeAutomationDLL.dll");

    if (!hDll)
    {
        WriteLog("Status : FAILED");
        return 1;
    }

    RunAutomationFunc runAutomation =
        (RunAutomationFunc)
        GetProcAddress(
            hDll,
            "RunAutomation");

    if (!runAutomation)
    {
        WriteLog("Status : FAILED");

        FreeLibrary(hDll);

        return 1;
    }

    bool result =
        runAutomation();

    if (result)
    {
        WriteLog("Status : SUCCESS");
    }
    else
    {
        WriteLog("Status : FAILED");
    }

    auto endTime =
        std::chrono::steady_clock::now();

    auto duration =
        std::chrono::duration_cast<
        std::chrono::seconds>(
            endTime - startTime);

    WriteLog(
        "End Time : "
        + GetCurrentDateTime());

    WriteLog(
        "Total Time Taken : "
        + std::to_string(
            duration.count())
        + " seconds");

    FreeLibrary(hDll);

    return result ? 0 : 1;
}
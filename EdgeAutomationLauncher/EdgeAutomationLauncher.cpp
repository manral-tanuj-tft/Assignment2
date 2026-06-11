#include <windows.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <string>
#include <sstream>
#include <shlobj.h>

// Helper function to generate a clean, readable timestamp
std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    struct tm timeInfo;
    localtime_s(&timeInfo, &in_time_t);
    ss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Handles logging requirements strictly within the EXE as requested
void WriteLog(const std::string& startTime, const std::string& endTime, bool success, double duration) {
    wchar_t appDataPath[MAX_PATH];

    // Grabs the non-admin safe Local AppData file path context
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::wstring wsFolder(appDataPath);
        std::wstring wsTargetDir = wsFolder + L"\\EdgeAutomation";
        std::wstring wsLogFile = wsTargetDir + L"\\automation.log";

        // Create directory structures if they are missing
        SHCreateDirectory(NULL, wsTargetDir.c_str());

        std::ofstream logFile(wsLogFile, std::ios_base::app);
        if (logFile.is_open()) {
            logFile << "========================================\n";
            logFile << "Start Time:         " << startTime << "\n";
            logFile << "End Time:           " << endTime << "\n";
            logFile << "Status:             " << (success ? "SUCCESS" : "FAILURE") << "\n";
            logFile << "Total Time Taken:   " << std::fixed << std::setprecision(2) << duration << " seconds\n";
            logFile << "========================================\n\n";
            logFile.close();
        }
    }
}

int main() {
    std::string startTime = GetCurrentTimestamp();
    auto startClock = std::chrono::high_resolution_clock::now();
    bool executionStatus = false;

    // 1. Dynamically look for the local engine DLL
    HMODULE hEngineDll = LoadLibraryW(L"EdgeAutomationEngine.dll");

    if (hEngineDll != NULL) {
        // MATCHED: Removed __stdcall to perfectly match the DLL's __cdecl export
        typedef bool(*_RunAutomation)();
        _RunAutomation RunAutomation = (_RunAutomation)GetProcAddress(hEngineDll, "RunAutomation");

        if (RunAutomation != NULL) {
            // 2. Execute the automation routine and capture return status
            executionStatus = RunAutomation();
        }
        else {
            std::cerr << "Error: 'RunAutomation' export signature not found in DLL.\n";
        }
        FreeLibrary(hEngineDll);
    }
    else {
        std::cerr << "Error: EdgeAutomationEngine.dll missing from the launcher directory.\n";
    }

    auto endClock = std::chrono::high_resolution_clock::now();
    std::string endTime = GetCurrentTimestamp();
    std::chrono::duration<double> elapsed = endClock - startClock;

    // 3. Log execution tracking data cleanly
    WriteLog(startTime, endTime, executionStatus, elapsed.count());

    return 0;
}
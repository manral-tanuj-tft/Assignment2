#pragma once

// Removed WIN32_LEAN_AND_MEAN — it strips COM/OLE headers that
// UIAutomationClient.h depends on (interface, HRESULT, GUID, IUnknown)

#include <windows.h>
#include <objbase.h>            // CoInitializeEx, CoCreateInstance, 'interface' macro
#include <shellapi.h>           // ShellExecuteW
#include <UIAutomationClient.h> // IUIAutomation, CLSID_CUIAutomation8, IID_IUIAutomation
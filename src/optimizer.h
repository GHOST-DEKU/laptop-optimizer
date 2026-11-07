#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <shlobj.h>
#include <shellapi.h>

struct OptimizationItem {
    int id;
    std::wstring name;
    std::wstring description;
    std::wstring benefit;
    bool enabled;
    bool currentStatus; // true = already optimized
};

class LaptopOptimizerEngine {
public:
    static bool SetRegistryDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& valueName, DWORD data) {
        HKEY hKey;
        if (RegCreateKeyExW(hRoot, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            LSTATUS status = RegSetValueExW(hKey, valueName.c_str(), 0, REG_DWORD, (const BYTE*)&data, sizeof(data));
            RegCloseKey(hKey);
            return status == ERROR_SUCCESS;
        }
        return false;
    }

    static bool SetRegistryString(HKEY hRoot, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& data) {
        HKEY hKey;
        if (RegCreateKeyExW(hRoot, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            LSTATUS status = RegSetValueExW(hKey, valueName.c_str(), 0, REG_SZ, (const BYTE*)data.c_str(), (DWORD)((data.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
            return status == ERROR_SUCCESS;
        }
        return false;
    }

    static DWORD GetRegistryDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& valueName, DWORD defaultVal = 0) {
        HKEY hKey;
        DWORD val = defaultVal;
        if (RegOpenKeyExW(hRoot, subKey.c_str(), 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
            DWORD type = 0, size = sizeof(DWORD);
            RegQueryValueExW(hKey, valueName.c_str(), NULL, &type, (LPBYTE)&val, &size);
            RegCloseKey(hKey);
        }
        return val;
    }

    static std::wstring GetRegistryString(HKEY hRoot, const std::wstring& subKey, const std::wstring& valueName) {
        HKEY hKey;
        wchar_t buf[256] = {0};
        if (RegOpenKeyExW(hRoot, subKey.c_str(), 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
            DWORD size = sizeof(buf);
            RegQueryValueExW(hKey, valueName.c_str(), NULL, NULL, (LPBYTE)buf, &size);
            RegCloseKey(hKey);
        }
        return std::wstring(buf);
    }

    static bool ExecuteCommand(const std::wstring& command, bool wait = true) {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::vector<wchar_t> cmdBuf(command.begin(), command.end());
        cmdBuf.push_back(0);

        if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            if (wait) {
                WaitForSingleObject(pi.hProcess, 15000);
            }
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        }
        return false;
    }

    // --- Optimization Implementations ---

    static bool OptimizePowerPlan() {
        // Duplicate High Performance template (8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c) and set active
        ExecuteCommand(L"powercfg -duplicatescheme 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c", true);
        return ExecuteCommand(L"powercfg -setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c", true) ||
               ExecuteCommand(L"powercfg -setactive 36779f97-38bc-4a49-95c2-71efe9e2771f", true);
    }

    static bool OptimizeGameDVR() {
        bool ok = true;
        ok &= SetRegistryDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_Enabled", 0);
        ok &= SetRegistryDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_FSEBehaviorMode", 2);
        ok &= SetRegistryDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_HonorUserFSEBehaviorMode", 1);
        ok &= SetRegistryDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", L"AppCaptureEnabled", 0);
        ok &= SetRegistryDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", L"HistoricalCaptureEnabled", 0);
        return ok;
    }

    static bool OptimizeMenuDelay() {
        return SetRegistryString(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"MenuShowDelay", L"20");
    }

    static bool OptimizeWidgets() {
        SetRegistryDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"TaskbarDa", 0);
        ExecuteCommand(L"taskkill /f /im WidgetService.exe", false);
        return ExecuteCommand(L"winget uninstall MicrosoftWindows.Client.WebExperience_cw5n1h2txyewy --silent --accept-source-agreements", true);
    }

    static bool OptimizeBingSearch() {
        bool ok = SetRegistryDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Search", L"BingSearchEnabled", 0);
        ExecuteCommand(L"taskkill /f /im SearchHost.exe", false);
        return ok;
    }

    static bool OptimizeDeliveryOptimization() {
        return SetRegistryDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\DeliveryOptimization", L"DODownloadMode", 0);
    }

    static bool OptimizeFastStartup() {
        return ExecuteCommand(L"powercfg -h off", true);
    }

    static bool OptimizeTempFiles() {
        wchar_t tempPath[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempPath)) {
            std::wstring searchPath = std::wstring(tempPath) + L"*";
            WIN32_FIND_DATAW findData;
            HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                        std::wstring filePath = std::wstring(tempPath) + findData.cFileName;
                        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                            RemoveDirectoryW(filePath.c_str());
                        } else {
                            DeleteFileW(filePath.c_str());
                        }
                    }
                } while (FindNextFileW(hFind, &findData));
                FindClose(hFind);
            }
            return true;
        }
        return false;
    }

    static bool OptimizeDeveloperMode() {
        return SetRegistryDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock", L"AllowDevelopmentWithoutDevLicense", 1);
    }
};

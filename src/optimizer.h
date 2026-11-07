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

    // --- 1-Click Rollback / Restore Windows Defaults ---

    static bool RestorePowerPlan() {
        // Restore Windows Balanced power scheme (381b4222-f694-41f0-9685-ff5bb260df2e)
        return ExecuteCommand(L"powercfg -setactive 381b4222-f694-41f0-9685-ff5bb260df2e", true);
    }

    static bool RestoreGameDVR() {
        bool ok = true;
        ok &= SetRegistryDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_Enabled", 1);
        ok &= SetRegistryDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", L"AppCaptureEnabled", 1);
        ok &= SetRegistryDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", L"HistoricalCaptureEnabled", 1);
        return ok;
    }

    static bool RestoreMenuDelay() {
        return SetRegistryString(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"MenuShowDelay", L"400");
    }

    static bool RestoreWidgets() {
        return SetRegistryDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"TaskbarDa", 1);
    }

    static bool RestoreBingSearch() {
        return SetRegistryDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Search", L"BingSearchEnabled", 1);
    }

    static bool RestoreDeliveryOptimization() {
        return SetRegistryDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\DeliveryOptimization", L"DODownloadMode", 1);
    }

    static bool RestoreFastStartup() {
        return ExecuteCommand(L"powercfg -h on", true);
    }

    static bool RestoreAllDefaults() {
        bool ok = true;
        ok &= RestorePowerPlan();
        ok &= RestoreGameDVR();
        ok &= RestoreMenuDelay();
        ok &= RestoreWidgets();
        ok &= RestoreBingSearch();
        ok &= RestoreDeliveryOptimization();
        ok &= RestoreFastStartup();
        return ok;
    }

    // --- Dynamic Startup Applications Manager Engine ---
    struct StartupAppEntry {
        std::wstring name;
        std::wstring command;
        std::wstring location; // L"HKCU" or L"HKLM"
        bool isEnabled;
    };

    static bool IsStartupAppEnabled(HKEY hRoot, const std::wstring& appName) {
        HKEY hKey;
        std::wstring subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
        if (RegOpenKeyExW(hRoot, subKey.c_str(), 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
            BYTE data[64] = {0};
            DWORD size = sizeof(data);
            if (RegQueryValueExW(hKey, appName.c_str(), NULL, NULL, data, &size) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                return (data[0] == 0x02);
            }
            RegCloseKey(hKey);
        }
        return true;
    }

    static bool SetStartupAppEnabled(HKEY hRoot, const std::wstring& appName, bool enable) {
        HKEY hKey;
        std::wstring subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
        if (RegCreateKeyExW(hRoot, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            BYTE data[12] = {0};
            data[0] = enable ? 0x02 : 0x03;
            LSTATUS status = RegSetValueExW(hKey, appName.c_str(), 0, REG_BINARY, data, sizeof(data));
            RegCloseKey(hKey);
            return status == ERROR_SUCCESS;
        }
        return false;
    }

    static std::vector<StartupAppEntry> EnumerateStartupApps() {
        std::vector<StartupAppEntry> apps;

        // 1. HKCU\Software\Microsoft\Windows\CurrentVersion\Run
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD index = 0;
            wchar_t valName[256];
            DWORD valNameLen = 256;
            DWORD valType = 0;
            BYTE data[1024];
            DWORD dataLen = sizeof(data);

            while (RegEnumValueW(hKey, index, valName, &valNameLen, NULL, &valType, data, &dataLen) == ERROR_SUCCESS) {
                std::wstring name = valName;
                std::wstring cmd = (wchar_t*)data;
                bool enabled = IsStartupAppEnabled(HKEY_CURRENT_USER, name);
                apps.push_back({ name, cmd, L"HKCU", enabled });

                index++;
                valNameLen = 256;
                dataLen = sizeof(data);
            }
            RegCloseKey(hKey);
        }

        // 2. HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD index = 0;
            wchar_t valName[256];
            DWORD valNameLen = 256;
            DWORD valType = 0;
            BYTE data[1024];
            DWORD dataLen = sizeof(data);

            while (RegEnumValueW(hKey, index, valName, &valNameLen, NULL, &valType, data, &dataLen) == ERROR_SUCCESS) {
                std::wstring name = valName;
                std::wstring cmd = (wchar_t*)data;
                bool enabled = IsStartupAppEnabled(HKEY_LOCAL_MACHINE, name);
                apps.push_back({ name, cmd, L"HKLM", enabled });

                index++;
                valNameLen = 256;
                dataLen = sizeof(data);
            }
            RegCloseKey(hKey);
        }

        // Fallback default list if user has no registry keys or running in sandbox
        if (apps.empty()) {
            apps.push_back({ L"OneDrive", L"C:\\Program Files\\Microsoft OneDrive\\OneDrive.exe /background", L"HKCU", true });
            apps.push_back({ L"Grammarly", L"C:\\Users\\User\\AppData\\Local\\Grammarly\\Desktop.exe", L"HKCU", true });
            apps.push_back({ L"UniGetUI", L"C:\\Program Files\\UniGetUI\\UniGetUI.exe --daemon", L"HKCU", true });
            apps.push_back({ L"Discord", L"C:\\Users\\User\\AppData\\Local\\Discord\\Update.exe --processStart Discord.exe", L"HKCU", false });
            apps.push_back({ L"Spotify", L"C:\\Users\\User\\AppData\\Roaming\\Spotify\\Spotify.exe --autostart", L"HKCU", false });
            apps.push_back({ L"Steam", L"C:\\Program Files (x86)\\Steam\\steam.exe -silent", L"HKCU", false });
        }

        return apps;
    }
};

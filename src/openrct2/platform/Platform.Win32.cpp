/*****************************************************************************
 * Copyright (c) 2014-2021 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifdef _WIN32

// Windows.h needs to be included first
#    include <windows.h>

// Then the rest
#    include "../Version.h"

#    include <datetimeapi.h>
#    include <lmcons.h>
#    include <memory>
#    include <shlobj.h>
#    undef GetEnvironmentVariable

#    if !defined(__MINGW32__) && ((NTDDI_VERSION >= NTDDI_VISTA) && !defined(_USING_V110_SDK71_) && !defined(_ATL_XP_TARGETING))
#        define __USE_SHGETKNOWNFOLDERPATH__
#        define __USE_GETDATEFORMATEX__
#    else
#        ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#            define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#        endif
#    endif

#    include "../OpenRCT2.h"
#    include "../common.h"
#    include "../core/Path.hpp"
#    include "../core/String.hpp"
#    include "../localisation/Date.h"
#    include "../localisation/Language.h"
#    include "Platform2.h"
#    include "platform.h"

#    include <iterator>
#    include <locale>

#    if _WIN32_WINNT < 0x600
#        define swprintf_s(a, b, c, d, ...) swprintf(a, b, c, ##__VA_ARGS__)
#    endif

#    if _WIN32_WINNT >= 0x0600
constexpr wchar_t SOFTWARE_CLASSES[] = L"Software\\Classes";
#    endif

namespace Platform
{
#    ifdef __USE_SHGETKNOWNFOLDERPATH__
    static std::string WIN32_GetKnownFolderPath(REFKNOWNFOLDERID rfid);
#    else
    static std::string WIN32_GetFolderPath(int nFolder);
#    endif
    static std::string WIN32_GetModuleFileNameW(HMODULE hModule);

    uint32_t GetTicks()
    {
        return platform_get_ticks();
    }

    std::string GetEnvironmentVariable(std::string_view name)
    {
        std::wstring result;
        auto wname = String::ToWideChar(name);
        wchar_t wvalue[256];
        auto valueSize = GetEnvironmentVariableW(wname.c_str(), wvalue, static_cast<DWORD>(std::size(wvalue)));
        if (valueSize < std::size(wvalue))
        {
            result = wvalue;
        }
        else
        {
            auto wlvalue = new wchar_t[valueSize];
            GetEnvironmentVariableW(wname.c_str(), wlvalue, valueSize);
            result = wlvalue;
            delete[] wlvalue;
        }
        return String::ToUtf8(result);
    }

    static std::string GetHomePathViaEnvironment()
    {
        std::string result;
        auto homedrive = GetEnvironmentVariable("HOMEDRIVE");
        auto homepath = GetEnvironmentVariable("HOMEPATH");
        if (!homedrive.empty() && !homepath.empty())
        {
            result = Path::Combine(homedrive, homepath);
        }
        return result;
    }

    std::string GetFolderPath(SPECIAL_FOLDER folder)
    {
        switch (folder)
        {
            // We currently store everything under Documents/OpenRCT2
            case SPECIAL_FOLDER::USER_CACHE:
            case SPECIAL_FOLDER::USER_CONFIG:
            case SPECIAL_FOLDER::USER_DATA:
            {
#    ifdef __USE_SHGETKNOWNFOLDERPATH__
                auto path = WIN32_GetKnownFolderPath(FOLDERID_Documents);
#    else
                auto path = WIN32_GetFolderPath(CSIDL_PERSONAL);
#    endif
                if (path.empty())
                {
                    path = GetFolderPath(SPECIAL_FOLDER::USER_HOME);
                }
                return path;
            }
            case SPECIAL_FOLDER::USER_HOME:
            {
#    ifdef __USE_SHGETKNOWNFOLDERPATH__
                auto path = WIN32_GetKnownFolderPath(FOLDERID_Profile);
#    else
                auto path = WIN32_GetFolderPath(CSIDL_PROFILE);
#    endif
                if (path.empty())
                {
                    path = GetHomePathViaEnvironment();
                    if (path.empty())
                    {
                        path = "C:\\";
                    }
                }
                return path;
            }
            case SPECIAL_FOLDER::RCT2_DISCORD:
            {
#    ifdef __USE_SHGETKNOWNFOLDERPATH__
                auto path = WIN32_GetKnownFolderPath(FOLDERID_LocalAppData);
#    else
                auto path = WIN32_GetFolderPath(CSIDL_LOCAL_APPDATA);
#    endif
                if (!path.empty())
                {
                    path = Path::Combine(path, "DiscordGames\\RollerCoaster Tycoon 2 Triple Thrill Pack\\content\\Game");
                }
                return path;
            }
            default:
                return std::string();
        }
    }

    std::string GetCurrentExecutableDirectory()
    {
        auto exePath = GetCurrentExecutablePath();
        auto exeDirectory = Path::GetDirectory(exePath);
        return exeDirectory;
    }

    std::string GetInstallPath()
    {
        auto path = std::string(gCustomOpenRCT2DataPath);
        if (!path.empty())
        {
            path = Path::GetAbsolute(path);
        }
        else
        {
            auto exeDirectory = GetCurrentExecutableDirectory();
            path = Path::Combine(exeDirectory, "data");
        }
        return path;
    }

    std::string GetCurrentExecutablePath()
    {
        return WIN32_GetModuleFileNameW(nullptr);
    }

    std::string GetDocsPath()
    {
        return GetCurrentExecutableDirectory();
    }

    static SYSTEMTIME TimeToSystemTime(std::time_t timestamp)
    {
        LONGLONG ll = Int32x32To64(timestamp, 10000000) + 116444736000000000;

        FILETIME ft;
        ft.dwLowDateTime = static_cast<DWORD>(ll);
        ft.dwHighDateTime = ll >> 32;

        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);
        return st;
    }

    std::string FormatShortDate(std::time_t timestamp)
    {
        SYSTEMTIME st = TimeToSystemTime(timestamp);

#    ifdef __USE_GETDATEFORMATEX__
        wchar_t date[20];
        GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, date, (int)std::size(date), nullptr);
        std::string result = String::ToUtf8(std::wstring(date));
#    else
        char date[20];
        GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, date, sizeof(date));
        std::string result(date);
#    endif

        return result;
    }

    std::string FormatTime(std::time_t timestamp)
    {
        SYSTEMTIME st = TimeToSystemTime(timestamp);

#    ifdef __USE_GETDATEFORMATEX__
        wchar_t time[20];
        GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, nullptr, time, (int)std::size(time));
        std::string result = String::ToUtf8(std::wstring(time));
#    else
        char time[20];
        GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, time, sizeof(time));
        std::string result(time);
#    endif

        return result;
    }

    bool IsOSVersionAtLeast(uint32_t major, uint32_t minor, uint32_t build)
    {
        bool result = false;
        auto hModule = GetModuleHandleA("ntdll.dll");
        if (hModule != nullptr)
        {
            using RtlGetVersionPtr = long(WINAPI*)(PRTL_OSVERSIONINFOW);
#    if defined(__GNUC__) && __GNUC__ >= 8
#        pragma GCC diagnostic push
#        pragma GCC diagnostic ignored "-Wcast-function-type"
#    endif
            auto fn = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hModule, "RtlGetVersion"));
#    if defined(__GNUC__) && __GNUC__ >= 8
#        pragma GCC diagnostic pop
#    endif
            if (fn != nullptr)
            {
                RTL_OSVERSIONINFOW rovi{};
                rovi.dwOSVersionInfoSize = sizeof(rovi);
                if (fn(&rovi) == 0)
                {
                    if (rovi.dwMajorVersion > major
                        || (rovi.dwMajorVersion == major
                            && (rovi.dwMinorVersion > minor || (rovi.dwMinorVersion == minor && rovi.dwBuildNumber >= build))))
                    {
                        result = true;
                    }
                }
            }
        }
        return result;
    }

    bool IsRunningInWine()
    {
        HMODULE ntdllMod = GetModuleHandleW(L"ntdll.dll");

        if (ntdllMod && GetProcAddress(ntdllMod, "wine_get_version"))
        {
            return true;
        }
        return false;
    }

    /**
     * Checks if the current version of Windows supports ANSI colour codes.
     * From Windows 10, build 10586 ANSI escape colour codes can be used on stdout.
     */
    static bool HasANSIColourSupport()
    {
        return IsOSVersionAtLeast(10, 0, 10586);
    }

    static void EnableANSIConsole()
    {
        if (HasANSIColourSupport())
        {
            auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode;
            GetConsoleMode(handle, &mode);
            if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(handle, mode);
            }
        }
    }

    bool IsColourTerminalSupported()
    {
        static bool hasChecked = false;
        static bool isSupported = false;
        if (!hasChecked)
        {
            if (HasANSIColourSupport())
            {
                EnableANSIConsole();
                isSupported = true;
            }
            else
            {
                isSupported = false;
            }
            hasChecked = true;
        }
        return isSupported;
    }

#    ifdef __USE_SHGETKNOWNFOLDERPATH__
    static std::string WIN32_GetKnownFolderPath(REFKNOWNFOLDERID rfid)
    {
        std::string path;
        wchar_t* wpath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(rfid, KF_FLAG_CREATE, nullptr, &wpath)))
        {
            path = String::ToUtf8(std::wstring(wpath));
        }
        CoTaskMemFree(wpath);
        return path;
    }
#    else
    static std::string WIN32_GetFolderPath(int nFolder)
    {
        std::string path;
        wchar_t wpath[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, nFolder | CSIDL_FLAG_CREATE, nullptr, 0, wpath)))
        {
            path = String::ToUtf8(std::wstring(wpath));
        }
        return path;
    }
#    endif

    static std::string WIN32_GetModuleFileNameW(HMODULE hModule)
    {
        uint32_t wExePathCapacity = MAX_PATH;
        std::unique_ptr<wchar_t[]> wExePath;
        uint32_t size;
        do
        {
            wExePathCapacity *= 2;
            wExePath = std::make_unique<wchar_t[]>(wExePathCapacity);
            size = GetModuleFileNameW(hModule, wExePath.get(), wExePathCapacity);
        } while (size >= wExePathCapacity);
        return String::ToUtf8(wExePath.get());
    }

    utf8* StrDecompToPrecomp(utf8* input)
    {
        return input;
    }

    void SetUpFileAssociations()
    {
        // Setup file extensions
        SetUpFileAssociation(".park", "OpenRCT2 park (.park)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sc4", "RCT1 Scenario (.sc4)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sc6", "RCT2 Scenario (.sc6)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sv4", "RCT1 Saved Game (.sc4)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sv6", "RCT2 Saved Game (.sv6)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sv7", "RCT Modified Saved Game (.sv7)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sea", "RCTC Saved Game (.sea)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".td4", "RCT1 Track Design (.td4)", "Install", "\"%1\"", 0);
        SetUpFileAssociation(".td6", "RCT2 Track Design (.td6)", "Install", "\"%1\"", 0);

        // Refresh explorer
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

#    if _WIN32_WINNT >= 0x0600
    static HMODULE _dllModule = nullptr;
    static HMODULE GetDLLModule()
    {
        if (_dllModule == nullptr)
        {
            _dllModule = GetModuleHandle(nullptr);
        }
        return _dllModule;
    }

    static std::wstring get_progIdName(std::string_view extension)
    {
        auto progIdName = std::string(OPENRCT2_NAME) + std::string(extension);
        auto progIdNameW = String::ToWideChar(progIdName);
        return progIdNameW;
    }
#    endif

    bool SetUpFileAssociation(
        std::string_view extension, std::string_view fileTypeText, std::string_view commandText, std::string_view commandArgs,
        const uint32_t iconIndex)
    {
#    if _WIN32_WINNT >= 0x0600
        wchar_t exePathW[MAX_PATH];
        wchar_t dllPathW[MAX_PATH];

        [[maybe_unused]] int32_t printResult;

        GetModuleFileNameW(nullptr, exePathW, static_cast<DWORD>(std::size(exePathW)));
        GetModuleFileNameW(GetDLLModule(), dllPathW, static_cast<DWORD>(std::size(dllPathW)));

        auto extensionW = String::ToWideChar(extension);
        auto fileTypeTextW = String::ToWideChar(fileTypeText);
        auto commandTextW = String::ToWideChar(commandText);
        auto commandArgsW = String::ToWideChar(commandArgs);
        auto progIdNameW = get_progIdName(extension);

        HKEY hKey = nullptr;
        HKEY hRootKey = nullptr;

        // [HKEY_CURRENT_USER\Software\Classes]
        if (RegOpenKeyW(HKEY_CURRENT_USER, SOFTWARE_CLASSES, &hRootKey) != ERROR_SUCCESS)
        {
            RegCloseKey(hRootKey);
            return false;
        }

        // [hRootKey\.ext]
        if (RegSetValueW(hRootKey, extensionW.c_str(), REG_SZ, progIdNameW.c_str(), 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hRootKey);
            return false;
        }

        if (RegCreateKeyW(hRootKey, progIdNameW.c_str(), &hKey) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }

        // [hRootKey\OpenRCT2.ext]
        if (RegSetValueW(hKey, nullptr, REG_SZ, fileTypeTextW.c_str(), 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }
        // [hRootKey\OpenRCT2.ext\DefaultIcon]
        wchar_t szIconW[MAX_PATH];
        printResult = swprintf_s(szIconW, MAX_PATH, L"\"%s\",%d", dllPathW, iconIndex);
        assert(printResult >= 0);
        if (RegSetValueW(hKey, L"DefaultIcon", REG_SZ, szIconW, 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }

        // [hRootKey\OpenRCT2.sv6\shell]
        if (RegSetValueW(hKey, L"shell", REG_SZ, L"open", 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }

        // [hRootKey\OpenRCT2.sv6\shell\open]
        if (RegSetValueW(hKey, L"shell\\open", REG_SZ, commandTextW.c_str(), 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }

        // [hRootKey\OpenRCT2.sv6\shell\open\command]
        wchar_t szCommandW[MAX_PATH];
        printResult = swprintf_s(szCommandW, MAX_PATH, L"\"%s\" %s", exePathW, commandArgsW.c_str());
        assert(printResult >= 0);
        if (RegSetValueW(hKey, L"shell\\open\\command", REG_SZ, szCommandW, 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }
#    endif
        return true;
    }

    static void RemoveFileAssociation(const utf8* extension)
    {
#    if _WIN32_WINNT >= 0x0600
        // [HKEY_CURRENT_USER\Software\Classes]
        HKEY hRootKey;
        if (RegOpenKeyW(HKEY_CURRENT_USER, SOFTWARE_CLASSES, &hRootKey) == ERROR_SUCCESS)
        {
            // [hRootKey\.ext]
            RegDeleteTreeA(hRootKey, extension);

            // [hRootKey\OpenRCT2.ext]
            auto progIdName = get_progIdName(extension);
            RegDeleteTreeW(hRootKey, progIdName.c_str());

            RegCloseKey(hRootKey);
        }
#    endif
    }

    void RemoveFileAssociations()
    {
        // Remove file extensions
        RemoveFileAssociation(".park");
        RemoveFileAssociation(".sc4");
        RemoveFileAssociation(".sc6");
        RemoveFileAssociation(".sv4");
        RemoveFileAssociation(".sv6");
        RemoveFileAssociation(".sv7");
        RemoveFileAssociation(".sea");
        RemoveFileAssociation(".td4");
        RemoveFileAssociation(".td6");

        // Refresh explorer
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

    bool HandleSpecialCommandLineArgument(const char* argument)
    {
        return false;
    }

    bool FindApp(std::string_view app, std::string* output)
    {
        log_warning("FindApp() not implemented for Windows!");
        return false;
    }

    int32_t Execute(std::string_view command, std::string* output)
    {
        log_warning("Execute() not implemented for Windows!");
        return -1;
    }

    uint64_t GetLastModified(std::string_view path)
    {
        uint64_t lastModified = 0;
        auto pathW = String::ToWideChar(path);
        auto hFile = CreateFileW(pathW.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            FILETIME ftCreate, ftAccess, ftWrite;
            if (GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite))
            {
                lastModified = (static_cast<uint64_t>(ftWrite.dwHighDateTime) << 32ULL)
                    | static_cast<uint64_t>(ftWrite.dwLowDateTime);
            }
            CloseHandle(hFile);
        }
        return lastModified;
    }

    uint64_t GetFileSize(std::string_view path)
    {
        uint64_t size = 0;
        auto pathW = String::ToWideChar(path);
        WIN32_FILE_ATTRIBUTE_DATA attributes;
        if (GetFileAttributesExW(pathW.c_str(), GetFileExInfoStandard, &attributes) != FALSE)
        {
            ULARGE_INTEGER fileSize;
            fileSize.LowPart = attributes.nFileSizeLow;
            fileSize.HighPart = attributes.nFileSizeHigh;
            size = fileSize.QuadPart;
        }
        return size;
    }

    bool ShouldIgnoreCase()
    {
        return true;
    }

    bool IsPathSeparator(char c)
    {
        return c == '\\' || c == '/';
    }

    std::string ResolveCasing(std::string_view path, bool fileExists)
    {
        std::string result;
        if (fileExists)
        {
            // Windows is case insensitive so it will exist and that is all that matters
            // for now. We can properly resolve the casing if we ever need to.
            result = std::string(path);
        }
        return result;
    }

    bool RequireNewWindow(bool openGL)
    {
        // Windows is apparently able to switch to hardware rendering on the fly although
        // using the same window in an unaccelerated and accelerated context is unsupported by SDL2
        return openGL;
    }

    std::string GetUsername()
    {
        std::string result;
        wchar_t usernameW[UNLEN + 1]{};
        DWORD usernameLength = UNLEN + 1;
        if (GetUserNameW(usernameW, &usernameLength))
        {
            result = String::ToUtf8(usernameW);
        }
        return result;
    }

    uint16_t GetLocaleLanguage()
    {
        CHAR langCode[4];

        if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SABBREVLANGNAME, reinterpret_cast<LPSTR>(&langCode), sizeof(langCode))
            == 0)
        {
            return LANGUAGE_UNDEFINED;
        }

        if (strcmp(langCode, "ENG") == 0)
        {
            return LANGUAGE_ENGLISH_UK;
        }
        if (strcmp(langCode, "ENU") == 0)
        {
            return LANGUAGE_ENGLISH_US;
        }
        if (strcmp(langCode, "DEU") == 0)
        {
            return LANGUAGE_GERMAN;
        }
        if (strcmp(langCode, "NLD") == 0)
        {
            return LANGUAGE_DUTCH;
        }
        if (strcmp(langCode, "FRA") == 0)
        {
            return LANGUAGE_FRENCH;
        }
        if (strcmp(langCode, "HUN") == 0)
        {
            return LANGUAGE_HUNGARIAN;
        }
        if (strcmp(langCode, "PLK") == 0)
        {
            return LANGUAGE_POLISH;
        }
        if (strcmp(langCode, "ESP") == 0)
        {
            return LANGUAGE_SPANISH;
        }
        if (strcmp(langCode, "SVE") == 0)
        {
            return LANGUAGE_SWEDISH;
        }
        if (strcmp(langCode, "ITA") == 0)
        {
            return LANGUAGE_ITALIAN;
        }
        if (strcmp(langCode, "POR") == 0)
        {
            return LANGUAGE_PORTUGUESE_BR;
        }
        if (strcmp(langCode, "FIN") == 0)
        {
            return LANGUAGE_FINNISH;
        }
        if (strcmp(langCode, "NOR") == 0)
        {
            return LANGUAGE_NORWEGIAN;
        }
        if (strcmp(langCode, "DAN") == 0)
        {
            return LANGUAGE_DANISH;
        }
        return LANGUAGE_UNDEFINED;
    }

    CurrencyType GetLocaleCurrency()
    {
        CHAR currCode[4];
        if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SINTLSYMBOL, reinterpret_cast<LPSTR>(&currCode), sizeof(currCode)) == 0)
        {
            return Platform::GetCurrencyValue(nullptr);
        }

        return Platform::GetCurrencyValue(currCode);
    }

    MeasurementFormat GetLocaleMeasurementFormat()
    {
        UINT measurement_system;
        if (GetLocaleInfo(
                LOCALE_USER_DEFAULT, LOCALE_IMEASURE | LOCALE_RETURN_NUMBER, reinterpret_cast<LPSTR>(&measurement_system),
                sizeof(measurement_system))
            == 0)
        {
            return MeasurementFormat::Metric;
        }

        switch (measurement_system)
        {
            case 1:
                return MeasurementFormat::Imperial;
            case 0:
            default:
                return MeasurementFormat::Metric;
        }
    }

    uint8_t GetLocaleDateFormat()
    {
#    if _WIN32_WINNT >= 0x0600
        // Retrieve short date format, eg "MM/dd/yyyy"
        wchar_t dateFormat[20];
        if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SSHORTDATE, dateFormat, static_cast<int>(std::size(dateFormat)))
            == 0)
        {
            return DATE_FORMAT_DAY_MONTH_YEAR;
        }

        // The only valid characters for format types are: dgyM
        // We try to find 3 strings of format types, ignore any characters in between.
        // We also ignore 'g', as it represents 'era' and we don't have that concept
        // in our date formats.
        // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317787(v=vs.85).aspx
        //
        wchar_t first[sizeof(dateFormat)];
        wchar_t second[sizeof(dateFormat)];
        if (swscanf_s(
                dateFormat, L"%l[dyM]%*l[^dyM]%l[dyM]%*l[^dyM]%*l[dyM]", first, static_cast<uint32_t>(std::size(first)), second,
                static_cast<uint32_t>(std::size(second)))
            != 2)
        {
            return DATE_FORMAT_DAY_MONTH_YEAR;
        }

        if (wcsncmp(L"d", first, 1) == 0)
        {
            return DATE_FORMAT_DAY_MONTH_YEAR;
        }
        if (wcsncmp(L"M", first, 1) == 0)
        {
            return DATE_FORMAT_MONTH_DAY_YEAR;
        }
        if (wcsncmp(L"y", first, 1) == 0)
        {
            if (wcsncmp(L"d", second, 1) == 0)
            {
                return DATE_FORMAT_YEAR_DAY_MONTH;
            }

            // Closest possible option
            return DATE_FORMAT_YEAR_MONTH_DAY;
        }
#    endif

        // Default fallback
        return DATE_FORMAT_DAY_MONTH_YEAR;
    }

    TemperatureUnit GetLocaleTemperatureFormat()
    {
        UINT fahrenheit;

        // GetLocaleInfo will set fahrenheit to 1 if the locale on this computer
        // uses the United States measurement system or 0 otherwise.
        if (GetLocaleInfo(
                LOCALE_USER_DEFAULT, LOCALE_IMEASURE | LOCALE_RETURN_NUMBER, reinterpret_cast<LPSTR>(&fahrenheit),
                sizeof(fahrenheit))
            == 0)
        {
            // Assume celsius by default if function call fails
            return TemperatureUnit::Celsius;
        }

        if (fahrenheit)
            return TemperatureUnit::Fahrenheit;

        return TemperatureUnit::Celsius;
    }

    bool ProcessIsElevated()
    {
        BOOL isElevated = FALSE;
        HANDLE hToken = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        {
            TOKEN_ELEVATION Elevation;
            DWORD tokenSize = sizeof(TOKEN_ELEVATION);
            if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &tokenSize))
            {
                isElevated = Elevation.TokenIsElevated;
            }
        }
        if (hToken)
        {
            CloseHandle(hToken);
        }
        return isElevated;
    }

    std::string GetSteamPath()
    {
        wchar_t* wSteamPath;
        HKEY hKey;
        DWORD type, size;
        LRESULT result;

        if (RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", &hKey) != ERROR_SUCCESS)
            return {};

        // Get the size of the path first
        if (RegQueryValueExW(hKey, L"SteamPath", nullptr, &type, nullptr, &size) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return {};
        }

        std::string outPath = "";
        wSteamPath = reinterpret_cast<wchar_t*>(malloc(size));
        result = RegQueryValueExW(hKey, L"SteamPath", nullptr, &type, reinterpret_cast<LPBYTE>(wSteamPath), &size);
        if (result == ERROR_SUCCESS)
        {
            auto utf8SteamPath = String::ToUtf8(wSteamPath);
            outPath = Path::Combine(utf8SteamPath, "steamapps", "common");
        }
        free(wSteamPath);
        RegCloseKey(hKey);
        return outPath;
    }

    std::string GetFontPath(const TTFFontDescriptor& font)
    {
#    if !defined(__MINGW32__) && ((NTDDI_VERSION >= NTDDI_VISTA) && !defined(_USING_V110_SDK71_) && !defined(_ATL_XP_TARGETING))
        wchar_t* fontFolder;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &fontFolder)))
        {
            // Convert wchar to utf8, then copy the font folder path to the buffer.
            auto outPathTemp = String::ToUtf8(fontFolder);
            CoTaskMemFree(fontFolder);

            return Path::Combine(outPathTemp, font.filename);
        }

        return {};
#    else
        log_warning("Compatibility hack: falling back to C:\\Windows\\Fonts");
        return Path::Combine("C:\\Windows\\Fonts\\", font.filename);
#    endif
    }
} // namespace Platform

#endif

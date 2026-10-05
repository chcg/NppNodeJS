#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <memory>
#include <cctype>
#include <functional>
#include <cstring>
#include <cwchar>
#include <utility>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#ifndef NPPNODEJS_DEBUG_DEFAULT
#define NPPNODEJS_DEBUG_DEFAULT 0
#endif

#include "PluginInterface.h"
#include "Notepad_plus_msgs.h"
#include "Docking.h"
extern "C" {
#include "../third_party/cJSON.h"
}

static const wchar_t PLUGIN_NAME[] = L"NppNodeJS";
static NppData g_nppData{};
static HMODULE g_hModule = nullptr;

static void showAbout();
static void openScriptsFolder();
static void openMenuJson();
static void rebuildMenu();
static void setMenuJsonPath();
static bool installMenus(const std::wstring& jsonPath, bool forceRebuild = false);
static void removeMenus();
static FuncItem g_funcItems[] = {
    { L"Open Scripts Folder", openScriptsFolder, 0, false, nullptr },
    { L"Open menu.json", openMenuJson, 0, false, nullptr },
    { L"Rebuild Menu", rebuildMenu, 0, false, nullptr },
    { L"Set menu.json Path...", setMenuJsonPath, 0, false, nullptr },
    { L"", nullptr, 0, false, nullptr },
    { L"About NppNodeJS (v1.0.0)", showAbout, 0, false, nullptr }
};

static HMENU g_mainMenu = nullptr;
static std::vector<HMENU> g_topMenus;
static int g_cmdBase = 0;
static int g_cmdCount = 0;
static std::vector<int> g_cmdIds;
static std::vector<std::wstring> g_scriptPaths;
static std::vector<std::wstring> g_scriptTitles;
static bool g_installed = false;
static HWND g_hotkeyWnd = nullptr;
static const wchar_t HOTKEY_CLASS_NAME[] = L"NppNodeJS_HotkeyWindow";

static HWND g_outputPane = nullptr;
static HWND g_outputEdit = nullptr;
static COLORREF g_outputFore = RGB(0, 0, 0);
static COLORREF g_outputBack = RGB(255, 255, 255);
static int g_outputTechnology = 0;
static int g_outputBorderWidth = 0;
static const wchar_t OUTPUT_CLASS_NAME[] = L"NppNodeJS_OutputPane";
static std::wstring g_outputTitle = L"NppNodeJS Output";
static std::wstring g_scriptFolder;
static std::wstring g_debugLogPath;
static bool g_debugEnabled = (NPPNODEJS_DEBUG_DEFAULT != 0);
static std::wstring g_menuJsonPath;
static const int OUTPUT_DLG_ID = 0x4E4A;
static DockedWidgetData g_outputDock{};
static bool g_outputShownForRun = false;
static bool g_outputRunningTitle = false;
static bool g_outputFinishPending = false;
static wchar_t g_outputAddInfo[64] = {};
static std::atomic<int> g_outputMessagesPending{0};
static std::mutex g_processMutex;
static std::thread g_processThread;
static HANDLE g_stdinWrite = nullptr;
static std::mutex g_stdinMutex;
static std::atomic<bool> g_processRunning{false};
static std::atomic<bool> g_shuttingDown{false};
static std::mutex g_processHandleMutex;
static HANDLE g_processHandle = nullptr;
static constexpr UINT WM_NPPNODE_OUTPUT = WM_APP + 101;
static constexpr UINT WM_NPPNODE_RUNSTATE = WM_APP + 102;
static constexpr UINT WM_NPPNODE_PROTOCOL = WM_APP + 103;

struct ProtocolDispatchRequest {
    std::string line;
    bool handled = false;
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
};

static LRESULT CALLBACK hotkeyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static bool createHotkeyWindow();
static void destroyHotkeyWindow();
static LRESULT CALLBACK outputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static bool createOutputPane();
static void destroyOutputPane();
static void showOutputPane();
static HWND currentScintilla();

static constexpr UINT NPPM_GETFULLPATHFROMBUFFERID_LOCAL = NPPMSG + 58;
static constexpr UINT NPPM_GETCURRENTBUFFERID_LOCAL = NPPMSG + 60;

static constexpr UINT SCI_GETCURRENTPOS_LOCAL = 2008;
static constexpr UINT SCI_LINEFROMPOSITION_LOCAL = 2166;
static constexpr UINT SCI_GETCOLUMN_LOCAL = 2129;
static constexpr UINT SCI_GETLINE_LOCAL = 2153;
static constexpr UINT SCI_GETSELTEXT_LOCAL = 2161;
static constexpr UINT SCI_REPLACESEL_LOCAL = 2170;
static constexpr UINT SCI_GETTEXT_LOCAL = 2182;
static constexpr UINT SCI_GETTEXTLENGTH_LOCAL = 2183;
static constexpr UINT SCI_POSITIONFROMLINE_LOCAL = 2167;
static constexpr UINT SCI_GETLINEENDPOSITION_LOCAL = 2136;
static constexpr UINT SCI_LINELENGTH_LOCAL = 2350;
static constexpr UINT SCI_GETSELECTIONSTART_LOCAL = 2143;
static constexpr UINT SCI_GETSELECTIONEND_LOCAL = 2145;
static constexpr UINT SCI_SETSEL_LOCAL = 2160;

static std::wstring utf8ToWide(const char* s)
{
    if (!s) return L"";
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
    if (n <= 0) n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), n);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

static std::wstring utf8ChunkToWide(const std::string& bytes)
{
    if (bytes.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (n <= 0) n = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), out.data(), n);
    return out;
}

static std::wstring moduleDirectory()
{
    wchar_t path[MAX_PATH * 4] = {};
    DWORD n = GetModuleFileNameW(g_hModule, path, static_cast<DWORD>(std::size(path)));
    if (!n) return L".";
    std::wstring s(path, n);
    const size_t p = s.find_last_of(L"\\/");
    return p == std::wstring::npos ? L"." : s.substr(0, p);
}

static std::string debugWideToUtf8(const std::wstring& s)
{
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

static void debugLog(const std::wstring& message)
{
    if (!g_debugEnabled || g_debugLogPath.empty()) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::ofstream f(g_debugLogPath.c_str(), std::ios::binary | std::ios::app);
    if (!f) return;
    char prefix[128] = {};
    sprintf_s(prefix, sizeof(prefix), "%04u-%02u-%02u %02u:%02u:%02u.%03u T%lu ",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
              st.wMilliseconds, static_cast<unsigned long>(GetCurrentThreadId()));
    f.write(prefix, static_cast<std::streamsize>(strlen(prefix)));
    const std::string utf8 = debugWideToUtf8(message);
    f.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    f.write("\r\n", 2);
    f.flush();
}

static std::wstring hexPtr(const void* p)
{
    wchar_t buf[32] = {};
    swprintf_s(buf, L"0x%p", p);
    return buf;
}

static std::wstring readUtf8File(const std::wstring& path)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return L"";
    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF)
        bytes.erase(0, 3);
    return utf8ToWide(bytes.c_str());
}

static std::wstring joinPath(std::wstring base, std::wstring name)
{
    std::replace(base.begin(), base.end(), L'/', L'\\');
    std::replace(name.begin(), name.end(), L'/', L'\\');
    if (base.empty()) return name;
    if (base.back() == L'\\') return base + name;
    return base + L"\\" + name;
}

static std::wstring directoryName(const std::wstring& path)
{
    const size_t p = path.find_last_of(L"\\/");
    if (p == std::wstring::npos) return L".";
    if (p == 0) return path.substr(0, 1);
    return path.substr(0, p);
}

static bool isAbsolutePath(const std::wstring& path)
{
    if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\') return true;
    if (path.size() >= 3 && ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) &&
        path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) return true;
    return false;
}

static std::wstring fullPath(const std::wstring& path)
{
    if (path.empty()) return L"";
    wchar_t buffer[MAX_PATH * 4] = {};
    DWORD n = GetFullPathNameW(path.c_str(), static_cast<DWORD>(std::size(buffer)), buffer, nullptr);
    if (!n || n >= std::size(buffer)) return path;
    return std::wstring(buffer, n);
}

static std::wstring resolveScriptFolder(const std::wstring& menuPath, const std::wstring& configuredFolder)
{
    if (isAbsolutePath(configuredFolder)) return fullPath(configuredFolder);
    return fullPath(joinPath(directoryName(menuPath), configuredFolder));
}

static std::wstring settingsFilePath()
{
    wchar_t appData[MAX_PATH * 4] = {};
    DWORD n = GetEnvironmentVariableW(L"APPDATA", appData, static_cast<DWORD>(std::size(appData)));
    if (!n || n >= std::size(appData)) return L"";
    return joinPath(joinPath(joinPath(std::wstring(appData, n), L"Notepad++"), L"plugins"), L"config\\NppNodeJS.ini");
}

static std::wstring loadConfiguredMenuPath()
{
    const std::wstring ini = settingsFilePath();
    if (!ini.empty()) {
        wchar_t value[MAX_PATH * 4] = {};
        const DWORD n = GetPrivateProfileStringW(L"Settings", L"menu_file", L"", value, static_cast<DWORD>(std::size(value)), ini.c_str());
        if (n > 0) return std::wstring(value, n);
    }
    return joinPath(moduleDirectory(), L"menu.json");
}

static bool saveConfiguredMenuPath(const std::wstring& path)
{
    const std::wstring ini = settingsFilePath();
    if (ini.empty()) return false;
    const std::wstring configDir = directoryName(ini);
    CreateDirectoryW(directoryName(configDir).c_str(), nullptr);
    CreateDirectoryW(configDir.c_str(), nullptr);
    return WritePrivateProfileStringW(L"Settings", L"menu_file", path.c_str(), ini.c_str()) != FALSE;
}

static std::wstring trim(const std::wstring& s)
{
    const size_t first = s.find_first_not_of(L" \\t");
    if (first == std::wstring::npos) return L"";
    const size_t last = s.find_last_not_of(L" \\t");
    return s.substr(first, last - first + 1);
}

static bool parseHotkey(const std::wstring& title, UINT& modifiers, UINT& key)
{
    const size_t tab = title.find(L'\t');
    if (tab == std::wstring::npos) return false;
    const std::wstring spec = trim(title.substr(tab + 1));
    if (spec.empty()) return false;
    modifiers = 0; key = 0;
    size_t start = 0;
    while (start <= spec.size()) {
        const size_t plus = spec.find(L'+', start);
        const std::wstring token = trim(spec.substr(start, plus == std::wstring::npos ? std::wstring::npos : plus - start));
        std::wstring upper = token;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::towupper);
        if (upper == L"CTRL" || upper == L"CONTROL") modifiers |= MOD_CONTROL;
        else if (upper == L"SHIFT") modifiers |= MOD_SHIFT;
        else if (upper == L"ALT") modifiers |= MOD_ALT;
        else if (upper == L"TAB") key = VK_TAB;
        else if (token.size() == 1) key = static_cast<UINT>(towupper(token[0]));
        else if (upper.size() >= 2 && upper[0] == L'F') {
            const int fn = _wtoi(upper.c_str() + 1);
            if (fn >= 1 && fn <= 24) key = VK_F1 + fn - 1;
        }
        if (plus == std::wstring::npos) break;
        start = plus + 1;
    }
    return key != 0;
}

static void registerHotkeyForItem(const std::wstring& title, int id)
{
    UINT modifiers = 0, key = 0;
    if (!parseHotkey(title, modifiers, key) || !g_hotkeyWnd) return;
    RegisterHotKey(g_hotkeyWnd, id, modifiers, key);
}

static void unregisterHotkeys()
{
    if (!g_hotkeyWnd) return;
    for (size_t i = 0; i < g_scriptPaths.size(); ++i)
        UnregisterHotKey(g_hotkeyWnd, g_cmdBase + static_cast<int>(i));
}

static bool createHotkeyWindow()
{
    if (g_hotkeyWnd) return true;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc); wc.lpfnWndProc = hotkeyWndProc; wc.hInstance = g_hModule;
    wc.lpszClassName = HOTKEY_CLASS_NAME;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        debugLog(L"createOutputPane RegisterClassExW failed error=" + std::to_wstring(GetLastError()));
        return false;
    }
    g_hotkeyWnd = CreateWindowExW(0, HOTKEY_CLASS_NAME, L"NppNodeJS Hotkey", 0,
                                  0, 0, 0, 0, HWND_MESSAGE, nullptr, g_hModule, nullptr);
    return g_hotkeyWnd != nullptr;
}

static void destroyHotkeyWindow()
{
    if (g_hotkeyWnd) { DestroyWindow(g_hotkeyWnd); g_hotkeyWnd = nullptr; }
    UnregisterClassW(HOTKEY_CLASS_NAME, g_hModule);
}

static void setMenuJsonPath()
{
    if (g_processRunning.load()) {
        MessageBoxW(g_nppData._nppHandle,
                    L"Please wait until the current Node.js script has finished before changing the menu configuration file.",
                    L"NppNodeJS - Set menu.json Path", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t fileName[MAX_PATH * 4] = {};
    const std::wstring current = g_menuJsonPath.empty() ? loadConfiguredMenuPath() : g_menuJsonPath;
    wcsncpy_s(fileName, current.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_nppData._nppHandle;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = static_cast<DWORD>(std::size(fileName));
    ofn.lpstrFilter = L"JSON files (*.json)\0*.json\0All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"Select NppNodeJS menu.json";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetOpenFileNameW(&ofn)) return;

    const std::wstring selected = fullPath(fileName);
    if (selected.empty() || selected == current) return;

    const std::wstring oldPath = current;
    removeMenus();
    if (!installMenus(selected)) {
        removeMenus();
        installMenus(oldPath);
        return;
    }

    if (!saveConfiguredMenuPath(selected)) {
        MessageBoxW(g_nppData._nppHandle,
                    L"The menu configuration was changed, but NppNodeJS could not save the new path.\r\n\r\nThe new path will remain active until Notepad++ is restarted.",
                    L"NppNodeJS - Set menu.json Path", MB_OK | MB_ICONWARNING);
    }
}

static void openScriptsFolder()
{
    if (g_processRunning.load()) {
        MessageBoxW(g_nppData._nppHandle,
                    L"Please wait until the current Node.js script has finished.",
                    L"NppNodeJS - Open Scripts Folder", MB_OK | MB_ICONINFORMATION);
        return;
    }

    const std::wstring folder = g_scriptFolder.empty()
        ? resolveScriptFolder(loadConfiguredMenuPath(), L"./script")
        : g_scriptFolder;
    if (folder.empty()) return;

    const DWORD attrs = GetFileAttributesW(folder.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        const int answer = MessageBoxW(
            g_nppData._nppHandle,
            L"The scripts folder does not exist. Create it?",
            L"NppNodeJS - Open Scripts Folder",
            MB_YESNO | MB_ICONQUESTION);
        if (answer != IDYES) return;
        const int result = SHCreateDirectoryExW(g_nppData._nppHandle, folder.c_str(), nullptr);
        if (result != ERROR_SUCCESS && result != ERROR_FILE_EXISTS && result != ERROR_ALREADY_EXISTS) {
            MessageBoxW(g_nppData._nppHandle,
                        L"Unable to create the scripts folder.",
                        L"NppNodeJS - Open Scripts Folder", MB_OK | MB_ICONERROR);
            return;
        }
    }

    const HINSTANCE result = ShellExecuteW(
        g_nppData._nppHandle, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        MessageBoxW(g_nppData._nppHandle,
                    L"Unable to open the scripts folder.",
                    L"NppNodeJS - Open Scripts Folder", MB_OK | MB_ICONWARNING);
    }
}

static void openMenuJson()
{
    const std::wstring path = g_menuJsonPath.empty() ? loadConfiguredMenuPath() : g_menuJsonPath;
    if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(g_nppData._nppHandle,
                    L"The current menu.json file does not exist.",
                    L"NppNodeJS - Open menu.json", MB_OK | MB_ICONERROR);
        return;
    }
    SendMessageW(g_nppData._nppHandle, NPPM_DOOPEN, 0,
                 reinterpret_cast<LPARAM>(path.c_str()));
}

static void rebuildMenu()
{
    if (g_processRunning.load()) {
        MessageBoxW(g_nppData._nppHandle,
                    L"Please wait until the current Node.js script has finished before rebuilding the menu.",
                    L"NppNodeJS - Rebuild Menu", MB_OK | MB_ICONINFORMATION);
        return;
    }

    const std::wstring path = g_menuJsonPath.empty() ? loadConfiguredMenuPath() : g_menuJsonPath;
    if (path.empty()) return;

    // installMenus() validates the new configuration before removing the current menu.
    if (!installMenus(path, true)) {
        installMenus(path, false);
    }
}

static void showAbout()
{
    const HINSTANCE result = ShellExecuteW(
        g_nppData._nppHandle, L"open",
        L"https://github.com/seantw/NppNodeJS",
        nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        MessageBoxW(g_nppData._nppHandle,
                    L"Unable to open the NppNodeJS GitHub page.",
                    L"NppNodeJS", MB_OK | MB_ICONWARNING);
    }
}

static HWND currentScintilla()
{
    if (!g_nppData._nppHandle) return nullptr;
    int which = 0;
    SendMessageW(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&which));
    return which == 0 ? g_nppData._scintillaMainHandle : g_nppData._scintillaSecondHandle;
}

static std::wstring filterAnsi(const std::wstring& input)
{
    std::wstring out; out.reserve(input.size());
    for (size_t i = 0; i < input.size();) {
        if (input[i] != L'\x1b') { out.push_back(input[i++]); continue; }
        ++i; if (i >= input.size()) break;
        if (input[i] == L'[') {
            ++i; while (i < input.size()) { wchar_t c = input[i++]; if (c >= 0x40 && c <= 0x7E) break; }
        } else if (input[i] == L']') {
            ++i; while (i < input.size()) { wchar_t c = input[i++]; if (c == L'\a') break; if (c == L'\x1b' && i < input.size() && input[i] == L'\\') { ++i; break; } }
        } else { ++i; }
    }
    return out;
}

static std::string wideToUtf8(const std::wstring& s)
{
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

static std::wstring normalizeOutputNewlines(const std::wstring& text)
{
    std::wstring out;
    out.reserve(text.size() + 16);
    for (size_t i = 0; i < text.size(); ++i) {
        const wchar_t c = text[i];
        if (c == L'\r') {
            out.push_back(L'\r');
            if (i + 1 < text.size() && text[i + 1] == L'\n') {
                out.push_back(L'\n');
                ++i;
            } else {
                out.push_back(L'\n');
            }
        } else if (c == L'\n') {
            out.append(L"\r\n");
        } else {
            out.push_back(c);
        }
    }
    return out;
}

static void clearOutputPane()
{
    if (!g_outputEdit) return;
    debugLog(L"clearOutputPane edit=" + hexPtr(g_outputEdit));

    // Scintilla refuses document modifications while read-only is enabled.
    // The output pane is normally read-only, so temporarily unlock it.
    SendMessageW(g_outputEdit, 2171 /* SCI_SETREADONLY */, FALSE, 0);
    SendMessageW(g_outputEdit, 2004 /* SCI_CLEARALL */, 0, 0);
    SendMessageW(g_outputEdit, 2171 /* SCI_SETREADONLY */, TRUE, 0);
}

static void appendOutput(const std::wstring& text)
{
    if (!g_outputEdit || text.empty()) {
        debugLog(L"appendOutput skipped edit=" + hexPtr(g_outputEdit) + L" textEmpty=" + std::to_wstring(text.empty()));
        return;
    }
    debugLog(L"appendOutput input wchar=" + std::to_wstring(text.size()));
    const std::wstring filtered = normalizeOutputNewlines(filterAnsi(text));
    if (filtered.empty()) { debugLog(L"appendOutput filtered empty"); return; }
    const std::string utf8 = wideToUtf8(filtered);
    if (utf8.empty()) { debugLog(L"appendOutput UTF-8 conversion empty"); return; }
    debugLog(L"appendOutput filtered wchar=" + std::to_wstring(filtered.size()) + L" utf8=" + std::to_wstring(utf8.size()));

    // SCI_APPENDTEXT is a document modification and is ignored while the
    // Scintilla document is read-only. Temporarily unlock for each chunk.
    constexpr UINT SCI_GETLENGTH = 2008;
    constexpr UINT SCI_GOTOPOS = 2025;
    constexpr UINT SCI_ADDTEXT = 2001;

    SendMessageW(g_outputEdit, 2171 /* SCI_SETREADONLY */, FALSE, 0);
    // Put the insertion point at EOF explicitly.  This avoids depending on
    // the initial caret position of the newly created Scintilla control.
    const LRESULT length = SendMessageW(g_outputEdit, SCI_GETLENGTH, 0, 0);
    debugLog(L"appendOutput before length=" + std::to_wstring(length));
    SendMessageW(g_outputEdit, SCI_GOTOPOS, static_cast<WPARAM>(length), 0);
    SendMessageW(g_outputEdit, SCI_ADDTEXT, static_cast<WPARAM>(utf8.size()),
                 reinterpret_cast<LPARAM>(utf8.data()));

    // Move the caret to the end so the newest output stays visible.
    const LRESULT newLength = SendMessageW(g_outputEdit, SCI_GETLENGTH, 0, 0);
    debugLog(L"appendOutput after length=" + std::to_wstring(newLength));
    SendMessageW(g_outputEdit, SCI_GOTOPOS, static_cast<WPARAM>(newLength), 0);
    SendMessageW(g_outputEdit, 2169 /* SCI_SCROLLCARET */, 0, 0);
    SendMessageW(g_outputEdit, 2171 /* SCI_SETREADONLY */, TRUE, 0);
}

static void updateOutputPaneTitle(bool running)
{
    if (!g_outputPane) return;

    // Notepad++ keeps the DockedWidgetData pointer registered with the
    // docking manager. Keep pszAddInfo pointing to one permanent buffer
    // instead of replacing the pointer or changing uMask after registration.
    // NPPM_DMMUPDATEDISPINFO then redraws the title using the current text.
    wcscpy_s(g_outputAddInfo, running ? L"Running" : L"");
    SendMessageW(g_nppData._nppHandle, NPPM_DMMUPDATEDISPINFO, 0,
                 reinterpret_cast<LPARAM>(g_outputPane));
    g_outputRunningTitle = running;
    debugLog(std::wstring(L"updateOutputPaneTitle running=") + (running ? L"true" : L"false") +
             L" addInfo=\"" + g_outputAddInfo + L"\"");
}

static void showOutputPane()
{
    if (g_outputPane) {
        const LRESULT r = SendMessageW(g_nppData._nppHandle, NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(g_outputPane));
        debugLog(L"showOutputPane pane=" + hexPtr(g_outputPane) + L" result=" + std::to_wstring(r));
        if (g_outputEdit) {
            ShowWindow(g_outputEdit, SW_SHOW);
            RECT er{};
            GetClientRect(g_outputEdit, &er);
            debugLog(L"showOutputPane edit visible=" + std::to_wstring(IsWindowVisible(g_outputEdit)) +
                     L" rect=" + std::to_wstring(er.right - er.left) + L"x" + std::to_wstring(er.bottom - er.top));
        }
    } else {
        debugLog(L"showOutputPane skipped: pane=null");
    }
}

static int readNppBorderWidth()
{
    // Notepad++ stores this under GUIConfig/ScintillaPrimaryView as borderWidth.
    // Prefer local-config mode when doLocalConf.xml exists beside notepad++.exe;
    // otherwise use the normal %APPDATA%\Notepad++\config.xml location.
    wchar_t exePath[MAX_PATH * 4] = {};
    DWORD n = GetModuleFileNameW(nullptr, exePath, static_cast<DWORD>(std::size(exePath)));
    if (!n) return 0;
    std::wstring exe(exePath, n);
    const size_t slash = exe.find_last_of(L"\\/");
    const std::wstring exeDir = slash == std::wstring::npos ? L"." : exe.substr(0, slash);

    std::wstring configPath;
    const std::wstring localMarker = exeDir + L"\\doLocalConf.xml";
    if (GetFileAttributesW(localMarker.c_str()) != INVALID_FILE_ATTRIBUTES) {
        configPath = exeDir + L"\\config.xml";
    } else {
        wchar_t appData[MAX_PATH * 4] = {};
        DWORD m = GetEnvironmentVariableW(L"APPDATA", appData, static_cast<DWORD>(std::size(appData)));
        if (!m || m >= std::size(appData)) return 0;
        configPath = std::wstring(appData, m) + L"\\Notepad++\\config.xml";
    }

    std::ifstream f(configPath.c_str(), std::ios::binary);
    if (!f) {
        debugLog(L"readNppBorderWidth config not found=" + configPath);
        return 0;
    }
    std::string xml((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const std::string view = "name=\"ScintillaPrimaryView\"";
    const size_t viewPos = xml.find(view);
    if (viewPos == std::string::npos) {
        debugLog(L"readNppBorderWidth ScintillaPrimaryView not found path=" + configPath);
        return 0;
    }
    const size_t end = xml.find('>', viewPos);
    if (end == std::string::npos) return 0;
    const size_t attr = xml.find("borderWidth=\"", viewPos);
    if (attr == std::string::npos || attr >= end) {
        debugLog(L"readNppBorderWidth borderWidth not found path=" + configPath);
        return 0;
    }
    const size_t valueStart = attr + std::strlen("borderWidth=\"");
    const size_t valueEnd = xml.find('"', valueStart);
    if (valueEnd == std::string::npos || valueEnd > end) return 0;
    const int value = std::clamp(std::atoi(xml.substr(valueStart, valueEnd - valueStart).c_str()), 0, 30);
    debugLog(L"readNppBorderWidth path=" + configPath + L" value=" + std::to_wstring(value));
    return value;
}

static void applyOutputPaneBorderLayout(HWND pane)
{
    if (!pane || !g_outputEdit) return;
    RECT rc{};
    GetClientRect(pane, &rc);
    const int b = std::max(0, g_outputBorderWidth);
    const int width = static_cast<int>(std::max(0L, (rc.right - rc.left) - b * 2));
    const int height = static_cast<int>(std::max(0L, (rc.bottom - rc.top) - b * 2));
    ShowWindow(g_outputEdit, SW_SHOW);
    SetWindowPos(g_outputEdit, nullptr, b, b, width, height,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
}

static void applyOutputStyle()
{
    if (!g_outputEdit) { debugLog(L"applyOutputStyle skipped: edit=null"); return; }
    HWND sci = currentScintilla();
    if (!sci) { debugLog(L"applyOutputStyle skipped: current scintilla=null"); return; }

    g_outputBorderWidth = readNppBorderWidth();

    constexpr int STYLE_DEFAULT = 32;
    constexpr UINT SCI_STYLECLEARALL = 2050;
    constexpr UINT SCI_STYLESETFORE = 2051;
    constexpr UINT SCI_STYLESETBACK = 2052;
    constexpr UINT SCI_STYLESETBOLD = 2053;
    constexpr UINT SCI_STYLESETITALIC = 2054;
    constexpr UINT SCI_STYLESETSIZE = 2055;
    constexpr UINT SCI_STYLESETFONT = 2056;
    constexpr UINT SCI_STYLEGETFORE = 2481;
    constexpr UINT SCI_STYLEGETBACK = 2482;
    constexpr UINT SCI_STYLEGETBOLD = 2483;
    constexpr UINT SCI_STYLEGETITALIC = 2484;
    constexpr UINT SCI_STYLEGETSIZE = 2485;
    constexpr UINT SCI_STYLEGETFONT = 2486;
    constexpr UINT SCI_GETTECHNOLOGY = 2631;
    constexpr UINT SCI_SETTECHNOLOGY = 2630;
    constexpr UINT SCI_SETLEXER = 4001;
    constexpr UINT SCI_SETCODEPAGE = 2037;
    constexpr UINT SCI_SETREADONLY = 2171;
    constexpr UINT SCI_SETSEL = 2160;
    constexpr UINT SCI_SETWRAPMODE = 2268;
    constexpr UINT SCI_SETHSCROLLBAR = 2130;
    constexpr UINT SCI_SETSCROLLWIDTH = 2274;
    constexpr UINT SCI_SETSCROLLWIDTHTRACKING = 2516;
    constexpr UINT SCI_SETMARGINWIDTHN = 2242;
    constexpr UINT SCI_GETMARGINLEFT = 2156;
    constexpr UINT SCI_SETMARGINLEFT = 2155;
    constexpr UINT SCI_GETMARGINRIGHT = 2158;
    constexpr UINT SCI_SETMARGINRIGHT = 2157;
    constexpr UINT SCI_GETELEMENTCOLOUR = 2754;
    constexpr UINT SCI_SETELEMENTCOLOUR = 2753;
    constexpr UINT SCI_GETSELECTIONLAYER = 2762;
    constexpr UINT SCI_SETSELECTIONLAYER = 2763;
    constexpr UINT SCI_GETSELALPHA = 2477;
    constexpr UINT SCI_SETSELALPHA = 2478;
    constexpr int SC_ELEMENT_SELECTION_TEXT = 10;
    constexpr int SC_ELEMENT_SELECTION_BACK = 11;
    constexpr UINT SCI_SETCARETFORE = 2069;

    // Read the active document's default style.  A freshly created Scintilla
    // may return an unset style value, so keep a visible fallback.
    const LRESULT docFore = SendMessageW(sci, SCI_STYLEGETFORE, STYLE_DEFAULT, 0);
    const LRESULT docBack = SendMessageW(sci, SCI_STYLEGETBACK, STYLE_DEFAULT, 0);
    g_outputFore = (docFore >= 0 && docFore <= 0xFFFFFF)
        ? static_cast<COLORREF>(docFore) : RGB(0, 0, 0);
    g_outputBack = (docBack >= 0 && docBack <= 0xFFFFFF)
        ? static_cast<COLORREF>(docBack) : RGB(255, 255, 255);
    g_outputTechnology = static_cast<int>(SendMessageW(sci, SCI_GETTECHNOLOGY, 0, 0));
    const LRESULT bold = SendMessageW(sci, SCI_STYLEGETBOLD, STYLE_DEFAULT, 0);
    const LRESULT italic = SendMessageW(sci, SCI_STYLEGETITALIC, STYLE_DEFAULT, 0);
    const LRESULT size = SendMessageW(sci, SCI_STYLEGETSIZE, STYLE_DEFAULT, 0);
    debugLog(L"applyOutputStyle source sci=" + hexPtr(sci) +
             L" fore=" + std::to_wstring(docFore) + L" back=" + std::to_wstring(docBack) +
             L" tech=" + std::to_wstring(g_outputTechnology) + L" bold=" + std::to_wstring(bold) +
             L" italic=" + std::to_wstring(italic) + L" size=" + std::to_wstring(size));

    char fontName[256] = {};
    SendMessageA(sci, SCI_STYLEGETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>(fontName));

    // SCI_STYLECLEARALL copies STYLE_DEFAULT to all styles.  It therefore has
    // to happen before setting our final STYLE_DEFAULT values.
    SendMessageW(g_outputEdit, SCI_STYLECLEARALL, 0, 0);
    SendMessageW(g_outputEdit, SCI_SETCODEPAGE, 65001 /* SC_CP_UTF8 */, 0);
    SendMessageW(g_outputEdit, SCI_SETLEXER, 0 /* SCLEX_NULL */, 0);

    if (g_outputTechnology >= 0 && g_outputTechnology <= 4)
        SendMessageW(g_outputEdit, SCI_SETTECHNOLOGY, static_cast<WPARAM>(g_outputTechnology), 0);

    SendMessageW(g_outputEdit, SCI_STYLESETFORE, STYLE_DEFAULT, g_outputFore);
    SendMessageW(g_outputEdit, SCI_STYLESETBACK, STYLE_DEFAULT, g_outputBack);
    SendMessageW(g_outputEdit, SCI_STYLESETBOLD, STYLE_DEFAULT, bold);
    SendMessageW(g_outputEdit, SCI_STYLESETITALIC, STYLE_DEFAULT, italic);
    if (size > 0) SendMessageW(g_outputEdit, SCI_STYLESETSIZE, STYLE_DEFAULT, size);
    if (fontName[0])
        SendMessageA(g_outputEdit, SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>(fontName));

    // Match the active editor's selection appearance.
    const LRESULT selectionFore = SendMessageW(sci, SCI_GETELEMENTCOLOUR, SC_ELEMENT_SELECTION_TEXT, 0);
    const LRESULT selectionBack = SendMessageW(sci, SCI_GETELEMENTCOLOUR, SC_ELEMENT_SELECTION_BACK, 0);
    if (selectionFore >= 0)
        SendMessageW(g_outputEdit, SCI_SETELEMENTCOLOUR, SC_ELEMENT_SELECTION_TEXT, selectionFore);
    if (selectionBack >= 0)
        SendMessageW(g_outputEdit, SCI_SETELEMENTCOLOUR, SC_ELEMENT_SELECTION_BACK, selectionBack);
    const LRESULT selectionAlpha = SendMessageW(sci, SCI_GETSELALPHA, 0, 0);
    if (selectionAlpha >= 0)
        SendMessageW(g_outputEdit, SCI_SETSELALPHA, selectionAlpha, 0);
    const LRESULT selectionLayer = SendMessageW(sci, SCI_GETSELECTIONLAYER, 0, 0);
    if (selectionLayer >= 0)
        SendMessageW(g_outputEdit, SCI_SETSELECTIONLAYER, selectionLayer, 0);

    // Match the active editor's blank margins around the text.
    const LRESULT marginLeft = SendMessageW(sci, SCI_GETMARGINLEFT, 0, 0);
    const LRESULT marginRight = SendMessageW(sci, SCI_GETMARGINRIGHT, 0, 0);
    SendMessageW(g_outputEdit, SCI_SETMARGINLEFT, 0, marginLeft >= 0 ? marginLeft : 1);
    SendMessageW(g_outputEdit, SCI_SETMARGINRIGHT, 0, marginRight >= 0 ? marginRight : 1);

    // Match the active editor's native border / client edge.
    const LONG_PTR sourceStyle = GetWindowLongPtrW(sci, GWL_STYLE);
    LONG_PTR outputStyle = GetWindowLongPtrW(g_outputEdit, GWL_STYLE);
    if (sourceStyle & WS_BORDER) outputStyle |= WS_BORDER;
    else outputStyle &= ~static_cast<LONG_PTR>(WS_BORDER);
    SetWindowLongPtrW(g_outputEdit, GWL_STYLE, outputStyle);
    const LONG_PTR sourceExStyle = GetWindowLongPtrW(sci, GWL_EXSTYLE);
    LONG_PTR outputExStyle = GetWindowLongPtrW(g_outputEdit, GWL_EXSTYLE);
    if (sourceExStyle & WS_EX_CLIENTEDGE) outputExStyle |= WS_EX_CLIENTEDGE;
    else outputExStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
    SetWindowLongPtrW(g_outputEdit, GWL_EXSTYLE, outputExStyle);
    SetWindowPos(g_outputEdit, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    applyOutputPaneBorderLayout(g_outputPane);

    // This is a log pane rather than a source editor.
    for (int margin = 0; margin < 5; ++margin)
        SendMessageW(g_outputEdit, SCI_SETMARGINWIDTHN, margin, 0);
    SendMessageW(g_outputEdit, SCI_SETWRAPMODE, 1 /* SC_WRAP_WORD */, 0);
    SendMessageW(g_outputEdit, SCI_SETHSCROLLBAR, FALSE, 0);
    SendMessageW(g_outputEdit, SCI_SETSCROLLWIDTH, 1, 0);
    SendMessageW(g_outputEdit, SCI_SETSCROLLWIDTHTRACKING, FALSE, 0);
    SendMessageW(g_outputEdit, SCI_SETCARETFORE, g_outputFore, 0);
    SendMessageW(g_outputEdit, SCI_SETREADONLY, TRUE, 0);
    SendMessageW(g_outputEdit, SCI_SETSEL, 0, 0);
    InvalidateRect(g_outputEdit, nullptr, TRUE);
}

static void resizeOutputEdit(HWND hwnd)
{
    RECT rc{};
    GetClientRect(hwnd, &rc);

    if (g_outputEdit) {
        applyOutputPaneBorderLayout(hwnd);
    }
}

static LRESULT CALLBACK outputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        debugLog(L"outputWndProc WM_CREATE pane=" + hexPtr(hwnd));
        g_outputEdit = reinterpret_cast<HWND>(SendMessageW(
            g_nppData._nppHandle, NPPM_CREATESCINTILLAHANDLE, 0,
            reinterpret_cast<LPARAM>(hwnd)));
        debugLog(L"NPPM_CREATESCINTILLAHANDLE edit=" + hexPtr(g_outputEdit));
        if (!g_outputEdit) return -1;
        applyOutputStyle();

        RECT er{};
        GetClientRect(g_outputEdit, &er);
        wchar_t cls[64] = {};
        GetClassNameW(g_outputEdit, cls, ARRAYSIZE(cls));
        debugLog(L"diagnostic edit visible=" + std::to_wstring(IsWindowVisible(g_outputEdit)) +
                 L" enabled=" + std::to_wstring(IsWindowEnabled(g_outputEdit)) +
                 L" rect=" + std::to_wstring(er.right - er.left) + L"x" + std::to_wstring(er.bottom - er.top) +
                 L" class=" + std::wstring(cls));
        return 0;
    }
    case WM_SIZE:
        resizeOutputEdit(hwnd);
        return 0;
    case WM_DESTROY:
        g_outputEdit = nullptr;
        g_outputPane = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool createOutputPane()
{
    debugLog(L"createOutputPane enter pane=" + hexPtr(g_outputPane));
    if (g_outputPane) return true;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = outputWndProc;
    wc.hInstance = g_hModule;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = OUTPUT_CLASS_NAME;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    g_outputPane = CreateWindowExW(0, OUTPUT_CLASS_NAME, g_outputTitle.c_str(),
                                   WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                   0, 0, 0, 0, g_nppData._nppHandle, nullptr, g_hModule, nullptr);
    if (!g_outputPane) { debugLog(L"createOutputPane CreateWindowExW failed error=" + std::to_wstring(GetLastError())); return false; }
    debugLog(L"createOutputPane created pane=" + hexPtr(g_outputPane));

    g_outputDock = {};
    g_outputDock.hClient = g_outputPane;
    g_outputDock.pszName = g_outputTitle.c_str();
    g_outputDock.dlgID = OUTPUT_DLG_ID;
    g_outputDock.uMask = DWS_DF_CONT_BOTTOM | DWS_ADDINFO;
    g_outputAddInfo[0] = L'\0';
    g_outputDock.pszAddInfo = g_outputAddInfo;
    g_outputDock.pszModuleName = PLUGIN_NAME;

    const LRESULT dockResult = SendMessageW(g_nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0,
                      reinterpret_cast<LPARAM>(&g_outputDock));
    debugLog(L"NPPM_DMMREGASDCKDLG result=" + std::to_wstring(dockResult) +
             L" pane=" + hexPtr(g_outputPane) + L" edit=" + hexPtr(g_outputEdit));
    if (!dockResult) {
        DestroyWindow(g_outputPane);
        g_outputPane = nullptr;
        return false;
    }
    return true;
}

static void destroyOutputPane()
{
    if (!g_outputPane) return;
    SendMessageW(g_nppData._nppHandle, NPPM_DMMHIDE, 0, reinterpret_cast<LPARAM>(g_outputPane));
    DestroyWindow(g_outputPane);
    g_outputPane = nullptr;
    g_outputEdit = nullptr;
    UnregisterClassW(OUTPUT_CLASS_NAME, g_hModule);
}

class Utf8StreamDecoder {
public:
    void feed(const char* data, size_t size, const std::function<void(const std::wstring&)>& emit)
    {
        _pending.append(data, size);
        size_t complete = 0;
        while (complete < _pending.size()) {
            const unsigned char c = static_cast<unsigned char>(_pending[complete]);
            size_t need = 1;
            if (c >= 0xC2 && c <= 0xDF) need = 2;
            else if (c >= 0xE0 && c <= 0xEF) need = 3;
            else if (c >= 0xF0 && c <= 0xF4) need = 4;
            else if (c >= 0x80 && c <= 0xBF) { ++complete; continue; }
            if (complete + need > _pending.size()) break;
            bool valid = true;
            for (size_t j = 1; j < need; ++j) {
                const unsigned char cc = static_cast<unsigned char>(_pending[complete + j]);
                if ((cc & 0xC0) != 0x80) { valid = false; break; }
            }
            if (!valid) { ++complete; continue; }
            complete += need;
        }
        if (complete == 0) return;
        const std::wstring w = utf8ChunkToWide(_pending.substr(0, complete));
        if (!w.empty()) emit(w);
        _pending.erase(0, complete);
    }

    void finish(const std::function<void(const std::wstring&)>& emit)
    {
        if (!_pending.empty()) {
            const std::wstring w = utf8ChunkToWide(_pending);
            if (!w.empty()) emit(w);
            _pending.clear();
        }
    }
private:
    std::string _pending;
};

static std::string encodeBase64(const std::string& input);
static std::string decodeBase64(const std::string& encoded, bool& ok);
static std::wstring getCurrentFilePath();
static std::pair<long, long> getCurrentCursor();

struct SystemDialogButtonTextState {
    std::wstring ok;
    std::wstring cancel;
};

static std::pair<std::wstring, std::wstring> getSystemDialogButtonTexts()
{
    // Read the standard MessageBox button captions directly from the localized
    // USER32 system resource.  Do not create a temporary MessageBox here: doing
    // so can activate/focus an unrelated window after the custom dialog closes.
    HMODULE user32Data = LoadLibraryExW(L"user32.dll", nullptr, LOAD_LIBRARY_AS_DATAFILE);
    if (user32Data) {
        wchar_t okBuffer[128] = {};
        wchar_t cancelBuffer[128] = {};

        const int okLength = LoadStringW(user32Data, 800, okBuffer,
                                         static_cast<int>(std::size(okBuffer)));
        const int cancelLength = LoadStringW(user32Data, 801, cancelBuffer,
                                             static_cast<int>(std::size(cancelBuffer)));

        FreeLibrary(user32Data);

        if (okLength > 0 && cancelLength > 0) {
            return { std::wstring(okBuffer, okLength),
                     std::wstring(cancelBuffer, cancelLength) };
        }
    }

    // Fallback only if the USER32 resources cannot be loaded.
    return { L"OK", L"Cancel" };
}

static bool showPromptDialog(const std::wstring& message, const std::wstring& defaultValue,
                             const std::wstring& title, std::wstring& value)
{
    struct PromptState {
        const std::wstring* message;
        const std::wstring* defaultValue;
        std::wstring* value;
        std::wstring okText;
        std::wstring cancelText;
        HFONT font = nullptr;
    } state{ &message, &defaultValue, &value };

    static const int IDC_PROMPT_EDIT = 1001;
    static const int IDC_PROMPT_MESSAGE = 1002;

    auto dialogProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> INT_PTR {
        PromptState* state = reinterpret_cast<PromptState*>(GetWindowLongPtrW(hwnd, DWLP_USER));

        if (msg == WM_INITDIALOG) {
            state = reinterpret_cast<PromptState*>(lParam);
            SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
            const auto buttonTexts = getSystemDialogButtonTexts();
            state->okText = buttonTexts.first;
            state->cancelText = buttonTexts.second;

            // MessageBox uses the system message-box font, not the 9pt dialog-template font.
            // Use the same NONCLIENTMETRICS message font here.
            NONCLIENTMETRICSW ncm{};
            ncm.cbSize = sizeof(ncm);
            if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
                state->font = CreateFontIndirectW(&ncm.lfMessageFont);
            }
            if (!state->font) {
                state->font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            }

            HDC dc = GetDC(hwnd);
            HFONT oldFont = state->font
                ? static_cast<HFONT>(SelectObject(dc, state->font))
                : nullptr;

            const int minWidth = 320;
            const int maxWidth = 560;
            const int contentWidth = maxWidth - 20;

            RECT singleLineRect{ 0, 0, 0, 0 };
            DrawTextW(dc, state->message->c_str(), -1, &singleLineRect,
                      DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

            int dialogWidth = static_cast<int>(singleLineRect.right) + 40;
            dialogWidth = std::clamp(dialogWidth, minWidth, maxWidth);

            RECT wrappedRect{ 0, 0, contentWidth, 0 };
            DrawTextW(dc, state->message->c_str(), -1, &wrappedRect,
                      DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            const int messageHeight = std::max(
                20, static_cast<int>(wrappedRect.bottom));

            if (oldFont) SelectObject(dc, oldFont);
            ReleaseDC(hwnd, dc);

            const int actualContentWidth = dialogWidth - 20;
            const int editY = 8 + messageHeight + 10;
            const int editHeight = 24;
            const int buttonY = editY + editHeight + 14;
            const int buttonHeight = 27;
            const int bottomMargin = 12;
            const int desiredClientHeight = buttonY + buttonHeight + bottomMargin;

            const int buttonWidth = 78;
            const int buttonGap = 8;
            const int buttonsWidth = buttonWidth * 2 + buttonGap;
            const int buttonsX = actualContentWidth - buttonsWidth + 10;

            HWND messageCtrl = CreateWindowExW(
                0, L"STATIC", state->message->c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                10, 8, actualContentWidth, messageHeight, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROMPT_MESSAGE)),
                g_hModule, nullptr);

            HWND editCtrl = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", state->defaultValue->c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                10, editY, actualContentWidth, editHeight, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROMPT_EDIT)),
                g_hModule, nullptr);

            HWND okCtrl = CreateWindowExW(
                0, L"BUTTON", state->okText.c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                buttonsX, buttonY, buttonWidth, buttonHeight, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
                g_hModule, nullptr);

            HWND cancelCtrl = CreateWindowExW(
                0, L"BUTTON", state->cancelText.c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                buttonsX + buttonWidth + buttonGap, buttonY,
                buttonWidth, buttonHeight, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
                g_hModule, nullptr);

            if (state->font) {
                SendMessageW(messageCtrl, WM_SETFONT,
                              reinterpret_cast<WPARAM>(state->font), TRUE);
                SendMessageW(editCtrl, WM_SETFONT,
                              reinterpret_cast<WPARAM>(state->font), TRUE);
                SendMessageW(okCtrl, WM_SETFONT,
                              reinterpret_cast<WPARAM>(state->font), TRUE);
                SendMessageW(cancelCtrl, WM_SETFONT,
                              reinterpret_cast<WPARAM>(state->font), TRUE);
            }

            // SetWindowPos() sizes the outer dialog window, while the coordinates
            // above are client coordinates. Convert the desired client size first.
            RECT windowRect{ 0, 0, dialogWidth, desiredClientHeight };
            DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
            DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
            AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);

            const int windowWidth = windowRect.right - windowRect.left;
            const int windowHeight = windowRect.bottom - windowRect.top;

            // Match MessageBoxW: center the prompt over the Notepad++ main window.
            // Because the final size is calculated above, compute the position after
            // resizing rather than relying on the dialog's initial template size.
            RECT ownerRect{};
            GetWindowRect(g_nppData._nppHandle, &ownerRect);
            const int ownerWidth = ownerRect.right - ownerRect.left;
            const int ownerHeight = ownerRect.bottom - ownerRect.top;
            const int x = ownerRect.left + (ownerWidth - windowWidth) / 2;
            const int y = ownerRect.top + (ownerHeight - windowHeight) / 2;

            SetWindowPos(hwnd, HWND_TOP, x, y, windowWidth, windowHeight,
                         SWP_NOACTIVATE);

            SendMessageW(editCtrl, EM_SETSEL, 0, -1);
            SetFocus(editCtrl);
            return FALSE;
        }

        if (msg == WM_COMMAND) {
            const int id = LOWORD(wParam);
            if (id == IDOK) {
                const int length = GetWindowTextLengthW(
                    GetDlgItem(hwnd, IDC_PROMPT_EDIT));
                std::wstring buffer(static_cast<size_t>(length) + 1, L'\0');
                if (length > 0) {
                    GetDlgItemTextW(hwnd, IDC_PROMPT_EDIT, buffer.data(), length + 1);
                    buffer.resize(static_cast<size_t>(length));
                } else {
                    buffer.clear();
                }
                *state->value = std::move(buffer);
                EndDialog(hwnd, IDOK);
                return TRUE;
            }
            if (id == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            }
        }

        if (msg == WM_NCDESTROY && state && state->font) {
            DeleteObject(state->font);
            state->font = nullptr;
        }

        return FALSE;
    };

    // Build only the dialog itself dynamically; child controls are created in WM_INITDIALOG.
    // Give it a generous initial client area; WM_INITDIALOG resizes the outer window
    // after measuring the actual controls.
    std::vector<BYTE> data(1024, 0);
    size_t pos = 0;
    auto putWord = [&](WORD v) {
        *reinterpret_cast<WORD*>(data.data() + pos) = v; pos += sizeof(WORD);
    };
    auto putDword = [&](DWORD v) {
        *reinterpret_cast<DWORD*>(data.data() + pos) = v; pos += sizeof(DWORD);
    };
    auto putString = [&](const wchar_t* text) {
        while (*text) { putWord(static_cast<WORD>(*text++)); }
        putWord(0);
    };

    putDword(DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    putDword(WS_EX_DLGMODALFRAME);
    putWord(0);
    putWord(10); putWord(10); putWord(400); putWord(180);
    putWord(0);
    putWord(0);
    putString(title.c_str());

    return DialogBoxIndirectParamW(g_hModule,
                                   reinterpret_cast<const DLGTEMPLATE*>(data.data()),
                                   g_nppData._nppHandle,
                                   dialogProc,
                                   reinterpret_cast<LPARAM>(&state)) == IDOK;
}

static bool showConfirmDialog(const std::wstring& message, const std::wstring& title)
{
    struct ConfirmState {
        const std::wstring* message;
        std::wstring okText;
        std::wstring cancelText;
        HFONT font = nullptr;
    } state{ &message };

    static const int IDC_CONFIRM_MESSAGE = 1101;

    auto dialogProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> INT_PTR {
        ConfirmState* state = reinterpret_cast<ConfirmState*>(GetWindowLongPtrW(hwnd, DWLP_USER));

        if (msg == WM_INITDIALOG) {
            state = reinterpret_cast<ConfirmState*>(lParam);
            SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
            const auto buttonTexts = getSystemDialogButtonTexts();
            state->okText = buttonTexts.first;
            state->cancelText = buttonTexts.second;

            NONCLIENTMETRICSW ncm{};
            ncm.cbSize = sizeof(ncm);
            if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
                state->font = CreateFontIndirectW(&ncm.lfMessageFont);
            }
            if (!state->font) {
                state->font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            }

            HDC dc = GetDC(hwnd);
            HFONT oldFont = state->font
                ? static_cast<HFONT>(SelectObject(dc, state->font))
                : nullptr;

            const int minWidth = 320;
            const int maxWidth = 560;
            const int contentWidth = maxWidth - 20;

            RECT singleLineRect{ 0, 0, 0, 0 };
            DrawTextW(dc, state->message->c_str(), -1, &singleLineRect,
                      DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

            int dialogWidth = static_cast<int>(singleLineRect.right) + 40;
            dialogWidth = std::clamp(dialogWidth, minWidth, maxWidth);

            RECT wrappedRect{ 0, 0, contentWidth, 0 };
            DrawTextW(dc, state->message->c_str(), -1, &wrappedRect,
                      DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            const int messageHeight = std::max(20, static_cast<int>(wrappedRect.bottom));

            if (oldFont) SelectObject(dc, oldFont);
            ReleaseDC(hwnd, dc);

            const int actualContentWidth = dialogWidth - 20;
            const int buttonY = 8 + messageHeight + 16;
            const int buttonHeight = 27;
            const int bottomMargin = 12;
            const int desiredClientHeight = buttonY + buttonHeight + bottomMargin;

            const int buttonWidth = 78;
            const int buttonGap = 8;
            const int buttonsWidth = buttonWidth * 2 + buttonGap;
            const int buttonsX = actualContentWidth - buttonsWidth + 10;

            HWND messageCtrl = CreateWindowExW(
                0, L"STATIC", state->message->c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                10, 8, actualContentWidth, messageHeight, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CONFIRM_MESSAGE)),
                g_hModule, nullptr);

            HWND okCtrl = CreateWindowExW(
                0, L"BUTTON", state->okText.c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                buttonsX, buttonY, buttonWidth, buttonHeight, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
                g_hModule, nullptr);

            HWND cancelCtrl = CreateWindowExW(
                0, L"BUTTON", state->cancelText.c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                buttonsX + buttonWidth + buttonGap, buttonY,
                buttonWidth, buttonHeight, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
                g_hModule, nullptr);

            if (state->font) {
                SendMessageW(messageCtrl, WM_SETFONT,
                              reinterpret_cast<WPARAM>(state->font), TRUE);
                SendMessageW(okCtrl, WM_SETFONT,
                              reinterpret_cast<WPARAM>(state->font), TRUE);
                SendMessageW(cancelCtrl, WM_SETFONT,
                              reinterpret_cast<WPARAM>(state->font), TRUE);
            }

            RECT windowRect{ 0, 0, dialogWidth, desiredClientHeight };
            DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
            DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
            AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);

            const int windowWidth = windowRect.right - windowRect.left;
            const int windowHeight = windowRect.bottom - windowRect.top;

            RECT ownerRect{};
            GetWindowRect(g_nppData._nppHandle, &ownerRect);
            const int ownerWidth = ownerRect.right - ownerRect.left;
            const int ownerHeight = ownerRect.bottom - ownerRect.top;
            const int x = ownerRect.left + (ownerWidth - windowWidth) / 2;
            const int y = ownerRect.top + (ownerHeight - windowHeight) / 2;

            SetWindowPos(hwnd, HWND_TOP, x, y, windowWidth, windowHeight,
                         SWP_NOACTIVATE);
            SetFocus(okCtrl);
            return FALSE;
        }

        if (msg == WM_COMMAND) {
            const int id = LOWORD(wParam);
            if (id == IDOK) {
                EndDialog(hwnd, IDOK);
                return TRUE;
            }
            if (id == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            }
        }

        if (msg == WM_NCDESTROY && state && state->font) {
            DeleteObject(state->font);
            state->font = nullptr;
        }

        return FALSE;
    };

    std::vector<BYTE> data(1024, 0);
    size_t pos = 0;
    auto putWord = [&](WORD v) {
        *reinterpret_cast<WORD*>(data.data() + pos) = v; pos += sizeof(WORD);
    };
    auto putDword = [&](DWORD v) {
        *reinterpret_cast<DWORD*>(data.data() + pos) = v; pos += sizeof(DWORD);
    };
    auto putString = [&](const wchar_t* text) {
        while (*text) { putWord(static_cast<WORD>(*text++)); }
        putWord(0);
    };

    putDword(DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    putDword(WS_EX_DLGMODALFRAME);
    putWord(0);
    putWord(10); putWord(10); putWord(400); putWord(180);
    putWord(0);
    putWord(0);
    putString(title.c_str());

    return DialogBoxIndirectParamW(g_hModule,
                                   reinterpret_cast<const DLGTEMPLATE*>(data.data()),
                                   g_nppData._nppHandle,
                                   dialogProc,
                                   reinterpret_cast<LPARAM>(&state)) == IDOK;
}

static bool handleConfirmProtocolLine(const std::string& line)
{
    static const std::string prefix = "\x1eNPPNODE_CONFIRM:";
    if (line.rfind(prefix, 0) != 0) return false;

    const std::string payload = line.substr(prefix.size());
    const size_t separator = payload.find('|');
    if (separator == std::string::npos) return false;

    bool messageOk = false, titleOk = false;
    const std::string decodedMessage = decodeBase64(payload.substr(0, separator), messageOk);
    const std::string decodedTitle = decodeBase64(payload.substr(separator + 1), titleOk);
    if (!messageOk || !titleOk) return false;

    const std::wstring message = utf8ToWide(decodedMessage.c_str());
    const std::wstring title = utf8ToWide(decodedTitle.c_str());
    const bool accepted = showConfirmDialog(message, title);

    std::lock_guard<std::mutex> lock(g_stdinMutex);
    if (g_stdinWrite) {
        const char* response = accepted ? "OK\n" : "CANCEL\n";
        DWORD written = 0;
        const BOOL ok = WriteFile(g_stdinWrite, response,
                                  static_cast<DWORD>(std::strlen(response)),
                                  &written, nullptr);
        FlushFileBuffers(g_stdinWrite);
        debugLog(L"protocol confirm response sent accepted=" + std::to_wstring(accepted) +
                 L" WriteFile=" + std::to_wstring(ok) +
                 L" bytes=" + std::to_wstring(written));
    }
    return true;
}

static bool handlePromptProtocolLine(const std::string& line)
{
    static const std::string prefix = "\x1eNPPNODE_PROMPT:";
    if (line.rfind(prefix, 0) != 0) return false;

    const std::string payload = line.substr(prefix.size());
    const size_t separator1 = payload.find('|');
    if (separator1 == std::string::npos) return false;
    const size_t separator2 = payload.find('|', separator1 + 1);
    if (separator2 == std::string::npos) return false;

    bool messageOk = false, defaultOk = false, titleOk = false;
    const std::string decodedMessage = decodeBase64(payload.substr(0, separator1), messageOk);
    const std::string decodedDefault = decodeBase64(payload.substr(separator1 + 1, separator2 - separator1 - 1), defaultOk);
    const std::string decodedTitle = decodeBase64(payload.substr(separator2 + 1), titleOk);
    if (!messageOk || !defaultOk || !titleOk) return false;

    const std::wstring message = utf8ToWide(decodedMessage.c_str());
    const std::wstring defaultValue = utf8ToWide(decodedDefault.c_str());
    const std::wstring title = utf8ToWide(decodedTitle.c_str());
    std::wstring value;
    const bool accepted = showPromptDialog(message, defaultValue, title, value);

    std::lock_guard<std::mutex> lock(g_stdinMutex);
    if (g_stdinWrite) {
        std::string response;
        if (accepted) {
            response = "OK:" + encodeBase64(debugWideToUtf8(value)) + "\n";
        } else {
            response = "CANCEL\n";
        }
        DWORD written = 0;
        const BOOL ok = WriteFile(g_stdinWrite, response.data(), static_cast<DWORD>(response.size()), &written, nullptr);
        FlushFileBuffers(g_stdinWrite);
        debugLog(L"protocol prompt response sent accepted=" + std::to_wstring(accepted) +
                 L" WriteFile=" + std::to_wstring(ok) +
                 L" bytes=" + std::to_wstring(written));
    }
    return true;
}

static std::string decodeBase64(const std::string& encoded, bool& ok)
{
    std::string decoded;
    decoded.reserve((encoded.size() / 4) * 3 + 3);
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int val = 0;
    int bits = -8;
    ok = true;
    for (unsigned char c : encoded) {
        if (c == '=') break;
        const char* p = std::strchr(alphabet, c);
        if (!p) { ok = false; return {}; }
        val = (val << 6) + static_cast<int>(p - alphabet);
        bits += 6;
        if (bits >= 0) {
            decoded.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return decoded;
}

static bool handleGetFileProtocolLine(const std::string& line)
{
    static const std::string prefix = "\x1eNPPNODE_GETFILE:";
    if (line != prefix) return false;

    const std::string response = "OK:" + encodeBase64(debugWideToUtf8(getCurrentFilePath())) + "\n";
    std::lock_guard<std::mutex> lock(g_stdinMutex);
    if (g_stdinWrite) {
        DWORD written = 0;
        const BOOL ok = WriteFile(g_stdinWrite, response.data(),
                                  static_cast<DWORD>(response.size()), &written, nullptr);
        FlushFileBuffers(g_stdinWrite);
        debugLog(L"protocol getFile response sent WriteFile=" + std::to_wstring(ok) +
                 L" bytes=" + std::to_wstring(written));
    }
    return true;
}

static bool writeProtocolResponse(const std::string& response)
{
    std::lock_guard<std::mutex> lock(g_stdinMutex);
    if (!g_stdinWrite) return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(g_stdinWrite, response.data(), static_cast<DWORD>(response.size()), &written, nullptr);
    FlushFileBuffers(g_stdinWrite);
    return ok && written == response.size();
}

static std::wstring getCurrentLineText()
{
    HWND sci = currentScintilla();
    if (!sci) return L"";
    const LRESULT line = SendMessageW(sci, SCI_LINEFROMPOSITION_LOCAL,
                                      static_cast<WPARAM>(SendMessageW(sci, SCI_GETCURRENTPOS_LOCAL, 0, 0)), 0);
    const LRESULT length = SendMessageW(sci, SCI_LINELENGTH_LOCAL, static_cast<WPARAM>(line), 0);
    if (length <= 0) return L"";
    std::string buffer(static_cast<size_t>(length) + 1, '\0');
    SendMessageW(sci, SCI_GETLINE_LOCAL, static_cast<WPARAM>(line), reinterpret_cast<LPARAM>(buffer.data()));
    while (!buffer.empty() && (buffer.back() == '\r' || buffer.back() == '\n')) buffer.pop_back();
    return utf8ToWide(buffer.c_str());
}

static bool replaceCurrentLine(const std::wstring& text)
{
    HWND sci = currentScintilla();
    if (!sci) return false;
    const LRESULT position = SendMessageW(sci, SCI_GETCURRENTPOS_LOCAL, 0, 0);
    const LRESULT line = SendMessageW(sci, SCI_LINEFROMPOSITION_LOCAL, static_cast<WPARAM>(position), 0);
    const LRESULT start = SendMessageW(sci, SCI_POSITIONFROMLINE_LOCAL, static_cast<WPARAM>(line), 0);
    const LRESULT end = SendMessageW(sci, SCI_GETLINEENDPOSITION_LOCAL, static_cast<WPARAM>(line), 0);
    const std::string utf8 = debugWideToUtf8(text);
    SendMessageW(sci, SCI_SETSEL_LOCAL, static_cast<WPARAM>(start), static_cast<LPARAM>(end));
    SendMessageW(sci, SCI_REPLACESEL_LOCAL, 0, reinterpret_cast<LPARAM>(utf8.c_str()));
    return true;
}

static std::wstring getSelectionText()
{
    HWND sci = currentScintilla();
    if (!sci) return L"";
    const LRESULT start = SendMessageW(sci, SCI_GETSELECTIONSTART_LOCAL, 0, 0);
    const LRESULT end = SendMessageW(sci, SCI_GETSELECTIONEND_LOCAL, 0, 0);
    if (start == end) return L"";
    const LRESULT length = SendMessageW(sci, SCI_GETSELTEXT_LOCAL, 0, 0);
    if (length <= 0) return L"";
    std::string buffer(static_cast<size_t>(length) + 1, '\0');
    SendMessageW(sci, SCI_GETSELTEXT_LOCAL, 0, reinterpret_cast<LPARAM>(buffer.data()));
    return utf8ToWide(buffer.c_str());
}

static bool replaceSelectionText(const std::wstring& text)
{
    HWND sci = currentScintilla();
    if (!sci) return false;
    const std::string utf8 = debugWideToUtf8(text);
    SendMessageW(sci, SCI_REPLACESEL_LOCAL, 0, reinterpret_cast<LPARAM>(utf8.c_str()));
    return true;
}

static bool hasCurrentSelection()
{
    HWND sci = currentScintilla();
    if (!sci) return false;
    return SendMessageW(sci, SCI_GETSELECTIONSTART_LOCAL, 0, 0) !=
           SendMessageW(sci, SCI_GETSELECTIONEND_LOCAL, 0, 0);
}

static std::wstring getEntireText()
{
    HWND sci = currentScintilla();
    if (!sci) return L"";
    const LRESULT length = SendMessageW(sci, SCI_GETTEXTLENGTH_LOCAL, 0, 0);
    if (length <= 0) return L"";
    std::string buffer(static_cast<size_t>(length) + 1, '\0');
    SendMessageW(sci, SCI_GETTEXT_LOCAL, static_cast<WPARAM>(buffer.size()), reinterpret_cast<LPARAM>(buffer.data()));
    return utf8ToWide(buffer.c_str());
}

static bool replaceEntireText(const std::wstring& text)
{
    HWND sci = currentScintilla();
    if (!sci) return false;
    const std::string utf8 = debugWideToUtf8(text);
    const LRESULT length = SendMessageW(sci, SCI_GETTEXTLENGTH_LOCAL, 0, 0);
    SendMessageW(sci, SCI_SETSEL_LOCAL, 0, static_cast<LPARAM>(length));
    SendMessageW(sci, SCI_REPLACESEL_LOCAL, 0, reinterpret_cast<LPARAM>(utf8.c_str()));
    return true;
}

static bool handleTextProtocolLine(const std::string& line)
{
    auto splitPayload = [&](const std::string& prefix, std::string& payload) -> bool {
        if (line.rfind(prefix, 0) != 0) return false;
        payload = line.substr(prefix.size());
        return true;
    };

    std::string payload;
    if (splitPayload("\x1eNPPNODE_GETLINE:", payload)) {
        const std::string response = "OK:" + encodeBase64(debugWideToUtf8(getCurrentLineText())) + "\n";
        writeProtocolResponse(response);
        return true;
    }
    if (splitPayload("\x1eNPPNODE_SETLINE:", payload)) {
        bool ok = false;
        const std::string decoded = decodeBase64(payload, ok);
        const bool result = ok && replaceCurrentLine(utf8ToWide(decoded.c_str()));
        writeProtocolResponse(result ? "OK\n" : "ERROR\n");
        return true;
    }
    if (splitPayload("\x1eNPPNODE_GETSELECTION:", payload)) {
        const std::string response = "OK:" + encodeBase64(debugWideToUtf8(getSelectionText())) + "\n";
        writeProtocolResponse(response);
        return true;
    }
    if (splitPayload("\x1eNPPNODE_SETSELECTION:", payload)) {
        bool ok = false;
        const std::string decoded = decodeBase64(payload, ok);
        const bool result = ok && replaceSelectionText(utf8ToWide(decoded.c_str()));
        writeProtocolResponse(result ? "OK\n" : "ERROR\n");
        return true;
    }
    if (splitPayload("\x1eNPPNODE_HASSELECTION:", payload)) {
        writeProtocolResponse(hasCurrentSelection() ? "OK:1\n" : "OK:0\n");
        return true;
    }
    if (splitPayload("\x1eNPPNODE_GETTEXT:", payload)) {
        const std::string response = "OK:" + encodeBase64(debugWideToUtf8(getEntireText())) + "\n";
        writeProtocolResponse(response);
        return true;
    }
    if (splitPayload("\x1eNPPNODE_SETTEXT:", payload)) {
        bool ok = false;
        const std::string decoded = decodeBase64(payload, ok);
        const bool result = ok && replaceEntireText(utf8ToWide(decoded.c_str()));
        writeProtocolResponse(result ? "OK\n" : "ERROR\n");
        return true;
    }
    return false;
}

static bool handleGetCursorProtocolLine(const std::string& line)
{
    static const std::string prefix = "\x1eNPPNODE_GETCURSOR:";
    if (line != prefix) return false;

    const auto [lineNumber, columnNumber] = getCurrentCursor();
    const std::string response = "OK:" + std::to_string(lineNumber) + "," +
                                 std::to_string(columnNumber) + "\n";
    std::lock_guard<std::mutex> lock(g_stdinMutex);
    if (g_stdinWrite) {
        DWORD written = 0;
        const BOOL ok = WriteFile(g_stdinWrite, response.data(),
                                  static_cast<DWORD>(response.size()), &written, nullptr);
        FlushFileBuffers(g_stdinWrite);
        debugLog(L"protocol getCursor response sent line=" + std::to_wstring(lineNumber) +
                 L" column=" + std::to_wstring(columnNumber) +
                 L" WriteFile=" + std::to_wstring(ok) +
                 L" bytes=" + std::to_wstring(written));
    }
    return true;
}

static bool handleProtocolLine(const std::string& line)
{
    static const std::string prefix = "\x1eNPPNODE_ALERT:";
    if (line.rfind(prefix, 0) != 0) return false;

    const std::string payload = line.substr(prefix.size());
    const size_t separator = payload.find('|');
    if (separator == std::string::npos) return false;

    bool messageOk = false, titleOk = false;
    const std::string decodedMessage = decodeBase64(payload.substr(0, separator), messageOk);
    const std::string decodedTitle = decodeBase64(payload.substr(separator + 1), titleOk);
    if (!messageOk || !titleOk) return false;

    const std::wstring message = utf8ToWide(decodedMessage.c_str());
    const std::wstring title = utf8ToWide(decodedTitle.c_str());
    debugLog(L"protocol alert received message=" + message + L" title=" + title);
    const int result = MessageBoxW(g_nppData._nppHandle, message.c_str(),
                                   title.c_str(), MB_OK | MB_ICONINFORMATION);

    std::lock_guard<std::mutex> lock(g_stdinMutex);
    if (g_stdinWrite) {
        const char response[] = "OK\n";
        DWORD written = 0;
        const BOOL ok = WriteFile(g_stdinWrite, response, static_cast<DWORD>(sizeof(response) - 1), &written, nullptr);
        FlushFileBuffers(g_stdinWrite);

        // Keep stdin open so the same Node.js process can issue another alert.
        // The stdin pipe is closed only after the Node.js process has exited.
        debugLog(L"protocol alert response sent result=" + std::to_wstring(result) +
                 L" WriteFile=" + std::to_wstring(ok) +
                 L" bytes=" + std::to_wstring(written) + L" stdin kept open");
    }
    return true;
}

static bool dispatchProtocolLineToUi(const std::string& line)
{
    if (!g_hotkeyWnd || g_shuttingDown.load()) return false;

    auto request = std::make_shared<ProtocolDispatchRequest>();
    request->line = line;
    auto* holder = new std::shared_ptr<ProtocolDispatchRequest>(request);
    if (!PostMessageW(g_hotkeyWnd, WM_NPPNODE_PROTOCOL, 0, reinterpret_cast<LPARAM>(holder))) {
        delete holder;
        return false;
    }

    std::unique_lock<std::mutex> lock(request->mutex);
    while (!request->done && !g_shuttingDown.load()) {
        request->cv.wait_for(lock, std::chrono::milliseconds(50));
    }
    if (g_shuttingDown.load() && !request->done) return false;
    return request->handled;
}

static void readPipe(HANDLE pipe, const std::shared_ptr<std::atomic<bool>>& hadOutput)
{
    debugLog(L"readPipe start pipe=" + hexPtr(pipe));
    Utf8StreamDecoder decoder;
    std::string protocolPending;
    char buffer[8192];
    DWORD read = 0;

    auto emitNormal = [&](const std::wstring& text) {
        if (text.empty()) return;
        hadOutput->store(true);
        g_outputMessagesPending.fetch_add(1);
        const BOOL posted = g_hotkeyWnd ? PostMessageW(g_hotkeyWnd, WM_NPPNODE_OUTPUT,
                                        0, reinterpret_cast<LPARAM>(new std::wstring(text))) : FALSE;
        if (!posted) g_outputMessagesPending.fetch_sub(1);
        debugLog(L"readPipe emit wchar=" + std::to_wstring(text.size()) + L" PostMessage=" + std::to_wstring(posted));
    };

    auto processWide = [&](const std::wstring& text) {
        if (text.empty()) return;
        const std::string utf8 = debugWideToUtf8(text);
        protocolPending.append(utf8);
        size_t pos = 0;
        for (;;) {
            const size_t nl = protocolPending.find('\n', pos);
            if (nl == std::string::npos) break;
            std::string line = protocolPending.substr(pos, nl - pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const bool protocolCandidate =
                line.rfind("\x1eNPPNODE_", 0) == 0;
            if (protocolCandidate) {
                if (!dispatchProtocolLineToUi(line)) {
                    // During shutdown the Node process is terminated and no
                    // protocol response is needed. Otherwise show malformed
                    // or unknown protocol lines as normal output.
                    if (!g_shuttingDown.load())
                        emitNormal(utf8ToWide(line.c_str()) + L"\n");
                }
            } else {
                emitNormal(utf8ToWide(line.c_str()) + L"\n");
            }
            pos = nl + 1;
        }
        if (pos != 0) protocolPending.erase(0, pos);
    };

    for (;;) {
        if (!ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr) || read == 0) break;
        decoder.feed(buffer, read, processWide);
    }
    decoder.finish(processWide);
    if (!protocolPending.empty()) {
        emitNormal(utf8ToWide(protocolPending.c_str()));
    }
    CloseHandle(pipe);
    debugLog(L"readPipe end");
}

static std::wstring getCurrentFilePath()
{
    if (!g_nppData._nppHandle) return L"";

    const UINT_PTR bufferId = static_cast<UINT_PTR>(SendMessageW(
        g_nppData._nppHandle, NPPM_GETCURRENTBUFFERID_LOCAL, 0, 0));
    if (!bufferId) return L"";

    const LRESULT length = SendMessageW(
        g_nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID_LOCAL,
        bufferId, 0);
    if (length <= 0) return L"";

    std::wstring path(static_cast<size_t>(length) + 1, L'\0');
    const LRESULT copied = SendMessageW(
        g_nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID_LOCAL,
        bufferId, reinterpret_cast<LPARAM>(path.data()));
    if (copied < 0) return L"";
    path.resize(static_cast<size_t>(copied));
    return path;
}

static std::pair<long, long> getCurrentCursor()
{
    HWND sci = currentScintilla();
    if (!sci) return {0, 0};

    const LRESULT position = SendMessageW(sci, SCI_GETCURRENTPOS_LOCAL, 0, 0);
    const LRESULT line = SendMessageW(sci, SCI_LINEFROMPOSITION_LOCAL,
                                      static_cast<WPARAM>(position), 0);
    const LRESULT column = SendMessageW(sci, SCI_GETCOLUMN_LOCAL,
                                        static_cast<WPARAM>(position), 0);
    return {static_cast<long>(line), static_cast<long>(column)};
}

static std::string encodeBase64(const std::string& input)
{
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);
    for (size_t i = 0; i < input.size(); i += 3) {
        const unsigned int a = static_cast<unsigned char>(input[i]);
        const bool hasB = i + 1 < input.size();
        const bool hasC = i + 2 < input.size();
        const unsigned int b = hasB ? static_cast<unsigned char>(input[i + 1]) : 0;
        const unsigned int c = hasC ? static_cast<unsigned char>(input[i + 2]) : 0;
        const unsigned int triple = (a << 16) | (b << 8) | c;
        output.push_back(alphabet[(triple >> 18) & 0x3F]);
        output.push_back(alphabet[(triple >> 12) & 0x3F]);
        output.push_back(hasB ? alphabet[(triple >> 6) & 0x3F] : '=');
        output.push_back(hasC ? alphabet[triple & 0x3F] : '=');
    }
    return output;
}

static std::wstring quoteCommandArg(const std::wstring& arg)
{
    if (arg.empty()) return L"\"\"";
    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') { ++backslashes; continue; }
        if (c == L'\"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'\"');
            backslashes = 0;
            continue;
        }
        out.append(backslashes, L'\\');
        backslashes = 0;
        out.push_back(c);
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'\"');
    return out;
}

static std::vector<wchar_t> makeEnvironmentBlock(const std::wstring& menuTitle)
{
    std::vector<std::wstring> entries;
    LPWCH env = GetEnvironmentStringsW();
    if (env) {
        for (LPWCH p = env; *p; p += std::wcslen(p) + 1) {
            const std::wstring entry(p);
            if (_wcsnicmp(entry.c_str(), L"NPPNODE_MENU_TITLE=", 19) == 0) continue;
            entries.push_back(entry);
        }
        FreeEnvironmentStringsW(env);
    }
    entries.push_back(L"NPPNODE_MENU_TITLE=" + menuTitle);
    std::sort(entries.begin(), entries.end(), [](const std::wstring& a, const std::wstring& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });

    size_t total = 1;
    for (const auto& entry : entries) total += entry.size() + 1;
    std::vector<wchar_t> block(total, L'\0');
    wchar_t* out = block.data();
    for (const auto& entry : entries) {
        std::copy(entry.begin(), entry.end(), out);
        out += entry.size();
        *out++ = L'\0';
    }
    *out = L'\0';
    return block;
}

static void runNodeScript(const std::wstring& path, const std::wstring& menuTitle)
{
    debugLog(L"runNodeScript path=" + path);
    std::lock_guard<std::mutex> lock(g_processMutex);
    if (g_processRunning.load()) {
        MessageBoxW(g_nppData._nppHandle, L"A Node.js script is already running.",
                    L"NppNodeJS", MB_OK | MB_ICONWARNING);
        return;
    }
    g_outputShownForRun = false;
    g_outputRunningTitle = false;
    g_outputFinishPending = false;
    g_outputMessagesPending.store(0);
    if (g_outputPane) {
        clearOutputPane();
        applyOutputStyle();
    }
    g_processRunning.store(true);

    if (g_processThread.joinable()) g_processThread.join();
    g_processThread = std::thread([path, menuTitle]() {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE inRead = nullptr, inWrite = nullptr;
        HANDLE outRead = nullptr, outWrite = nullptr;
        HANDLE errRead = nullptr, errWrite = nullptr;
        if (!CreatePipe(&inRead, &inWrite, &sa, 0) ||
            !CreatePipe(&outRead, &outWrite, &sa, 0) ||
            !CreatePipe(&errRead, &errWrite, &sa, 0)) {
            if (inRead) CloseHandle(inRead); if (inWrite) CloseHandle(inWrite);
            if (outRead) CloseHandle(outRead); if (outWrite) CloseHandle(outWrite);
            if (errRead) CloseHandle(errRead); if (errWrite) CloseHandle(errWrite);
            if (g_hotkeyWnd) PostMessageW(g_hotkeyWnd, WM_NPPNODE_OUTPUT, 0,
                reinterpret_cast<LPARAM>(new std::wstring(L"[NppNodeJS] Unable to create stdout/stderr pipes.\r\n")));
            g_processRunning.store(false);
            return;
        }
        SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);

        const std::string menuTitleUtf8 = debugWideToUtf8(menuTitle);
        const std::string menuTitleBase64 = encodeBase64(menuTitleUtf8);
        std::wstring command = L"node.exe " + quoteCommandArg(path) +
            L" --nppnode-menu-title-b64 " + quoteCommandArg(utf8ToWide(menuTitleBase64.c_str()));
        std::vector<wchar_t> environment = makeEnvironmentBlock(menuTitle);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = inRead;
        si.hStdOutput = outWrite;
        si.hStdError = errWrite;
        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> cmd(command.begin(), command.end());
        cmd.push_back(L'\0');

        debugLog(L"CreateProcess command=" + command);
        BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                 CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                                 environment.data(), nullptr, &si, &pi);
        CloseHandle(inRead);
        CloseHandle(outWrite);
        CloseHandle(errWrite);
        if (!ok) {
            const DWORD err = GetLastError();
            debugLog(L"CreateProcess FAILED error=" + std::to_wstring(err));
            std::wstring msg = L"[NppNodeJS] Unable to start node.exe. Error code: " + std::to_wstring(err) + L"\r\n";
            if (g_hotkeyWnd) PostMessageW(g_hotkeyWnd, WM_NPPNODE_OUTPUT, 0,
                reinterpret_cast<LPARAM>(new std::wstring(std::move(msg))));
            CloseHandle(inWrite);
            CloseHandle(outRead); CloseHandle(errRead);
            g_processRunning.store(false);
            return;
        }

        debugLog(L"CreateProcess OK process=" + hexPtr(pi.hProcess));
        {
            std::lock_guard<std::mutex> lock(g_processHandleMutex);
            g_processHandle = pi.hProcess;
        }
        {
            std::lock_guard<std::mutex> lock(g_stdinMutex);
            g_stdinWrite = inWrite;
        }
        auto hadOutput = std::make_shared<std::atomic<bool>>(false);
        std::thread outThread(readPipe, outRead, hadOutput);
        std::thread errThread(readPipe, errRead, hadOutput);
        WaitForSingleObject(pi.hProcess, INFINITE);
        outThread.join();
        errThread.join();
        {
            std::lock_guard<std::mutex> lock(g_stdinMutex);
            if (g_stdinWrite) {
                CloseHandle(g_stdinWrite);
                g_stdinWrite = nullptr;
            }
        }
        {
            std::lock_guard<std::mutex> lock(g_processHandleMutex);
            if (g_processHandle == pi.hProcess) g_processHandle = nullptr;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        debugLog(L"process finished hadOutput=" + std::to_wstring(hadOutput->load()));
        g_processRunning.store(false);
        if (g_hotkeyWnd)
            PostMessageW(g_hotkeyWnd, WM_NPPNODE_RUNSTATE, 0, 0);
    });
}

static bool createScriptTemplate(const std::wstring& scriptPath)
{
    const std::wstring parent = directoryName(scriptPath);
    if (!parent.empty() && parent != L".") {
        const DWORD attrs = GetFileAttributesW(parent.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            const int result = SHCreateDirectoryExW(g_nppData._nppHandle, parent.c_str(), nullptr);
            if (result != ERROR_SUCCESS && result != ERROR_FILE_EXISTS && result != ERROR_ALREADY_EXISTS) {
                return false;
            }
        }
    }

    std::ofstream f(scriptPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;

    const std::wstring lower = [&]() {
        std::wstring value = scriptPath;
        std::transform(value.begin(), value.end(), value.begin(), ::towlower);
        return value;
    }();
    const bool isMjs = lower.size() >= 4 && lower.rfind(L".mjs") == lower.size() - 4;
    const std::string text = isMjs
        ? "// NppNodeJS API:\n// https://github.com/seantw/NppNodeJS\n\nimport npp from '#menu-helper';\n\nconsole.log('Hello, World!');\n"
        : "// NppNodeJS API:\n// https://github.com/seantw/NppNodeJS\n\nconst npp = require('#menu-helper');\n\nconsole.log('Hello, World!');\n";
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    return f.good();
}

static bool openOrCreateScript(const std::wstring& scriptPath)
{
    const DWORD attrs = GetFileAttributesW(scriptPath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        SendMessageW(g_nppData._nppHandle, NPPM_DOOPEN, 0,
                     reinterpret_cast<LPARAM>(scriptPath.c_str()));
        return true;
    }

    const std::wstring fileName = scriptPath.substr(scriptPath.find_last_of(L"\\/") + 1);
    const std::wstring message = fileName + L" 不存在，是否建立？";
    if (MessageBoxW(g_nppData._nppHandle, message.c_str(),
                    L"NppNodeJS", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return false;
    }

    if (!createScriptTemplate(scriptPath)) {
        MessageBoxW(g_nppData._nppHandle,
                    L"Unable to create the script file.",
                    L"NppNodeJS", MB_OK | MB_ICONERROR);
        return false;
    }

    SendMessageW(g_nppData._nppHandle, NPPM_DOOPEN, 0,
                 reinterpret_cast<LPARAM>(scriptPath.c_str()));
    return true;
}

static void showScriptPath(int id)
{
    const int index = id - g_cmdBase;
    if (index < 0 || index >= static_cast<int>(g_scriptPaths.size())) return;

    const std::wstring& scriptPath = g_scriptPaths[static_cast<size_t>(index)];

    // Hidden shortcut: Ctrl+click a script menu item opens the script in Notepad++
    // instead of executing it.
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        openOrCreateScript(scriptPath);
        return;
    }

    const DWORD attrs = GetFileAttributesW(scriptPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        if (!openOrCreateScript(scriptPath)) return;
        return;
    }

    runNodeScript(scriptPath, g_scriptTitles[static_cast<size_t>(index)]);
}

static LRESULT CALLBACK hotkeyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_HOTKEY) {
        const int id = static_cast<int>(wParam);
        if (id >= g_cmdBase && id < g_cmdBase + g_cmdCount) {
            showScriptPath(id);
            return 0;
        }
    } else if (msg == WM_NPPNODE_PROTOCOL) {
        auto* holder = reinterpret_cast<std::shared_ptr<ProtocolDispatchRequest>*>(lParam);
        if (holder) {
            std::shared_ptr<ProtocolDispatchRequest> request = *holder;
            delete holder;
            bool handled = handleProtocolLine(request->line);
            if (!handled) handled = handlePromptProtocolLine(request->line);
            if (!handled) handled = handleConfirmProtocolLine(request->line);
            if (!handled) handled = handleGetFileProtocolLine(request->line);
            if (!handled) handled = handleGetCursorProtocolLine(request->line);
            if (!handled) handled = handleTextProtocolLine(request->line);
            {
                std::lock_guard<std::mutex> lock(request->mutex);
                request->handled = handled;
                request->done = true;
            }
            request->cv.notify_one();
        }
        return 0;
    } else if (msg == WM_NPPNODE_OUTPUT) {
        std::unique_ptr<std::wstring> text(reinterpret_cast<std::wstring*>(lParam));
        debugLog(L"WM_NPPNODE_OUTPUT received text=" + std::to_wstring(text ? text->size() : 0) +
                 L" pane=" + hexPtr(g_outputPane) + L" edit=" + hexPtr(g_outputEdit));
        if (text && !text->empty()) {
            if (!g_outputPane && !createOutputPane()) {
                g_outputMessagesPending.fetch_sub(1);
                debugLog(L"createOutputPane FAILED from output message");
                return 0;
            }
            appendOutput(*text);
            if (!g_outputShownForRun) {
                showOutputPane();
                g_outputShownForRun = true;
            }
            if (!g_outputRunningTitle)
                updateOutputPaneTitle(true);
        }
        g_outputMessagesPending.fetch_sub(1);
        if (g_outputFinishPending && g_outputMessagesPending.load() == 0) {
            g_outputFinishPending = false;
            if (g_outputRunningTitle)
                updateOutputPaneTitle(false);
        }
        return 0;
    } else if (msg == WM_NPPNODE_RUNSTATE) {
        // The process and both pipe reader threads are finished. Output
        // messages already queued for the UI are drained before restoring
        // the original dock title.
        if (g_outputMessagesPending.load() != 0) {
            g_outputFinishPending = true;
        } else if (g_outputRunningTitle) {
            updateOutputPaneTitle(false);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool addMenuItem(HMENU menu, const std::wstring& title, const std::wstring& path)
{
    const int index = static_cast<int>(g_scriptPaths.size());
    const int id = g_cmdBase + index;
    g_scriptPaths.push_back(path);
    const size_t tab = title.find(L'\t');
    g_scriptTitles.push_back(tab == std::wstring::npos ? title : title.substr(0, tab));
    g_cmdIds.push_back(id);
    if (!AppendMenuW(menu, MF_STRING, static_cast<UINT_PTR>(id), title.c_str())) {
        g_scriptPaths.pop_back(); g_scriptTitles.pop_back(); g_cmdIds.pop_back(); return false;
    }
    registerHotkeyForItem(title, id);
    return true;
}

static bool addJsonMenu(HMENU menu, cJSON* obj, const std::wstring& folder)
{
    if (!cJSON_IsObject(obj)) return false;
    for (cJSON* item = obj->child; item; item = item->next) {
        if (!item->string) continue;
        const std::wstring title = utf8ToWide(item->string);
        if (cJSON_IsString(item)) {
            std::wstring file = utf8ToWide(item->valuestring);
            std::wstring lower = file;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            const bool validExt = lower.size() >= 3 &&
                (lower.rfind(L".js") == lower.size() - 3 ||
                 (lower.size() >= 4 && lower.rfind(L".mjs") == lower.size() - 4));
            if (!validExt) continue;
            addMenuItem(menu, title, joinPath(folder, file));
        } else if (cJSON_IsObject(item)) {
            HMENU sub = CreatePopupMenu();
            if (!sub) return false;
            if (!addJsonMenu(sub, item, folder)) { DestroyMenu(sub); return false; }
            if (!AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), title.c_str())) {
                DestroyMenu(sub); return false;
            }
        }
    }
    return true;
}

static void removeMenus()
{
    unregisterHotkeys();
    if (!g_mainMenu) return;
    const int count = GetMenuItemCount(g_mainMenu);
    for (int i = count - 1; i >= 0; --i) {
        MENUITEMINFOW mi{};
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_SUBMENU;
        if (GetMenuItemInfoW(g_mainMenu, static_cast<UINT>(i), TRUE, &mi)) {
            for (HMENU h : g_topMenus) {
                if (mi.hSubMenu == h) { RemoveMenu(g_mainMenu, static_cast<UINT>(i), MF_BYPOSITION); break; }
            }
        }
    }
    for (HMENU h : g_topMenus) DestroyMenu(h);
    DrawMenuBar(g_nppData._nppHandle);
    g_topMenus.clear(); g_mainMenu = nullptr;
    g_scriptPaths.clear(); g_scriptTitles.clear(); g_cmdIds.clear(); g_cmdBase = 0; g_cmdCount = 0; g_installed = false;
    destroyHotkeyWindow();
}

static bool installMenus(const std::wstring& jsonPath, bool forceRebuild)
{
    if (g_installed && !forceRebuild) return true;
    const std::wstring jsonText = readUtf8File(jsonPath);
    if (jsonText.empty()) {
        MessageBoxW(g_nppData._nppHandle, jsonPath.c_str(), L"NppNodeJS - menu.json not found or empty", MB_OK | MB_ICONERROR);
        return false;
    }
    std::ifstream f(jsonPath.c_str(), std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) bytes.erase(0, 3);
    cJSON* root = cJSON_Parse(bytes.c_str());
    if (!root) {
        const char* err = cJSON_GetErrorPtr();
        std::wstring msg = L"JSON parse error: "; msg += utf8ToWide(err ? err : "unknown error");
        MessageBoxW(g_nppData._nppHandle, msg.c_str(), L"NppNodeJS", MB_OK | MB_ICONERROR); return false;
    }
    cJSON* scriptFolderItem = cJSON_GetObjectItemCaseSensitive(root, "script_folder");
    cJSON* outputTitleItem = cJSON_GetObjectItemCaseSensitive(root, "output_pan_title");
    cJSON* debugItem = cJSON_GetObjectItemCaseSensitive(root, "debug");
    cJSON* menuItem = cJSON_GetObjectItemCaseSensitive(root, "menu");
    if (!cJSON_IsString(scriptFolderItem) || !cJSON_IsString(outputTitleItem) ||
        !cJSON_IsObject(menuItem) || (debugItem && !cJSON_IsBool(debugItem))) {
        cJSON_Delete(root);
        MessageBoxW(g_nppData._nppHandle, L"menu.json must contain string 'script_folder', string 'output_pan_title', and object 'menu'.", L"NppNodeJS", MB_OK | MB_ICONERROR);
        return false;
    }
    // Validate the new configuration completely before removing the current menu.
    // This keeps the existing menu available when Rebuild Menu encounters invalid JSON.
    int itemCount = 0;
    std::vector<cJSON*> stack{menuItem};
    while (!stack.empty()) {
        cJSON* obj = stack.back(); stack.pop_back();
        for (cJSON* x = obj->child; x; x = x->next) {
            if (cJSON_IsString(x)) ++itemCount;
            else if (cJSON_IsObject(x)) stack.push_back(x);
        }
    }
    if (forceRebuild && g_installed) {
        removeMenus();
    }
    if (itemCount > 0) {
        if (!SendMessageW(g_nppData._nppHandle, NPPM_ALLOCATECMDID, static_cast<WPARAM>(itemCount), reinterpret_cast<LPARAM>(&g_cmdBase))) {
            cJSON_Delete(root);
            MessageBoxW(g_nppData._nppHandle, L"Notepad++ could not allocate menu command IDs.", L"NppNodeJS", MB_OK | MB_ICONERROR); return false;
        }
        g_cmdCount = itemCount;
    }
    if (!createHotkeyWindow()) { cJSON_Delete(root); MessageBoxW(g_nppData._nppHandle, L"Cannot create hotkey window.", L"NppNodeJS", MB_OK | MB_ICONERROR); return false; }
    g_outputTitle = utf8ToWide(outputTitleItem->valuestring);
    if (g_outputTitle.empty()) g_outputTitle = L"NppNodeJS Output";
    g_mainMenu = reinterpret_cast<HMENU>(SendMessageW(g_nppData._nppHandle, NPPM_GETMENUHANDLE, NPPMAINMENU, 0));
    if (!g_mainMenu) { cJSON_Delete(root); MessageBoxW(g_nppData._nppHandle, L"Cannot get Notepad++ main menu.", L"NppNodeJS", MB_OK | MB_ICONERROR); return false; }
    const std::wstring configuredFolder = utf8ToWide(scriptFolderItem->valuestring);
    const std::wstring folder = resolveScriptFolder(jsonPath, configuredFolder);
    g_menuJsonPath = fullPath(jsonPath);
    g_scriptFolder = folder;
    // An explicit menu.json boolean overrides the build-time default.
    // When omitted, NPPNODEJS_DEBUG_DEFAULT determines whether logging is enabled.
    g_debugEnabled = debugItem ? cJSON_IsTrue(debugItem) : (NPPNODEJS_DEBUG_DEFAULT != 0);
    g_debugLogPath = g_debugEnabled ? joinPath(g_scriptFolder, L"NppNodeJS-debug.log") : L"";
    debugLog(L"=== NppNodeJS debug session ===");
    debugLog(L"menu.json=" + jsonPath);
    debugLog(L"script_folder=" + g_scriptFolder);
    debugLog(L"output_pan_title=" + g_outputTitle);
    for (cJSON* top = menuItem->child; top; top = top->next) {
        if (!top->string || !cJSON_IsObject(top)) continue;
        HMENU sub = CreatePopupMenu();
        if (!sub || !addJsonMenu(sub, top, folder)) {
            if (sub) DestroyMenu(sub); cJSON_Delete(root); removeMenus(); return false;
        }
        const std::wstring title = utf8ToWide(top->string);
        if (!AppendMenuW(g_mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), title.c_str())) {
            DestroyMenu(sub); cJSON_Delete(root); removeMenus(); return false;
        }
        g_topMenus.push_back(sub);
    }
    DrawMenuBar(g_nppData._nppHandle);
    g_installed = true;
    debugLog(L"installMenus complete scriptCount=" + std::to_wstring(g_scriptPaths.size()));
    cJSON_Delete(root);
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { g_hModule = hModule; DisableThreadLibraryCalls(hModule); }
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData nppData)
{
    g_nppData = nppData;
    installMenus(loadConfiguredMenuPath());
}

extern "C" __declspec(dllexport) const wchar_t* getName() { return PLUGIN_NAME; }

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF)
{
    *nbF = static_cast<int>(sizeof(g_funcItems) / sizeof(g_funcItems[0]));
    return g_funcItems;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notifyCode)
{
    if (notifyCode && notifyCode->nmhdr.code == NPPN_SHUTDOWN) {
        g_shuttingDown.store(true);
        {
            std::lock_guard<std::mutex> lock(g_processHandleMutex);
            if (g_processHandle) {
                TerminateProcess(g_processHandle, 1);
            }
        }
        if (g_processThread.joinable()) {
            g_processThread.join();
        }
        removeMenus();
        destroyOutputPane();
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM)
{
    if (Message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id >= g_cmdBase && id < g_cmdBase + g_cmdCount) { showScriptPath(id); return TRUE; }
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }

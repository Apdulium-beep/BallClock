// launcher.cpp — BallClock Controller launcher (GUI)
// Портативный, без внешних зависимостей (только WinAPI)
// Требуется флаг компилятора /utf-8 (см. свойства проекта)

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Gdi32.lib")

// ============================================================
//  ID для контролов
// ============================================================
#define ID_BTN_QT       101
#define ID_BTN_WEB      102
#define ID_BTN_SETTINGS 103
#define ID_BTN_LOGS     104
#define ID_BTN_EXIT     105
#define ID_STATUS       106
#define ID_BTN_GEN      107

// ============================================================
//  Глобальные переменные
// ============================================================
static HINSTANCE g_hInst = nullptr;
static HWND      g_hMain = nullptr;
static HWND      g_hStatus = nullptr;
static HFONT     g_hFontTitle = nullptr;
static HFONT     g_hFontNormal = nullptr;
static HFONT     g_hFontMono = nullptr;

// ============================================================
//  Утилиты работы с путями и файлами
// ============================================================

std::wstring GetExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    PathRemoveFileSpecW(buf);
    return std::wstring(buf);
}

std::wstring Join(const std::wstring& a, const std::wstring& b) {
    std::wstring r = a;
    if (!r.empty() && r.back() != L'\\') r += L'\\';
    r += b;
    return r;
}

bool FileExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring ReadConfig(const std::wstring& path, const std::wstring& key,
    const std::wstring& defaultValue = L"") {
    std::wifstream f(path);
    if (!f.is_open()) return defaultValue;
    std::wstring line;
    std::wstring needle = L"\"" + key + L"\"";
    while (std::getline(f, line)) {
        size_t pos = line.find(needle);
        if (pos == std::wstring::npos) continue;
        size_t colon = line.find(L':', pos);
        if (colon == std::wstring::npos) continue;
        size_t q1 = line.find(L'"', colon + 1);
        if (q1 == std::wstring::npos) continue;
        size_t q2 = line.find(L'"', q1 + 1);
        if (q2 == std::wstring::npos) continue;
        return line.substr(q1 + 1, q2 - q1 - 1);
    }
    return defaultValue;
}

void WriteLog(const std::wstring& baseDir, const std::wstring& msg) {
    std::wstring logDir = Join(baseDir, L"logs");
    CreateDirectoryW(logDir.c_str(), NULL);
    std::wstring logFile = Join(logDir, L"launcher.log");
    std::wofstream f(logFile, std::ios::app);
    if (!f.is_open()) return;

    time_t t = time(nullptr);
    struct tm tmv;
    localtime_s(&tmv, &t);
    wchar_t buf[64];
    wcsftime(buf, 64, L"%Y-%m-%d %H:%M:%S", &tmv);
    f << L"[" << buf << L"] " << msg << L"\n";
}

// ============================================================
//  Обновление статусной строки
// ============================================================
void SetStatus(const std::wstring& text) {
    if (g_hStatus) {
        SetWindowTextW(g_hStatus, text.c_str());
    }
}

// ============================================================
//  Запуск процессов
// ============================================================
bool RunProcess(const std::wstring& exe, const std::wstring& workingDir,
    const std::wstring& args = L"") {
    std::wstring cmd = L"\"";
    cmd += exe;
    cmd += L"\"";
    if (!args.empty()) {
        cmd += L" ";
        cmd += args;
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(
        NULL, cmdBuf.data(), NULL, NULL, FALSE,
        0, NULL,
        workingDir.empty() ? NULL : workingDir.c_str(),
        &si, &pi);

    if (!ok) return false;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

void OpenUrl(const std::wstring& url) {
    ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void OpenFile(const std::wstring& path) {
    ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

bool InstallVCRedist(const std::wstring& baseDir) {
    std::wstring exe = Join(baseDir, L"vc_redist.x64.exe");
    if (!FileExists(exe)) return false;

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = exe.c_str();
    sei.lpParameters = L"/install /quiet /norestart";
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExW(&sei)) return false;
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 120000);
        CloseHandle(sei.hProcess);
    }
    return true;
}

// ============================================================
//  Действия кнопок
// ============================================================

void ActionQt() {
    std::wstring baseDir = GetExeDir();
    std::wstring qtDir = Join(baseDir, L"qt_app");
    std::wstring exe = Join(qtDir, L"BallClockController.exe");

    if (!FileExists(exe)) {
        std::wstring errMsg = L"Не найден файл:\n";
        errMsg += exe;
        errMsg += L"\n\nСоберите Qt-проект или проверьте структуру папок.";
        MessageBoxW(g_hMain, errMsg.c_str(), L"Ошибка",
            MB_ICONERROR | MB_OK);
        SetStatus(L"[!] Qt-приложение не найдено");
        WriteLog(baseDir, L"Qt exe not found: " + exe);
        return;
    }

    SetStatus(L"[i] Запуск Qt-приложения...");
    if (RunProcess(exe, qtDir)) {
        SetStatus(L"[OK] Qt-приложение запущено");
        WriteLog(baseDir, L"Qt app launched: " + exe);
    }
    else {
        SetStatus(L"[X] Не удалось запустить Qt");
        WriteLog(baseDir, L"Qt launch failed: " + exe);
    }
}

void ActionWeb() {
    std::wstring baseDir = GetExeDir();
    std::wstring cfg = Join(baseDir, L"config.json");
    std::wstring ip = ReadConfig(cfg, L"clock_ip", L"192.168.4.1");
    if (ip.empty()) ip = L"192.168.4.1";
    std::wstring url = ip;
    if (url.rfind(L"http://", 0) != 0 && url.rfind(L"https://", 0) != 0)
        url = L"http://" + url;

    std::wstring openMsg = L"[i] Открываю ";
    openMsg += url;
    SetStatus(openMsg);
    auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(g_hMain, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL));
    if (result <= 32) {
        MessageBoxW(g_hMain, L"Не удалось открыть браузер по умолчанию.", L"Ошибка", MB_ICONERROR);
        return;
    }
    WriteLog(baseDir, L"Web opened: " + url);
}

void ActionGenerator() {
    std::wstring path = Join(GetExeDir(), L"web\\BallClockGen.html");
    if (!FileExists(path)) {
        MessageBoxW(g_hMain, L"Не найден web\\BallClockGen.html", L"Ошибка", MB_ICONERROR);
        return;
    }
    OpenFile(path);
    SetStatus(L"Генератор открыт в браузере");
}

void ActionSettings() {
    std::wstring baseDir = GetExeDir();
    std::wstring cfg = Join(baseDir, L"config.json");
    if (!FileExists(cfg)) {
        std::wofstream f(cfg);
        f << L"{\n";
        f << L"    \"clock_ip\": \"192.168.4.1\"\n";
        f << L"}\n";
    }
    wchar_t windowsDir[MAX_PATH] = {};
    GetWindowsDirectoryW(windowsDir, MAX_PATH);
    std::wstring notepad = Join(windowsDir, L"notepad.exe");
    RunProcess(notepad, baseDir, L"\"" + cfg + L"\"");
    SetStatus(L"[i] Открыт config.json");
}

void ActionLogs() {
    std::wstring baseDir = GetExeDir();
    std::wstring logs = Join(baseDir, L"logs");
    CreateDirectoryW(logs.c_str(), NULL);
    OpenFile(logs);
    SetStatus(L"[i] Открыта папка логов");
}

// ============================================================
//  Создание шрифтов
// ============================================================
void CreateFonts() {
    g_hFontTitle = CreateFontW(
        -24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    g_hFontNormal = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    g_hFontMono = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
}

// ============================================================
//  Создание контролов
// ============================================================
HWND CreateButton(HWND parent, const wchar_t* text, int id,
    int x, int y, int w, int h) {
    HWND btn = CreateWindowExW(
        0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        x, y, w, h,
        parent, (HMENU)(INT_PTR)id, g_hInst, nullptr);
    SendMessageW(btn, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    return btn;
}

void CreateControls(HWND hWnd) {
    int margin = 25;
    int btnW = 280;
    int btnH = 40;
    int y = 90;

    CreateButton(hWnd, L"Запустить Qt-приложение", ID_BTN_QT,
        margin, y, btnW, btnH);
    y += btnH + 10;

    CreateButton(hWnd, L"Веб-панель часов (по IP)", ID_BTN_WEB,
        margin, y, btnW, btnH);
    y += btnH + 10;

    CreateButton(hWnd, L"Генератор BallClock (локально)", ID_BTN_GEN,
        margin, y, btnW, btnH);
    y += btnH + 10;

    CreateButton(hWnd, L"Настройки (config.json)", ID_BTN_SETTINGS,
        margin, y, btnW, btnH);
    y += btnH + 10;

    CreateButton(hWnd, L"Папка с логами", ID_BTN_LOGS,
        margin, y, btnW, btnH);

    CreateButton(hWnd, L"Выход", ID_BTN_EXIT,
        margin + btnW - 100, 370, 100, 32);

    g_hStatus = CreateWindowExW(
        0, L"STATIC", L"Готов к работе",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SUNKEN,
        margin, 420, btnW, 24,
        hWnd, (HMENU)(INT_PTR)ID_STATUS, g_hInst, nullptr);
    SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
}

// ============================================================
//  Обработчик сообщений окна
// ============================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hWnd);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case ID_BTN_QT:       ActionQt();           break;
        case ID_BTN_WEB:      ActionWeb();          break;
        case ID_BTN_GEN:      ActionGenerator();    break;
        case ID_BTN_SETTINGS: ActionSettings();     break;
        case ID_BTN_LOGS:     ActionLogs();         break;
        case ID_BTN_EXIT:     DestroyWindow(hWnd);  break;
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        HFONT old = (HFONT)SelectObject(hdc, g_hFontTitle);
        SetTextColor(hdc, RGB(40, 40, 40));
        TextOutW(hdc, 25, 25, L"BallClock Controller", lstrlenW(L"BallClock Controller"));

        SelectObject(hdc, g_hFontNormal);
        SetTextColor(hdc, RGB(110, 110, 110));
        TextOutW(hdc, 25, 58, L"Управление светодиодными часами", lstrlenW(L"Управление светодиодными часами"));

        SelectObject(hdc, old);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================
//  Точка входа
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInst = hInstance;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BallClockLauncherWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Ошибка регистрации окна",
            L"Ошибка", MB_ICONERROR);
        return 1;
    }

    CreateFonts();

    int winW = 350;
    int winH = 500;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    g_hMain = CreateWindowExW(
        0,
        L"BallClockLauncherWnd",
        L"BallClock Controller",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, winW, winH,
        NULL, NULL, hInstance, NULL);

    if (!g_hMain) {
        MessageBoxW(NULL, L"Ошибка создания окна",
            L"Ошибка", MB_ICONERROR);
        return 1;
    }

    std::wstring baseDir = GetExeDir();
    WriteLog(baseDir, L"Launcher started (GUI)");

    ShowWindow(g_hMain, nCmdShow);
    UpdateWindow(g_hMain);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(g_hMain, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (g_hFontTitle)  DeleteObject(g_hFontTitle);
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontMono)   DeleteObject(g_hFontMono);

    WriteLog(baseDir, L"Launcher exit");
    return 0;
}

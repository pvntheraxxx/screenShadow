#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#define WM_TRAYICON (WM_USER + 1)
#define MUTEX_NAME L"Local\\screenShadowMutex"

#include <windows.h>
#include "MouseHook.h"
#include "Overlay.h"
#include "resource.h"

// DPI-awareness: поддержка корректных координат на мониторах с разным масштабированием.
static BOOL EnableDPIAwareness() {
    typedef BOOL (WINAPI *SetProcessDpiAwarenessContextProc)(HANDLE);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        SetProcessDpiAwarenessContextProc pSetProcessDpiAwarenessContext =
            (SetProcessDpiAwarenessContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pSetProcessDpiAwarenessContext) {
            if (pSetProcessDpiAwarenessContext((HANDLE)-4)) { // -4 соответствует PER_MONITOR_AWARE_V2
                return TRUE;
            }
        }
        typedef BOOL (WINAPI *SetProcessDPIAwareProc)(void);
        SetProcessDPIAwareProc pSetProcessDPIAware =
            (SetProcessDPIAwareProc)GetProcAddress(hUser32, "SetProcessDPIAware");
        if (pSetProcessDPIAware) {
            return pSetProcessDPIAware();
        }
    }
    return FALSE;
}

// Добавляет иконку в системный трей с контекстным меню.
NOTIFYICONDATA nid = { 0 };
HMENU hTrayMenu = nullptr;
HWND hWndTrayStub = nullptr;

void AddTrayIcon(HINSTANCE hInstance) {
    hTrayMenu = CreatePopupMenu();
    AppendMenuW(hTrayMenu, MF_STRING, 1001, L"Выход");

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWndTrayStub;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = reinterpret_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR));
    wcscpy(nid.szTip, L"screenShadow");

    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
    if (hTrayMenu) {
        DestroyMenu(hTrayMenu);
        hTrayMenu = nullptr;
    }
}

LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hTrayMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == 1001) {
                PostQuitMessage(0);
            }
            break;
        case WM_DESTROY:
            RemoveTrayIcon();
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Предотвращаем запуск нескольких экземпляров.
    HANDLE hMutex = CreateMutex(nullptr, TRUE, MUTEX_NAME);
    if (hMutex == nullptr) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    EnableDPIAwareness();

    // Регистрация служебного окна для системного трея.
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DummyTrayClass";
    RegisterClassW(&wc);

    hWndTrayStub = CreateWindowW(L"DummyTrayClass", L"", WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                 nullptr, nullptr, hInstance, nullptr);

    AddTrayIcon(hInstance);
    InstallMouseHook(hInstance);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    RemoveMouseHook();
    CloseHandle(hMutex);
    return 0;
}

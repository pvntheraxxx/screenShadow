#include <windows.h>
#include <cwchar>
#include <algorithm>
#include "Overlay.h"

HWND overlayWindow = nullptr;
POINT startPoint;
POINT currentPoint;
bool selecting = false;
RECT monitorBounds = { 0 };

static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            FillRect(hdc, &clientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));

            // Преобразуем глобальные координаты в локальные относительно границ монитора.
            POINT localStart = { startPoint.x - monitorBounds.left, startPoint.y - monitorBounds.top };
            POINT localCurrent = { currentPoint.x - monitorBounds.left, currentPoint.y - monitorBounds.top };

            RECT rect;
            rect.left   = std::min(localStart.x, localCurrent.x);
            rect.top    = std::min(localStart.y, localCurrent.y);
            rect.right  = std::max(localStart.x, localCurrent.x);
            rect.bottom = std::max(localStart.y, localCurrent.y);

            // Заливка выделенной области – красный оттенок (стиль "Шэдоу").
            HBRUSH fillBrush = CreateSolidBrush(RGB(128, 0, 32));
            FillRect(hdc, &rect, fillBrush);
            DeleteObject(fillBrush);

            // Рисуем тёмный (чёрный) контур вокруг выделенной области.
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            SelectObject(hdc, pen);
            SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
            DeleteObject(pen);

            // Выводим размеры выделенной области.
            int width = abs(localCurrent.x - localStart.x);
            int height = abs(localCurrent.y - localStart.y);
            wchar_t sizeText[64];
            _snwprintf(sizeText, 64, L"%d x %d", width, height);

            RECT textRect = { rect.left + 5, rect.top - 25, rect.left + 125, rect.top };
            FillRect(hdc, &textRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SelectObject(hdc, font);
            TextOutW(hdc, textRect.left, textRect.top, sizeText, wcslen(sizeText));

            EndPaint(hwnd, &ps);
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void CreateOverlayWindow(HINSTANCE hInstance, POINT anchor) {
    HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(monitor, &mi);
    monitorBounds = mi.rcMonitor;

    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = OverlayProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"OverlayClass";
        RegisterClassW(&wc);
        classRegistered = true;
    }

    overlayWindow = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
        L"OverlayClass", L"",
        WS_POPUP,
        monitorBounds.left,
        monitorBounds.top,
        monitorBounds.right - monitorBounds.left,
        monitorBounds.bottom - monitorBounds.top,
        nullptr, nullptr, hInstance, nullptr
    );

    SetLayeredWindowAttributes(overlayWindow, 0, (255 * 40) / 100, LWA_ALPHA);
    ShowWindow(overlayWindow, SW_SHOW);
}

void DestroyOverlayWindow() {
    if (overlayWindow) {
        DestroyWindow(overlayWindow);
        overlayWindow = nullptr;
    }
}

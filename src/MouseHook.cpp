#include "MouseHook.h"
#include "Overlay.h"
#include "Screenshot.h" // Убедитесь, что здесь подключён правильный заголовок
#include <windows.h>
#include <algorithm>

HHOOK mouseHook = nullptr;
bool rbuttonHeld = false;
bool movedWhileHolding = false;

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        switch (wParam) {
            case WM_RBUTTONDOWN: {
                rbuttonHeld = true;
                movedWhileHolding = false;
                break;
            }

            case WM_MOUSEMOVE: {
                if (rbuttonHeld) {
                    if (!selecting && (ms->pt.x != currentPoint.x || ms->pt.y != currentPoint.y)) {
                        // Получаем монитор до присвоения startPoint
                        HMONITOR monitor = MonitorFromPoint(ms->pt, MONITOR_DEFAULTTONEAREST);
                        MONITORINFO mi = { sizeof(mi) };
                        GetMonitorInfo(monitor, &mi);
                        monitorBounds = mi.rcMonitor;

                        startPoint = ms->pt;

                        selecting = true;
                        CreateOverlayWindow(GetModuleHandle(nullptr), startPoint);
                    }

                    movedWhileHolding = true;
                    currentPoint = ms->pt;
                    InvalidateRect(overlayWindow, nullptr, TRUE);
                }
                break;
            }

            case WM_RBUTTONUP: {
                rbuttonHeld = false;

                if (selecting && movedWhileHolding) {
                    selecting = false;
                    DestroyOverlayWindow();

                    RECT selection;
                    selection.left   = std::min(startPoint.x, currentPoint.x);
                    selection.top    = std::min(startPoint.y, currentPoint.y);
                    selection.right  = std::max(startPoint.x, currentPoint.x);
                    selection.bottom = std::max(startPoint.y, currentPoint.y);

                    IntersectRect(&selection, &selection, &monitorBounds);

                    if ((selection.right - selection.left) >= 5 &&
                        (selection.bottom - selection.top) >= 5) {
                        // Используем новую функцию, которая устанавливает данные в буфер обмена с учетом контекста.
                        CaptureAndPrepareClipboard(selection);
                    }
                } else if (selecting) {
                    selecting = false;
                    DestroyOverlayWindow();
                }
                break;
            }
        }
    }

    return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}

void InstallMouseHook(HINSTANCE hInstance) {
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, nullptr, 0);
    if (!mouseHook) {
        MessageBoxW(nullptr, L"Не удалось установить хук мыши!", L"Ошибка", MB_ICONERROR);
        ExitProcess(1);
    }
}

void RemoveMouseHook() {
    if (mouseHook) {
        UnhookWindowsHookEx(mouseHook);
        mouseHook = nullptr;
    }
}

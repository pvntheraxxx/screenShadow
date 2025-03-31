#include "Screenshot.h"
#include <windows.h>
#include <gdiplus.h>
#include <cwchar>
#include <cstdlib>
#include <shellapi.h>

#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#ifndef DROPFILES
typedef struct _DROPFILES {
    DWORD pFiles;  // Смещение от начала структуры до списка файлов
    POINT pt;      // Точка (не используется здесь)
    BOOL fNC;      // TRUE, если координаты без рамок окна
    BOOL fWide;    // TRUE, если имена файлов в формате Unicode (WCHAR)
} DROPFILES, *LPDROPFILES;
#endif

#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

// Глобальные переменные для отложенной установки данных в буфер обмена.
static HBITMAP g_screenshotBitmap = nullptr;
static WCHAR g_screenshotFile[MAX_PATH] = {0};
static HHOOK g_contextHook = nullptr;

// Вспомогательная функция для получения CLSID энкодера по MIME-типу.
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;  // Нет доступных энкодеров

    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == nullptr)
        return -1;

    GetImageEncoders(num, size, pImageCodecInfo);
    int ret = -1;
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            ret = j;
            break;
        }
    }
    free(pImageCodecInfo);
    return ret;
}

// Функция, которая устанавливает данные в буфер обмена в зависимости от контекста.
void SetClipboardForContext(bool isExplorer) {
    if (g_screenshotBitmap == nullptr || g_screenshotFile[0] == L'\0')
        return; // Нет данных для установки

    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        // Всегда устанавливаем CF_BITMAP
        SetClipboardData(CF_BITMAP, g_screenshotBitmap);

        // Если целевое окно — проводник, дополнительно устанавливаем CF_HDROP для файла.
        if (isExplorer) {
            size_t fileNameLen = wcslen(g_screenshotFile) + 1; // с нулевым терминатором
            size_t dropFilesSize = sizeof(DROPFILES) + (fileNameLen * sizeof(WCHAR));
            HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, dropFilesSize);
            if (hGlobal) {
                DROPFILES* dropFiles = (DROPFILES*)GlobalLock(hGlobal);
                dropFiles->pFiles = sizeof(DROPFILES); // смещение до списка файлов
                dropFiles->pt.x = 0;
                dropFiles->pt.y = 0;
                dropFiles->fNC = FALSE;
                dropFiles->fWide = TRUE;
                wcscpy((WCHAR*)((BYTE*)dropFiles + sizeof(DROPFILES)), g_screenshotFile);
                GlobalUnlock(hGlobal);
                SetClipboardData(CF_HDROP, hGlobal);
            }
        }
        CloseClipboard();
    }
    // После установки удаляем временный хук и очищаем глобальные данные.
    if (g_contextHook) {
        UnhookWindowsHookEx(g_contextHook);
        g_contextHook = nullptr;
    }
    // Графический объект теперь передан в буфер обмена, поэтому не удаляем его.
    g_screenshotBitmap = nullptr;
    g_screenshotFile[0] = L'\0';
}

// Временный low-level mouse hook для определения контекста на основе клика.
LRESULT CALLBACK ContextMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_LBUTTONUP) {
        // При отпускании левой кнопки мыши получаем активное окно.
        HWND fg = GetForegroundWindow();
        WCHAR className[256] = {0};
        GetClassNameW(fg, className, 256);
        // Если активное окно проводника (Explorer) — его классы могут быть "CabinetWClass" или "ExploreWClass".
        bool isExplorer = (wcscmp(className, L"CabinetWClass") == 0 || wcscmp(className, L"ExploreWClass") == 0);
        // Устанавливаем буфер обмена в зависимости от контекста.
        SetClipboardForContext(isExplorer);
    }
    return CallNextHookEx(g_contextHook, nCode, wParam, lParam);
}

// Захватывает изображение экрана в пределах заданного прямоугольника,
// сохраняет его как PNG во временной папке, но не устанавливает данные в буфер сразу.
// Вместо этого данные сохраняются в глобальных переменных, а установка буфера происходит по следующему клику.
void CaptureAndPrepareClipboard(RECT rect) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width < 5 || height < 5)
        return;

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(screenDC, width, height);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBitmap);

    BitBlt(memDC, 0, 0, width, height, screenDC, rect.left, rect.top, SRCCOPY);

    // Инициализация GDI+ для сохранения в PNG.
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    Bitmap* bitmap = new Bitmap(hBitmap, nullptr);

    WCHAR tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    WCHAR fileName[MAX_PATH];
    GetTempFileNameW(tempPath, L"SCN", 0, fileName);
    // Меняем расширение на .png.
    WCHAR* dot = wcsrchr(fileName, L'.');
    if (dot)
        wcscpy(dot, L".png");

    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) == -1) {
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return;
    }

    Status status = bitmap->Save(fileName, &pngClsid, nullptr);
    // Можно добавить проверку status при необходимости.

    delete bitmap;
    GdiplusShutdown(gdiplusToken);

    // Возвращаем оригинальный объект в DC.
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    // Сохраняем полученные данные в глобальных переменных.
    g_screenshotBitmap = hBitmap;
    wcscpy(g_screenshotFile, fileName);

    // Устанавливаем временный mouse hook, чтобы по следующему клику определить контекст.
    g_contextHook = SetWindowsHookExW(WH_MOUSE_LL, ContextMouseProc, nullptr, 0);
}

// Основная функция захвата, ранее называлась CaptureAndCopyToClipboard,
// теперь называется CaptureAndPrepareClipboard, и вызывается из кода выделения.

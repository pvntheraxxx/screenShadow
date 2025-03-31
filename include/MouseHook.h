#pragma once
#include <windows.h>

// Устанавливает глобальный хук мыши для перехвата событий WM_RBUTTONDOWN, WM_MOUSEMOVE и WM_RBUTTONUP.
// Параметр hInstance — экземпляр модуля, который может потребоваться для создания оверлейного окна.
void InstallMouseHook(HINSTANCE hInstance);

// Удаляет ранее установленный глобальный хук мыши.
void RemoveMouseHook();

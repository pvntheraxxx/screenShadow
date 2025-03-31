#pragma once
#include <windows.h>

// Объявление новой функции, которая захватывает изображение и устанавливает данные в буфер обмена с учетом контекста.
void CaptureAndPrepareClipboard(RECT rect);

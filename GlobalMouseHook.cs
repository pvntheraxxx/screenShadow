using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace screenShadow
{
    public class GlobalMouseHook 
    {
        private static DateTime lastRightClickTime = DateTime.MinValue;
        private static readonly TimeSpan doubleClickThreshold = TimeSpan.FromMilliseconds(300); // можно настроить

        private const int WH_MOUSE_LL = 14;
        private const int WM_RBUTTONDOWN = 0x0204;
        private const int WM_RBUTTONUP = 0x0205;
        private const int WM_MOUSEMOVE = 0x0200;

        private LowLevelMouseProc _proc;
        private IntPtr _hookID = IntPtr.Zero;

        public event Action OnRightMouseDown;
        public event Action OnRightMouseUp;
        public event Action<int, int> OnMouseMove;

        public GlobalMouseHook()
        {
            _proc = HookCallback;
        }

        public void Hook()
        { 
            _hookID = SetHook(_proc);
        }

        public void Unhook()
        {
            UnhookWindowsHookEx(_hookID);
        }

        private IntPtr SetHook(LowLevelMouseProc proc)
        {
            using (Process curProcess = Process.GetCurrentProcess())
            using (ProcessModule curModule = curProcess.MainModule)
            {
                return SetWindowsHookEx(WH_MOUSE_LL, proc, GetModuleHandle(curModule.ModuleName), 0);
            }
        }
        private delegate IntPtr LowLevelMouseProc(int nCode, IntPtr wParam, IntPtr lParam);

        private IntPtr HookCallback(int nCode, IntPtr wParam, IntPtr lParam)
        {
            if (nCode >= 0)
            {
                MSLLHOOKSTRUCT hookStruct = Marshal.PtrToStructure<MSLLHOOKSTRUCT>(lParam);
                int x = hookStruct.pt.x;
                int y = hookStruct.pt.y;

                switch ((int)wParam)
                {
                    case WM_RBUTTONDOWN:
                        OnRightMouseDown?.Invoke();
                        break;
                    case WM_MOUSEMOVE:
                        OnMouseMove?.Invoke(x, y);
                        break;
                    case WM_RBUTTONUP:
                        OnRightMouseUp?.Invoke();
                        break;
                }
            }
            if ((int)wParam == WM_RBUTTONDOWN)
            {
                DateTime now = DateTime.Now;
                if (now - lastRightClickTime < doubleClickThreshold)
                {
                    // Это двойной клик — пропускаем, разрешаем Windows показать меню
                    lastRightClickTime = DateTime.MinValue;
                    return CallNextHookEx(_hookID, nCode, wParam, lParam);
                }
                else
                {
                    // Это одиночный клик — начинаем выделение
                    lastRightClickTime = now;
                    return (IntPtr)1; // блокируем контекстное меню
                }
            }
            return CallNextHookEx(_hookID, nCode, wParam, lParam);

        }
        #region WinAPI

        [StructLayout(LayoutKind.Sequential)]
        private struct POINT
        {
            public int x;
            public int y;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct MSLLHOOKSTRUCT
        {
            public POINT pt;
            public uint mouseData;
            public uint flags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        [DllImport("user32.dll", SetLastError = true)]
        private static extern IntPtr SetWindowsHookEx(int idHook,
            LowLevelMouseProc lpfn, IntPtr hMod, uint dwThreadId);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool UnhookWindowsHookEx(IntPtr hhk);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern IntPtr CallNextHookEx(IntPtr hhk,
            int nCode, IntPtr wParam, IntPtr lParam);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr GetModuleHandle(string lpModuleName);

        #endregion
    }
}
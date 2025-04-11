using System;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace screenShadow
{
    public class TrayIcon : ApplicationContext
    {
        private NotifyIcon trayIcon;

        private GlobalMouseHook mouseHook;
        private bool isSelecting = false;
        private ScreenOverlay overlay;

        private bool hasMoved = false;
        private Point dragStart;

        public TrayIcon()
        {
            // Меню трея
            ContextMenuStrip trayMenu = new ContextMenuStrip();
            trayMenu.Items.Add("Выход", null, OnExit);

            // Иконка в трее
            trayIcon = new NotifyIcon()
            {
                Icon = new Icon(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "icon.ico")),
                ContextMenuStrip = trayMenu,
                Text = "screenShadow работает",
                Visible = true
            };

            // Подключаем хук мыши 
            mouseHook = new GlobalMouseHook();
            mouseHook.OnRightMouseDown += HandleMouseDown;
            mouseHook.OnRightMouseUp += HandleMouseUp;
            mouseHook.OnMouseMove += HandleMouseMove;
            mouseHook.Hook();

            overlay = new ScreenOverlay();
            overlay.OnSelectionComplete += HandleSelectionComplete;

            Application.ApplicationExit += (s, e) =>
            {
                CleanUpTempFolder();
            };

            // Очистка старых PNG при запуске
            CleanOldFiles();
        }
        
        private void HandleMouseDown()
        {
            isSelecting = true;
            hasMoved = false;
            dragStart = Cursor.Position;
        }

        private void HandleMouseMove(int x, int y)
        {
            if (isSelecting)
            {
                if (!hasMoved)
                {
                    if (Math.Abs(x - dragStart.X) > 2 || Math.Abs(y - dragStart.Y) > 2)
                    {
                        hasMoved = true;
                        overlay.StartSelection(dragStart);
                    }
                }
                else
                {
                    overlay.UpdateSelection(new Point(x, y));
                }
            }
        }

        private void HandleMouseUp()
        {
            if (isSelecting)
            {
                isSelecting = false;
                if (hasMoved)
                {
                    overlay.EndSelection();
                }
            }
        }

        private void HandleSelectionComplete(Rectangle rect)
        {
            try
            {
                using (Bitmap bmp = new Bitmap(rect.Width, rect.Height))
                {
                    using (Graphics g = Graphics.FromImage(bmp))
                    {
                        g.CopyFromScreen(rect.Location, Point.Empty, rect.Size);
                    }

                    string folder = Path.Combine(Path.GetTempPath(), "screenShadow");
                    Directory.CreateDirectory(folder);

                    string fileName;
                    string fullPath;

                    // Цикл генерации уникальных имён
                    do
                    {
                        Thread.Sleep(1); // Гарантирует другую миллисекунду
                        string timestamp = DateTime.Now.ToString("yyyyMMdd_HHmmss_fff");
                        fileName = $"screenshot_{timestamp}.png";
                        fullPath = Path.Combine(folder, fileName);
                    } while (File.Exists(fullPath));

                    bmp.Save(fullPath, System.Drawing.Imaging.ImageFormat.Png);

                    // Сброс и создание нового буфера
                    Clipboard.Clear();

                    var files = new System.Collections.Specialized.StringCollection();
                    files.Add(fullPath);

                    DataObject data = new DataObject();
                    data.SetImage(bmp);
                    data.SetFileDropList(files);

                    Clipboard.SetDataObject(data, true);
                    Console.WriteLine("PNG скопирован в буфер: " + fileName);
                }

            } catch (Exception ex)
            {
                MessageBox.Show("Ошибка: " + ex.Message);
            }
        }

        private void CleanUpTempFolder()
        {
            try
            {
                string folder = Path.Combine(Path.GetTempPath(), "screenShadow");
                if (Directory.Exists(folder))
                {
                    Directory.Delete(folder, true); // удаляет ВСЮ папку рекурсивно
                    Console.WriteLine("🧹 Временная папка удалена.");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Ошибка при удалении временной папки: " + ex.Message);
            }
        }

        private void CleanOldFiles()
        {
            try
            {
                string folder = Path.Combine(Path.GetTempPath(), "screenShadow");
                if (Directory.Exists(folder))
                {
                    foreach (var file in Directory.GetFiles(folder, "*.png"))
                    {
                        try
                        {
                            var age = DateTime.Now - File.GetLastWriteTime(file);
                            if (age.TotalMinutes > 10)
                                File.Delete(file);
                        }
                        catch
                        {
                            // Игнорируем ошибки (например, файл занят)
                        }
                    }

                    Console.WriteLine("🧹 Старые PNG-файлы очищены.");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Ошибка при очистке старых файлов: " + ex.Message);
            }
        }

        private void OnExit(object sender, EventArgs e)
        {
            if (trayIcon != null)
            {
                trayIcon.Visible = false;
                trayIcon.Dispose(); // 💡 удаляет из трея мгновенно
            }

            Application.Exit();
        }

    }

}

using System;
using System.Drawing;
using System.Windows.Forms;

namespace screenShadow
{
    public class ScreenOverlay : Form
    {
        private Point startPoint;
        private Point currentPoint;
        private bool isDrawing = false;

        public event Action<Rectangle> OnSelectionComplete;


        public ScreenOverlay()
        {
            this.FormBorderStyle = FormBorderStyle.None;
            this.StartPosition = FormStartPosition.Manual;
            this.ShowInTaskbar = false;
            this.TopMost = true;
            this.DoubleBuffered = true;
            this.Opacity = 0.25;
            this.BackColor = Color.Black;
            this.Enabled = false;

            Rectangle bounds = GetVirtualScreenBounds();
            this.Bounds = bounds;
        }

        private Rectangle GetVirtualScreenBounds()
        {
            int minX = int.MaxValue, minY = int.MaxValue;
            int maxX = int.MinValue, maxY = int.MinValue;

            foreach (var screen in Screen.AllScreens)
            {
                if (screen.Bounds.Left < minX) minX = screen.Bounds.Left;
                if (screen.Bounds.Top < minY) minY = screen.Bounds.Top;
                if (screen.Bounds.Right > maxX) maxX = screen.Bounds.Right;
                if (screen.Bounds.Bottom > maxY) maxY = screen.Bounds.Bottom;
            }

            return new Rectangle(minX, minY, maxX - minX, maxY - minY);
        }

        public void StartSelection(Point start)
        {
            // Переводим глобальные координаты курсора → в координаты формы
            startPoint = new Point(start.X - this.Bounds.X, start.Y - this.Bounds.Y);
            currentPoint = startPoint;
            isDrawing = true;
            Show();
        }


        public void UpdateSelection(Point current)
        {
            // Текущая позиция мыши → тоже переводим относительно формы
            currentPoint = new Point(current.X - this.Bounds.X, current.Y - this.Bounds.Y);
            Invalidate(); // Перерисовываем прямоугольник
        }


        public void EndSelection()
        {
            isDrawing = false;

            // Возвращаем в глобальные координаты перед передачей наружу
            Rectangle globalRect = GetRectangle(
                new Point(startPoint.X + this.Bounds.X, startPoint.Y + this.Bounds.Y),
                new Point(currentPoint.X + this.Bounds.X, currentPoint.Y + this.Bounds.Y)
            );

            Hide();
            OnSelectionComplete?.Invoke(globalRect);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            if (isDrawing)
            {
                Rectangle rect = GetRectangle(startPoint, currentPoint);

                // 1. Затемняем всё, кроме выделения
                Region darkRegion = new Region();
                foreach (var screen in Screen.AllScreens)
                {
                    darkRegion.Union(screen.Bounds);
                }
                darkRegion.Exclude(rect);

                using (Brush dimBrush = new SolidBrush(Color.FromArgb(180, 13, 13, 13))) // полупрозрачный тёмный фон
                {
                    e.Graphics.FillRegion(dimBrush, darkRegion);
                }

                // 2. Белая рамка
                using (Pen border = new Pen(Color.White, 2))
                {
                    e.Graphics.DrawRectangle(border, rect);
                }

                // 3. Размер области
                string size = $"{rect.Width} x {rect.Height}";
                using (Font font = new Font("Segoe UI", 10, FontStyle.Regular))
                {
                    int labelX = Math.Max(rect.Left, 2);
                    int labelY = Math.Max(rect.Top - 30, 2);

                    SizeF textSize = e.Graphics.MeasureString(size, font);
                    RectangleF bgRect = new RectangleF(labelX - 4, labelY - 2, textSize.Width + 8, textSize.Height + 4);

                    using (Brush bg = new SolidBrush(Color.FromArgb(200, 13, 13, 13))) // фон текста остаётся тёмным
                    {
                        e.Graphics.FillRectangle(bg, bgRect);
                    }

                    using (Brush brush = new SolidBrush(Color.White))
                    {
                        e.Graphics.DrawString(size, font, brush, labelX, labelY);
                    }
                }
            }
        }

        private Rectangle GetRectangle(Point p1, Point p2)
        {
            int x = Math.Min(p1.X, p2.X);
            int y = Math.Min(p1.Y, p2.Y);
            int width = Math.Max(1, Math.Abs(p1.X - p2.X));
            int height = Math.Max(1, Math.Abs(p1.Y - p2.Y));

            return new Rectangle(x, y, width, height);
        }

    }
}
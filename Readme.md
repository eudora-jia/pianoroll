# Pianoroll for Windows

A flexible, featured pianoroll control, using Direct2D to paint.

To use:

1. Instantiate PR::PIANOROLL object
2. Pass WM_KEYDOWN, WM_LBUTTONDBLCLK, WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MOUSEMOVE,WM_LBUTTONUP, WM_SYSKEYDOWN to the control
3. When you want to paint it, call member "Paint()" passing a Direct2D object and the RECT


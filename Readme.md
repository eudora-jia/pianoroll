# Pianoroll for Windows

A flexible, featured pianoroll control, using Direct2D to paint.
![Pianoroll](https://raw.githubusercontent.com/WindowsNT/pianoroll/master/1.jpg)

## To use

1. Instantiate PR::PIANOROLL object
2. Pass WM_KEYDOWN, WM_LBUTTONDBLCLK, WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MOUSEMOVE,WM_LBUTTONUP, WM_SYSKEYDOWN to the control
3. When you want to paint it, call member "Paint()" passing a Direct2D object and the RECT

## Features

* Notes support channel (0-15), velocity (0-127) , layer (unlimited)
* Moving, Resizing with snap controls
* Supports diatonic movement through specified Key and Mode (Major/Minor)
* Unlimited undo/redo
* Unlimited layers
* Piano side (Left/Right) 
* Callbacks
* Tools - Auto, Eraser, Single click entry
* Serialization/Deserialization to XML
* Key per measure
* Time signature per measure
* MIDI export (working, not 100% completed yet)

## Callbacks
Your callbacks are notified when:
* Note is changed or inserted or removed
* Piano note is tapped
* Redraw requested

## Keyboard shortcuts
* A : auto tool
* E : eraser tool
* I : single entry tool
* 1,2,3,4 (up row) : Select beat duration for next note
* Shift+1,2,3,4 (up row) : Beat duration 1/8, 1/16, 1/32,  1/64
* < and > : Change selected items velocity
* Ctrl+ < and > : Velocity off/full
* +/- : Change selected items position diatonically
* Shift +/- : Change selected items position chromatically
* Numpad +/-/* : Zoom in,out,all
* Shift + Arrows up/down : Change channel
* Alt + Arrows up/down : Change layer
* / and \ : Enlarge/reduce notes
* J : Join notes
* Ctrl+1...6 : Snapping resolution
* Alt+1-9 Numpad : Toggle layers
* 1-9 Numpad : Next layer
* Ctrl+A : Select all
* Ctrl+C : Copy
* Ctrl+V : Paste to last clicked measure
* Ctrl+Z : Undo
* Ctrl+Y : Redo
* Right/Left arrow : move roll 
* Del : Delete selected notes

## Mouse
* Dblclick : insert note (auto tool) 
* Dblclick on note : remove note
* Click on note : select/unselect
* Drag outside note: select (insert in single entry tool, delete in eraser tool)
* Drag/Resize note

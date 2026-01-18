# waydraw
Overlay drawing in wayland.

## Building
Dependencies:
 - wayland-scanner
 - wayland-client
 - xkbcommon
 - cairo

```
$ meson setup build
$ meson compile -C build
```

## Shortcuts
 - tab/shift-tab - cycle through color palette
 - b - select brush tool
 - c - select circle tool
 - r - select rectangle tool
 - ctrl-z/ctrl-Z - undo/redo
 - ctrl-x/ctrl-X - earlier/later
 - q - quit


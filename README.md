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
 - t/T - cycle through available tools (there is currently no visual indicator on which tool you have chosen)
 - ctrl-z/ctrl-Z - undo/redo
 - ctrl-x/ctrl-X - earlier/later
 - q - quit


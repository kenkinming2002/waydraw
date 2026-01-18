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
 - h - "hibernate" but the surface is still visible
 - H - "hibernate" and the surface is no longer visible
 - q - quit

## Hibernate
Hibernation refer to a state in which the program is still running but can no
longer receive pointer and keyboard inputs. Instead, all pointer and keyboard
inputs will pass-through to the window at the back. To wakeup waydraw, simply
re-launch another instance.

# whiteboard

A terminal-based infinite canvas whiteboard with Vim-style modal editing, built with ncurses and C++17.

## Dependencies

- CMake >= 3.15
- A C++17 compiler (clang++ or g++)
- ncurses

On macOS: `brew install ncurses`
On Debian/Ubuntu: `sudo apt install libncurses-dev`

## Build

```sh
mkdir -p build && cd build
cmake ..
make
```

## Usage

```sh
./whiteboard [filename]
```

If a filename is given, the canvas is loaded from it (or created fresh if it doesn't exist). All saves write back to that file.

The canvas is an infinite 2D sparse grid. Move freely, place characters, draw boxes, and annotate — all without fixed bounds.

---

## Modes

| Mode | Enter via |
|------|-----------|
| Normal | `Esc` |
| Insert | `i`, `a`, `o`, `O` |
| Visual | `v` |
| Box | `b` |
| Command | `:` |
| Search | `/`, `?` |

---

## Normal Mode

**Movement**

| Key | Action |
|-----|--------|
| `h` / `←` | Move cursor left |
| `l` / `→` | Move cursor right |
| `k` / `↑` | Move cursor up |
| `j` / `↓` | Move cursor down |
| `H` / `J` / `K` / `L` | Pan viewport (detach from cursor) |
| `zz` | Re-center viewport on cursor |
| `Ctrl+d` / `Ctrl+u` | Half-page scroll down / up |
| `gg` / `G` | Jump to top / bottom of content |
| `0` / `$` | Jump to column 0 / end of current row |

**Editing**

| Key | Action |
|-----|--------|
| `i` | Enter Insert at cursor |
| `a` | Enter Insert after cursor |
| `o` | Enter Insert on line below |
| `O` | Enter Insert on line above |
| `x` | Delete character under cursor |
| `dd` | Delete entire current row |
| `yy` | Yank (copy) entire current row |
| `p` | Paste clipboard |
| `u` | Undo |
| `Ctrl+r` | Redo |

**Other**

| Key | Action |
|-----|--------|
| `v` | Enter Visual mode |
| `b` | Enter Box drawing mode |
| `:` | Enter Command mode |
| `/` | Search forward |
| `?` | Search backward |
| `n` / `N` | Next / previous search match |
| `m` | Toggle minimap overlay |
| `q` | Quit |
| `Ctrl+C` | Force quit |

---

## Insert Mode

| Key | Action |
|-----|--------|
| `Esc` | Return to Normal mode |
| Arrow keys | Move cursor |
| `Backspace` | Delete character to the left |
| `Enter` | Move to start of next line |
| Any printable char | Place character on canvas and advance cursor |

---

## Visual Mode

Select a region, then yank or delete it.

| Key | Action |
|-----|--------|
| `Esc` | Return to Normal mode |
| `h` `j` `k` `l` / arrows | Extend selection |
| `y` | Yank selection to clipboard |
| `d` | Delete selection |

---

## Box Drawing Mode

Draw box-drawing characters. Corners, tees, and crossings are joined automatically.

| Key | Action |
|-----|--------|
| `Esc` | Return to Normal mode |
| `h` `j` `k` `l` / arrows | Draw in that direction |

---

## Command Mode

Entered with `:`. Type a command and press `Enter`.

| Command | Action |
|---------|--------|
| `:w [filename]` | Save canvas (to optional filename) |
| `:e <filename>` | Load canvas from file |
| `:wq` | Save and quit |
| `:q` / `:q!` | Quit |
| `:goto x y` | Teleport cursor to coordinates `(x, y)` |
| `:clear` | Clear the entire canvas |
| `:color [name\|num]` | Show or set active color |

Colors: `default`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white` (or `0`–`7`).
The active color is applied to characters placed in Insert and Box modes.

---

## Search Mode

Entered with `/` (forward) or `?` (backward).

Type a substring pattern and press `Enter` to jump to the first match. Press `n`/`N` in Normal mode to cycle through matches. Press `Esc` to cancel.

---

## Mouse

| Action | Effect |
|--------|--------|
| Left click | Move cursor to clicked position |
| Scroll up / down | Pan viewport |

---

## File Format

One cell per line:

```
x y token color_pair
```

- `token` is the ASCII character for printable chars, or `~NAME` for box-drawing characters (e.g. `~HLINE`, `~ULCORNER`).
- `color_pair` is optional and defaults to `0`.
- Lines starting with `#` are ignored.

---

## Architecture

| File | Role |
|------|------|
| `src/main.cpp` | Entry point, event loop, signal handling |
| `src/canvas.hpp` | `Cell` struct and row-major `Canvas` type (`map<int, map<int, Cell>>`) |
| `src/modes.hpp` | `Mode` enum (NORMAL, INSERT, VISUAL, COMMAND, SEARCH, BOX) |
| `src/editor.hpp/.cpp` | Editor state machine and all mode key handlers |
| `src/renderer.hpp/.cpp` | ncurses rendering layer and minimap |
| `src/undo.hpp` | Header-only undo/redo stack (full canvas snapshots) |
| `src/serialization.hpp/.cpp` | Save/load canvas to/from file |

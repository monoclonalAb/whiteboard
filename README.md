# whiteboard

A terminal-based infinite canvas whiteboard with Vim-style keybindings, built with ncurses and C++17.

## Dependencies

- CMake >= 3.15
- A C++17 compiler (clang++ or g++)
- ncurses

On macOS: `brew install ncurses`
On Debian/Ubuntu: `sudo apt install libncurses-dev`

## Build

```sh
mkdir build && cd build
cmake ..
make
./whiteboard
```

## Usage

The canvas is an infinite 2D grid. Move around and place characters freely. The editor is modal, like Vim.

### Modes

| Mode | Enter via |
|------|-----------|
| Normal | `Esc` |
| Insert | `i`, `a`, `o`, `O` |
| Visual | `v` |
| Command | `:` |

---

### Normal Mode

**Movement**

| Key | Action |
|-----|--------|
| `h` / `←` | Move left |
| `l` / `→` | Move right |
| `k` / `↑` | Move up |
| `j` / `↓` | Move down |

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
| `p` | Paste |
| `u` | Undo |
| `Ctrl+r` | Redo |

**Other**

| Key | Action |
|-----|--------|
| `v` | Enter Visual mode |
| `:` | Enter Command mode |
| `q` | Quit |

---

### Insert Mode

| Key | Action |
|-----|--------|
| `Esc` | Return to Normal mode |
| Arrow keys | Move cursor |
| `Backspace` | Delete character to the left |
| `Enter` | Move to start of next line |
| Any printable char | Place character on canvas |

---

### Visual Mode

Select a rectangular region, then yank or delete it.

| Key | Action |
|-----|--------|
| `Esc` | Return to Normal mode |
| `h` `j` `k` `l` / arrows | Extend selection |
| `y` | Yank selection |
| `d` | Delete selection |

---

### Command Mode

Entered with `:`. Type a command and press `Enter`.

| Command | Action |
|---------|--------|
| `q` / `q!` | Quit |
| `clear` | Clear the entire canvas |
| `goto x y` | Jump cursor to coordinates `(x, y)` |

## Architecture

| File | Role |
|------|------|
| `src/main.cpp` | Entry point, main loop |
| `src/editor.hpp/.cpp` | Editor state and key handling |
| `src/renderer.hpp/.cpp` | ncurses rendering |
| `src/canvas.hpp` | Canvas type (sparse `unordered_map<(x,y), Cell>`) |
| `src/modes.hpp` | Mode enum |
| `src/undo.hpp` | Undo/redo stack |

Terminal Whiteboard — C++ TUI Whiteboard

Core Concept

A TUI application with an infinite 2D sparse canvas rendered through a fixed terminal viewport, with vim modal editing.

---
Building

  mkdir -p build && cd build && cmake .. && make
  ./whiteboard [filename]

---
Architecture

  src/
    main.cpp              - Entry point, event loop, signal handling
    canvas.hpp            - Cell struct, row-major Canvas type alias
    editor.hpp/cpp        - Editor state machine, all mode handlers
    renderer.hpp/cpp      - ncurses rendering layer, minimap
    modes.hpp             - Mode enum (NORMAL, INSERT, VISUAL, COMMAND, SEARCH, BOX)
    undo.hpp              - Undo/redo stack (header-only)
    serialization.hpp/cpp - Save/load canvas to file
  CMakeLists.txt

---
Data Structures

  Canvas — row-major sparse map, sorted by (y, x):
    struct Cell { chtype ch; short color_pair; };
    using Row    = std::map<int, Cell>;       // x → Cell
    using Canvas = std::map<int, Row>;        // y → Row

  Viewport — window into canvas:
    struct Viewport { int x, y, w, h; };

  Cursor — canvas coordinates (not screen):
    struct Cursor { int x, y; };

  Clipboard — relative-coordinate cell storage for yank/paste:
    struct Clipboard { vector<pair<pair<int,int>, Cell>> cells; bool is_line_yank; bool valid; };

---
Modes & Keybindings

  NORMAL mode:
    h/j/k/l (or arrows) - move cursor
    H/J/K/L             - pan viewport (detach from cursor)
    i                    - enter INSERT at cursor
    a                    - enter INSERT after cursor
    o                    - enter INSERT on next line
    O                    - enter INSERT on previous line
    v                    - enter VISUAL mode
    b                    - enter BOX drawing mode
    :                    - enter COMMAND mode
    /                    - search forward
    ?                    - search backward
    n/N                  - next/previous search match
    x                    - delete char at cursor
    dd                   - delete entire row
    yy                   - yank entire row
    p                    - paste clipboard
    u                    - undo
    Ctrl+r               - redo
    zz                   - center viewport on cursor
    Ctrl+d / Ctrl+u      - half-page scroll down/up
    gg / G               - jump to top/bottom of content
    0 / $                - jump to column 0 / end of row content
    m                    - toggle minimap
    q                    - quit
    Ctrl+C               - force quit

  INSERT mode:
    Esc                  - back to NORMAL
    Arrows               - move cursor
    Backspace            - delete char behind cursor
    Enter                - move to next line, column 0
    Any printable char   - write to canvas, advance cursor

  VISUAL mode:
    h/j/k/l (or arrows) - extend selection
    y                    - yank selection to clipboard
    d                    - delete selection
    Esc                  - cancel, back to NORMAL

  BOX mode:
    h/j/k/l (or arrows) - draw box-drawing characters (auto-joins corners/tees)
    Esc                  - back to NORMAL

  COMMAND mode (:):
    :w [filename]        - save canvas
    :e <filename>        - load canvas
    :wq                  - save and quit
    :q / :q!             - quit
    :goto x y            - teleport cursor
    :clear               - clear entire canvas
    :color [name|num]    - show or set active color
                           colors: default, red, green, yellow, blue, magenta, cyan, white (0-7)

  SEARCH mode (/ or ?):
    Type pattern, Enter  - search (substring match across row content)
    Esc                  - cancel

  Mouse:
    Left click           - move cursor to clicked position
    Scroll up/down       - scroll viewport

---
Features

  - Infinite sparse 2D canvas (no fixed bounds)
  - Row-major sorted storage for efficient row/range operations
  - Vim-style modal editing (6 modes)
  - Box-drawing mode with auto-joining corners, tees, and crossings
  - 8 foreground colors (applied in INSERT and BOX modes)
  - Minimap overlay (top-right corner, shows canvas bounds and viewport position)
  - Incremental search with wrapping (forward / and backward ?)
  - Undo/redo (full canvas snapshots)
  - Clipboard with line-yank and rectangular (visual) yank/paste
  - File persistence (text format: "x y token color_pair" per cell, supports box-drawing chars)
  - Mouse support (click to move, scroll to pan)
  - Auto-scroll margin when cursor approaches viewport edges
  - Terminal resize handling

---
File Format

  One cell per line: x y token color_pair
  - token is the ASCII char for printable chars, or ~NAME for box-drawing (e.g., ~HLINE, ~ULCORNER)
  - color_pair is optional (defaults to 0)
  - Lines starting with # are ignored

---
Dependencies

  - ncurses (terminal rendering)
  - CMake >= 3.15
  - C++17


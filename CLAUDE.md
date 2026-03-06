  Terminal Whiteboard — C++ Design Plan

  Core Concept

  A TUI application with an infinite 2D sparse canvas rendered through a fixed terminal viewport, with full vim modal editing.

  ---
  Architecture

  src/
    main.cpp              - Entry point, event loop
    canvas.hpp/cpp        - Sparse infinite canvas (hash map)
    viewport.hpp/cpp      - Viewport: pan/zoom over canvas
    renderer.hpp/cpp      - ncurses rendering layer
    editor.hpp/cpp        - Top-level editor state machine
    input.hpp/cpp         - Raw key parsing
    modes.hpp             - Mode enum (NORMAL, INSERT, VISUAL, COMMAND)
    keybindings.hpp/cpp   - Key→action dispatch
    commands.hpp/cpp      - :colon command parser
    undo.hpp/cpp          - Undo/redo stack
    serialization.hpp/cpp - Save/load canvas
  CMakeLists.txt

  ---
  Data Structures

  Canvas — sparse, infinite:
  struct Cell { char ch; short color_pair; attr_t attrs; };
  using Canvas = std::unordered_map<std::pair<int,int>, Cell, PairHash>;

  Viewport — window into canvas:
  struct Viewport {
      int canvas_x, canvas_y;   // top-left corner in canvas coords
      int width, height;         // terminal dimensions
  };

  Cursor — canvas coordinates (not screen):
  struct Cursor { int x, y; };

  ---
  Modes & Keybindings

  ┌─────────┬───────────┬───────────────────────────────────┐
  │  Mode   │    Key    │              Action               │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ h/j/k/l   │ move cursor                       │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ H/J/K/L   │ pan viewport (detach from cursor) │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ i/a/o/O   │ enter INSERT                      │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ v         │ enter VISUAL                      │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ :         │ enter COMMAND                     │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ x         │ delete char                       │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ dd        │ delete line                       │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ yy/p      │ yank/paste line                   │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ u/Ctrl+r  │ undo/redo                         │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ zz        │ center viewport on cursor         │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ Ctrl+d/u  │ half-page scroll                  │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ gg/G      │ jump to top/bottom of content     │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ NORMAL  │ 0/$       │ start/end of line                 │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ INSERT  │ Esc       │ back to NORMAL                    │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ INSERT  │ any char  │ write to canvas at cursor         │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ VISUAL  │ y/d       │ yank/delete selection             │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ COMMAND │ :w        │ save                              │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ COMMAND │ :q/:q!    │ quit                              │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ COMMAND │ :goto x y │ teleport cursor                   │
  ├─────────┼───────────┼───────────────────────────────────┤
  │ COMMAND │ :clear    │ clear canvas                      │
  └─────────┴───────────┴───────────────────────────────────┘

  ---
  Rendering Loop

  while (running):
    1. get terminal size → update viewport
    2. for each visible (screen_x, screen_y):
         canvas_coord = (viewport.x + screen_x, viewport.y + screen_y)
         draw canvas[canvas_coord] or space
    3. draw status bar: mode | cursor coords | filename
    4. position terminal cursor at (cursor - viewport)
    5. poll input → dispatch to current mode handler
    6. repeat

  ---
  Phases

  ┌─────────────────────┬────────────────────────────────────────────────────────────────────────────────────────────────┐
  │        Phase        │                                             Scope                                              │
  ├─────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 1 — Foundation      │ ncurses setup, canvas, viewport, hjkl movement, INSERT text input, Esc/i switching, status bar │
  ├─────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 2 — Vim Core        │ VISUAL mode, :COMMAND mode, dd/yy/p, undo/redo                                                 │
  ├─────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 3 — Canvas Features │ Viewport pan (H/J/K/L), zz centering, gg/G/0/$, Ctrl+d/u, search /?                            │
  ├─────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 4 — Persistence     │ :w/:e save/load, simple text-coordinate file format                                            │
  ├─────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ 5 — Polish          │ Colors, box-drawing mode, minimap, mouse support                                               │
  └─────────────────────┴────────────────────────────────────────────────────────────────────────────────────────────────┘

  ---
  Dependencies

  - ncurses (terminal rendering)
  - CMake (build)
  - C++17 (structured bindings, std::optional, etc.)


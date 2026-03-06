#pragma once
#include "canvas.hpp"
#include "modes.hpp"
#include "undo.hpp"
#include <string>
#include <vector>

struct Cursor {
    int x = 0;
    int y = 0;
};

struct Viewport {
    int x = 0;
    int y = 0;
    int w = 80;
    int h = 24;
};

struct Clipboard {
    // cells stored as (dx, dy) relative coords
    std::vector<std::pair<std::pair<int,int>, Cell>> cells;
    bool is_line_yank = false; // yy: paste preserves absolute x positions
    bool valid = false;
};

class Editor {
public:
    Canvas    canvas;
    Viewport  vp;
    Cursor    cursor;
    Mode      mode        = Mode::NORMAL;
    bool      running     = true;

    // VISUAL mode
    Cursor    visual_anchor;

    // COMMAND mode
    std::string cmd_buf;
    std::string status_msg;

    // SEARCH mode
    std::string search_buf;
    std::string last_search;
    bool search_forward = true;

    // Clipboard and undo
    Clipboard clipboard;
    UndoStack undo_stack;

    void handle_key(int key);
    void ensure_visible();

private:
    int pending_key = 0; // for dd, yy double-key sequences

    void normal_key(int key);
    void insert_key(int key);
    void visual_key(int key);
    void command_key(int key);

    void move_cursor(int dx, int dy);
    void save_undo();
    void do_dd();
    void do_yy();
    void do_paste();
    void execute_command(const std::string& cmd);
    void search_key(int key);
    bool do_find_next(bool forward);
};

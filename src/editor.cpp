#include "editor.hpp"
#include <ncurses.h>
#include <algorithm>
#include <cstdio>

static constexpr int SCROLL_MARGIN = 3;

void Editor::save_undo() {
    undo_stack.save(canvas, cursor.x, cursor.y);
}

void Editor::move_cursor(int dx, int dy) {
    cursor.x += dx;
    cursor.y += dy;
    ensure_visible();
}

void Editor::ensure_visible() {
    int mx = std::min(SCROLL_MARGIN, vp.w / 4);
    int my = std::min(SCROLL_MARGIN, (vp.h - 1) / 4);

    if (cursor.x < vp.x + mx)
        vp.x = cursor.x - mx;
    if (cursor.x >= vp.x + vp.w - mx)
        vp.x = cursor.x - vp.w + mx + 1;

    if (cursor.y < vp.y + my)
        vp.y = cursor.y - my;
    if (cursor.y >= vp.y + (vp.h - 1) - my)
        vp.y = cursor.y - (vp.h - 1) + my + 1;
}

void Editor::do_dd() {
    save_undo();
    for (auto it = canvas.begin(); it != canvas.end(); ) {
        if (it->first.second == cursor.y)
            it = canvas.erase(it);
        else
            ++it;
    }
}

void Editor::do_yy() {
    clipboard.cells.clear();
    for (auto& [pos, cell] : canvas) {
        if (pos.second == cursor.y)
            clipboard.cells.push_back({{pos.first, 0}, cell});
    }
    clipboard.is_line_yank = true;
    clipboard.valid = true;
}

void Editor::do_paste() {
    if (!clipboard.valid) return;
    save_undo();
    if (clipboard.is_line_yank) {
        int target_y = cursor.y + 1;
        for (auto& [rel, cell] : clipboard.cells)
            canvas[{rel.first, target_y}] = cell;
        cursor.y = target_y;
    } else {
        // Visual yank: paste at cursor using relative coords
        for (auto& [rel, cell] : clipboard.cells)
            canvas[{cursor.x + rel.first, cursor.y + rel.second}] = cell;
    }
    ensure_visible();
}

void Editor::normal_key(int key) {
    // Handle pending double-key sequences (dd, yy)
    if (pending_key != 0) {
        if (key == pending_key) {
            if (pending_key == 'd') do_dd();
            else if (pending_key == 'y') do_yy();
        }
        pending_key = 0;
        return;
    }

    switch (key) {
        case 'h': case KEY_LEFT:  move_cursor(-1,  0); break;
        case 'l': case KEY_RIGHT: move_cursor( 1,  0); break;
        case 'k': case KEY_UP:    move_cursor( 0, -1); break;
        case 'j': case KEY_DOWN:  move_cursor( 0,  1); break;

        case 'i':
            save_undo();
            mode = Mode::INSERT;
            break;
        case 'a':
            save_undo();
            move_cursor(1, 0);
            mode = Mode::INSERT;
            break;
        case 'o':
            save_undo();
            cursor.y++;
            cursor.x = 0;
            ensure_visible();
            mode = Mode::INSERT;
            break;
        case 'O':
            save_undo();
            cursor.y--;
            cursor.x = 0;
            ensure_visible();
            mode = Mode::INSERT;
            break;

        case 'v':
            visual_anchor = cursor;
            mode = Mode::VISUAL;
            break;

        case ':':
            cmd_buf.clear();
            status_msg.clear();
            mode = Mode::COMMAND;
            break;

        case 'x':
            save_undo();
            canvas.erase({cursor.x, cursor.y});
            break;

        case 'd':
            pending_key = 'd';
            break;
        case 'y':
            pending_key = 'y';
            break;
        case 'p':
            do_paste();
            break;

        case 'u':
            if (undo_stack.can_undo()) {
                auto s = undo_stack.pop_undo(canvas, cursor.x, cursor.y);
                canvas   = std::move(s.canvas);
                cursor.x = s.cursor_x;
                cursor.y = s.cursor_y;
                ensure_visible();
            }
            break;
        case 18: // Ctrl+r
            if (undo_stack.can_redo()) {
                auto s = undo_stack.pop_redo(canvas, cursor.x, cursor.y);
                canvas   = std::move(s.canvas);
                cursor.x = s.cursor_x;
                cursor.y = s.cursor_y;
                ensure_visible();
            }
            break;

        case 'q':
            running = false;
            break;
    }
}

void Editor::insert_key(int key) {
    switch (key) {
        case 27: // Escape
            mode = Mode::NORMAL;
            break;
        case KEY_LEFT:  move_cursor(-1,  0); break;
        case KEY_RIGHT: move_cursor( 1,  0); break;
        case KEY_UP:    move_cursor( 0, -1); break;
        case KEY_DOWN:  move_cursor( 0,  1); break;
        case KEY_BACKSPACE: case 127:
            if (cursor.x > 0) {
                cursor.x--;
                canvas.erase({cursor.x, cursor.y});
                ensure_visible();
            }
            break;
        case '\n': case KEY_ENTER:
            cursor.y++;
            cursor.x = 0;
            ensure_visible();
            break;
        default:
            if (key >= 32 && key <= 126) {
                canvas[{cursor.x, cursor.y}] = Cell{(char)key};
                move_cursor(1, 0);
            }
            break;
    }
}

void Editor::visual_key(int key) {
    switch (key) {
        case 27: // Escape
            mode = Mode::NORMAL;
            break;
        case 'h': case KEY_LEFT:  move_cursor(-1,  0); break;
        case 'l': case KEY_RIGHT: move_cursor( 1,  0); break;
        case 'k': case KEY_UP:    move_cursor( 0, -1); break;
        case 'j': case KEY_DOWN:  move_cursor( 0,  1); break;
        case 'y': {
            int min_x = std::min(visual_anchor.x, cursor.x);
            int max_x = std::max(visual_anchor.x, cursor.x);
            int min_y = std::min(visual_anchor.y, cursor.y);
            int max_y = std::max(visual_anchor.y, cursor.y);
            clipboard.cells.clear();
            for (auto& [pos, cell] : canvas) {
                if (pos.first >= min_x && pos.first <= max_x &&
                    pos.second >= min_y && pos.second <= max_y)
                    clipboard.cells.push_back({{pos.first - min_x, pos.second - min_y}, cell});
            }
            clipboard.is_line_yank = false;
            clipboard.valid = true;
            mode = Mode::NORMAL;
            break;
        }
        case 'd': {
            save_undo();
            int min_x = std::min(visual_anchor.x, cursor.x);
            int max_x = std::max(visual_anchor.x, cursor.x);
            int min_y = std::min(visual_anchor.y, cursor.y);
            int max_y = std::max(visual_anchor.y, cursor.y);
            for (auto it = canvas.begin(); it != canvas.end(); ) {
                if (it->first.first  >= min_x && it->first.first  <= max_x &&
                    it->first.second >= min_y && it->first.second <= max_y)
                    it = canvas.erase(it);
                else
                    ++it;
            }
            mode = Mode::NORMAL;
            break;
        }
    }
}

void Editor::command_key(int key) {
    switch (key) {
        case 27: // Escape
            mode = Mode::NORMAL;
            break;
        case '\n': case KEY_ENTER: {
            std::string cmd = cmd_buf;
            cmd_buf.clear();
            mode = Mode::NORMAL;
            execute_command(cmd);
            break;
        }
        case KEY_BACKSPACE: case 127:
            if (cmd_buf.empty())
                mode = Mode::NORMAL;
            else
                cmd_buf.pop_back();
            break;
        default:
            if (key >= 32 && key <= 126)
                cmd_buf += (char)key;
            break;
    }
}

void Editor::execute_command(const std::string& cmd) {
    if (cmd == "q" || cmd == "q!") {
        running = false;
    } else if (cmd == "clear") {
        save_undo();
        canvas.clear();
    } else if (cmd.size() > 5 && cmd.substr(0, 5) == "goto ") {
        int x, y;
        if (std::sscanf(cmd.c_str() + 5, "%d %d", &x, &y) == 2) {
            cursor.x = x;
            cursor.y = y;
            ensure_visible();
        }
    } else if (cmd == "w") {
        status_msg = "[Phase 4] :w not yet implemented";
    } else if (!cmd.empty()) {
        status_msg = "Unknown command: " + cmd;
    }
}

void Editor::handle_key(int key) {
    if (key == KEY_RESIZE) { ensure_visible(); return; }
    if (key == 3)          { running = false;  return; } // Ctrl+C
    switch (mode) {
        case Mode::NORMAL:  normal_key(key);  break;
        case Mode::INSERT:  insert_key(key);  break;
        case Mode::VISUAL:  visual_key(key);  break;
        case Mode::COMMAND: command_key(key); break;
    }
}

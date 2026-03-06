#include "editor.hpp"
#include <ncurses.h>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <map>

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
            else if (pending_key == 'g') {
                int min_y = INT_MAX;
                for (auto& [pos, cell] : canvas)
                    if (pos.second < min_y) min_y = pos.second;
                cursor.y = (min_y == INT_MAX) ? 0 : min_y;
                cursor.x = 0;
                ensure_visible();
            } else if (pending_key == 'z') {
                vp.x = cursor.x - vp.w / 2;
                vp.y = cursor.y - (vp.h - 1) / 2;
            }
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

        // Viewport pan — detach from cursor
        case 'H': vp.x--; break;
        case 'L': vp.x++; break;
        case 'K': vp.y--; break;
        case 'J': vp.y++; break;

        // Jump to bottom of content
        case 'G': {
            int max_y = INT_MIN;
            for (auto& [pos, cell] : canvas)
                if (pos.second > max_y) max_y = pos.second;
            cursor.y = (max_y == INT_MIN) ? 0 : max_y;
            cursor.x = 0;
            ensure_visible();
            break;
        }

        // Start / end of line
        case '0':
            cursor.x = 0;
            ensure_visible();
            break;
        case '$': {
            int max_x = INT_MIN;
            for (auto& [pos, cell] : canvas)
                if (pos.second == cursor.y && pos.first > max_x) max_x = pos.first;
            if (max_x != INT_MIN) { cursor.x = max_x; ensure_visible(); }
            break;
        }

        // Half-page scroll
        case 4: { // Ctrl+d
            int half = (vp.h - 1) / 2;
            cursor.y += half;
            vp.y    += half;
            break;
        }
        case 21: { // Ctrl+u
            int half = (vp.h - 1) / 2;
            cursor.y -= half;
            vp.y    -= half;
            break;
        }

        // Double-key prefixes
        case 'g': pending_key = 'g'; break;
        case 'z': pending_key = 'z'; break;

        // Search
        case '/':
            search_buf.clear();
            search_forward = true;
            mode = Mode::SEARCH;
            break;
        case '?':
            search_buf.clear();
            search_forward = false;
            mode = Mode::SEARCH;
            break;
        case 'n': do_find_next(search_forward);  break;
        case 'N': do_find_next(!search_forward); break;

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

void Editor::search_key(int key) {
    switch (key) {
        case 27: // Escape
            mode = Mode::NORMAL;
            status_msg.clear();
            break;
        case '\n': case KEY_ENTER: {
            last_search = search_buf;
            mode = Mode::NORMAL;
            if (!last_search.empty() && !do_find_next(search_forward))
                status_msg = "Pattern not found: " + last_search;
            break;
        }
        case KEY_BACKSPACE: case 127:
            if (search_buf.empty())
                mode = Mode::NORMAL;
            else
                search_buf.pop_back();
            break;
        default:
            if (key >= 32 && key <= 126)
                search_buf += (char)key;
            break;
    }
}

bool Editor::do_find_next(bool forward) {
    if (last_search.empty()) return false;

    // Build per-row sorted content
    std::map<int, std::map<int, char>> rows;
    for (auto& [pos, cell] : canvas)
        rows[pos.second][pos.first] = cell.ch;

    if (rows.empty()) return false;

    // Collect all match positions (y, x) across the canvas
    std::vector<std::pair<int,int>> matches;
    for (auto& [y, row] : rows) {
        if (row.empty()) continue;
        int min_x = row.begin()->first;
        int max_x = row.rbegin()->first;
        std::string s(max_x - min_x + 1, ' ');
        for (auto& [x, ch] : row) s[x - min_x] = ch;

        size_t p = 0;
        while ((p = s.find(last_search, p)) != std::string::npos) {
            matches.push_back({y, min_x + (int)p});
            p++;
        }
    }

    if (matches.empty()) {
        status_msg = "Pattern not found: " + last_search;
        return false;
    }

    std::sort(matches.begin(), matches.end());
    std::pair<int,int> cur = {cursor.y, cursor.x};

    if (forward) {
        auto it = std::upper_bound(matches.begin(), matches.end(), cur);
        if (it == matches.end()) { status_msg = "search wrapped"; it = matches.begin(); }
        cursor.y = it->first;
        cursor.x = it->second;
    } else {
        auto it = std::lower_bound(matches.begin(), matches.end(), cur);
        if (it == matches.begin()) { status_msg = "search wrapped"; it = matches.end(); }
        --it;
        cursor.y = it->first;
        cursor.x = it->second;
    }

    ensure_visible();
    return true;
}

void Editor::handle_key(int key) {
    if (key == KEY_RESIZE) { ensure_visible(); return; }
    if (key == 3)          { running = false;  return; } // Ctrl+C
    switch (mode) {
        case Mode::NORMAL:  normal_key(key);  break;
        case Mode::INSERT:  insert_key(key);  break;
        case Mode::VISUAL:  visual_key(key);  break;
        case Mode::COMMAND: command_key(key); break;
        case Mode::SEARCH:  search_key(key);  break;
    }
}

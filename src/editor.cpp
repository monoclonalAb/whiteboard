#include "editor.hpp"
#include "serialization.hpp"
#include <ncurses.h>
#include <algorithm>
#include <cstdio>

static constexpr int SCROLL_MARGIN = 3;

// ─── Box-drawing helpers ───────────────────────────────────────────────────────

static bool is_box_char(chtype c) {
    return c == ACS_HLINE || c == ACS_VLINE ||
           c == ACS_ULCORNER || c == ACS_URCORNER ||
           c == ACS_LLCORNER || c == ACS_LRCORNER ||
           c == ACS_LTEE || c == ACS_RTEE ||
           c == ACS_TTEE || c == ACS_BTEE || c == ACS_PLUS;
}

// Each function answers: does this char (at a neighbor) connect *toward* us?
static bool connects_right(chtype c) {
    return c == ACS_HLINE || c == ACS_ULCORNER || c == ACS_LLCORNER ||
           c == ACS_LTEE || c == ACS_TTEE || c == ACS_BTEE || c == ACS_PLUS;
}
static bool connects_left(chtype c) {
    return c == ACS_HLINE || c == ACS_URCORNER || c == ACS_LRCORNER ||
           c == ACS_RTEE || c == ACS_TTEE || c == ACS_BTEE || c == ACS_PLUS;
}
static bool connects_down(chtype c) {
    return c == ACS_VLINE || c == ACS_ULCORNER || c == ACS_URCORNER ||
           c == ACS_LTEE || c == ACS_RTEE || c == ACS_TTEE || c == ACS_PLUS;
}
static bool connects_up(chtype c) {
    return c == ACS_VLINE || c == ACS_LLCORNER || c == ACS_LRCORNER ||
           c == ACS_LTEE || c == ACS_RTEE || c == ACS_BTEE || c == ACS_PLUS;
}

static chtype pick_box_char(bool L, bool R, bool U, bool D) {
    if (L && R && U && D) return ACS_PLUS;
    if (L && R && U)      return ACS_BTEE;
    if (L && R && D)      return ACS_TTEE;
    if (L && U && D)      return ACS_RTEE;
    if (R && U && D)      return ACS_LTEE;
    if (U && D)           return ACS_VLINE;
    if (L && R)           return ACS_HLINE;
    if (L && U)           return ACS_LRCORNER;
    if (L && D)           return ACS_URCORNER;
    if (R && U)           return ACS_LLCORNER;
    if (R && D)           return ACS_ULCORNER;
    if (L || R)           return ACS_HLINE;
    if (U || D)           return ACS_VLINE;
    return ACS_PLUS;
}

// Returns {L,R,U,D} connections for (x,y) derived purely from its canvas neighbors.
// Helper: look up a single cell in the row-major canvas.
static const Cell* cell_at(const Canvas& canvas, int x, int y) {
    auto rit = canvas.find(y);
    if (rit == canvas.end()) return nullptr;
    auto cit = rit->second.find(x);
    if (cit == rit->second.end()) return nullptr;
    return &cit->second;
}

static std::tuple<bool,bool,bool,bool> cell_connections(const Canvas& canvas, int x, int y) {
    bool L = false, R = false, U = false, D = false;
    auto get = [&](int nx, int ny) -> chtype {
        auto* c = cell_at(canvas, nx, ny);
        return (c && is_box_char(c->ch)) ? c->ch : 0;
    };
    if (chtype c = get(x-1, y); c && connects_right(c)) L = true;
    if (chtype c = get(x+1, y); c && connects_left(c))  R = true;
    if (chtype c = get(x, y-1); c && connects_down(c))  U = true;
    if (chtype c = get(x, y+1); c && connects_up(c))    D = true;
    return {L, R, U, D};
}

// ─── Editor methods ────────────────────────────────────────────────────────────

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
    canvas.erase(cursor.y);
}

void Editor::do_yy() {
    clipboard.cells.clear();
    auto rit = canvas.find(cursor.y);
    if (rit != canvas.end()) {
        for (auto& [x, cell] : rit->second)
            clipboard.cells.push_back({{x, 0}, cell});
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
            canvas[target_y][rel.first] = cell;
        cursor.y = target_y;
    } else {
        for (auto& [rel, cell] : clipboard.cells)
            canvas[cursor.y + rel.second][cursor.x + rel.first] = cell;
    }
    ensure_visible();
}

void Editor::update_box_cell(int x, int y, int going_dx, int going_dy) {
    bool L = false, R = false, U = false, D = false;

    // Connection from where we came (opposite of last movement direction)
    if (box_last_dx ==  1) L = true;
    if (box_last_dx == -1) R = true;
    if (box_last_dy ==  1) U = true;
    if (box_last_dy == -1) D = true;

    // Connection toward where we're going
    if (going_dx ==  1) R = true;
    if (going_dx == -1) L = true;
    if (going_dy ==  1) D = true;
    if (going_dy == -1) U = true;

    // Merge with neighbor connections (auto-join existing box chars)
    auto [nL, nR, nU, nD] = cell_connections(canvas, x, y);
    L |= nL; R |= nR; U |= nU; D |= nD;

    canvas[y][x] = Cell{pick_box_char(L, R, U, D), active_color};

    // Update immediate neighbors so their junction type stays correct
    std::pair<int,int> neighbors[] = {{x-1,y}, {x+1,y}, {x,y-1}, {x,y+1}};
    for (auto [nx, ny] : neighbors) {
        auto* c = cell_at(canvas, nx, ny);
        if (c && is_box_char(c->ch)) {
            auto [cL, cR, cU, cD] = cell_connections(canvas, nx, ny);
            canvas[ny][nx].ch = pick_box_char(cL, cR, cU, cD);
        }
    }
}

void Editor::box_key(int key) {
    int dx = 0, dy = 0;
    switch (key) {
        case 'h': case KEY_LEFT:  dx = -1; break;
        case 'l': case KEY_RIGHT: dx =  1; break;
        case 'k': case KEY_UP:    dy = -1; break;
        case 'j': case KEY_DOWN:  dy =  1; break;
        case 27: // Escape
            mode = Mode::NORMAL;
            box_last_dx = box_last_dy = 0;
            return;
        default:
            return;
    }
    update_box_cell(cursor.x, cursor.y, dx, dy);
    cursor.x += dx;
    cursor.y += dy;
    box_last_dx = dx;
    box_last_dy = dy;
    ensure_visible();
}

void Editor::handle_mouse() {
    MEVENT me;
    if (getmouse(&me) != OK) return;

    if (me.bstate & BUTTON1_CLICKED) {
        if (me.y < vp.h - 1) {
            cursor.x = vp.x + me.x;
            cursor.y = vp.y + me.y;
            ensure_visible();
        }
    }
#ifdef BUTTON4_PRESSED
    if (me.bstate & BUTTON4_PRESSED) vp.y -= 3;
#endif
#ifdef BUTTON5_PRESSED
    if (me.bstate & BUTTON5_PRESSED) vp.y += 3;
#endif
}

void Editor::normal_key(int key) {
    // Handle pending double-key sequences (dd, yy, gg, zz)
    if (pending_key != 0) {
        if (key == pending_key) {
            if (pending_key == 'd') do_dd();
            else if (pending_key == 'y') do_yy();
            else if (pending_key == 'g') {
                cursor.y = canvas.empty() ? 0 : canvas.begin()->first;
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

        case 'b':
            save_undo();
            box_last_dx = box_last_dy = 0;
            mode = Mode::BOX;
            break;

        case 'm':
            show_minimap = !show_minimap;
            break;

        case ':':
            cmd_buf.clear();
            status_msg.clear();
            mode = Mode::COMMAND;
            break;

        case 'x': {
            save_undo();
            auto rit = canvas.find(cursor.y);
            if (rit != canvas.end()) {
                rit->second.erase(cursor.x);
                if (rit->second.empty()) canvas.erase(rit);
            }
            break;
        }

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
            cursor.y = canvas.empty() ? 0 : canvas.rbegin()->first;
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
            auto rit = canvas.find(cursor.y);
            if (rit != canvas.end() && !rit->second.empty()) {
                cursor.x = rit->second.rbegin()->first;
                ensure_visible();
            }
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
                auto rit = canvas.find(cursor.y);
                if (rit != canvas.end()) {
                    rit->second.erase(cursor.x);
                    if (rit->second.empty()) canvas.erase(rit);
                }
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
                canvas[cursor.y][cursor.x] = Cell{(chtype)key, active_color};
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
            for (auto rit = canvas.lower_bound(min_y);
                 rit != canvas.end() && rit->first <= max_y; ++rit) {
                for (auto cit = rit->second.lower_bound(min_x);
                     cit != rit->second.end() && cit->first <= max_x; ++cit) {
                    clipboard.cells.push_back(
                        {{cit->first - min_x, rit->first - min_y}, cit->second});
                }
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
            for (auto rit = canvas.lower_bound(min_y);
                 rit != canvas.end() && rit->first <= max_y; ) {
                for (auto cit = rit->second.lower_bound(min_x);
                     cit != rit->second.end() && cit->first <= max_x; )
                    cit = rit->second.erase(cit);
                if (rit->second.empty())
                    rit = canvas.erase(rit);
                else
                    ++rit;
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
    if (cmd == "q") {
        running = false;
    } else if (cmd == "q!") {
        running = false;
    } else if (cmd == "wq" || cmd == "wq!") {
        if (filename.empty()) {
            status_msg = "E: no filename — use :w <filename>";
            return;
        }
        if (save_canvas(canvas, filename))
            running = false;
        else
            status_msg = "E: could not write \"" + filename + "\"";
    } else if (cmd == "w") {
        if (filename.empty()) {
            status_msg = "E: no filename — use :w <filename>";
        } else if (save_canvas(canvas, filename)) {
            status_msg = "\"" + filename + "\" written";
        } else {
            status_msg = "E: could not write \"" + filename + "\"";
        }
    } else if (cmd.size() > 2 && cmd.substr(0, 2) == "w ") {
        filename = cmd.substr(2);
        if (save_canvas(canvas, filename))
            status_msg = "\"" + filename + "\" written";
        else
            status_msg = "E: could not write \"" + filename + "\"";
    } else if (cmd.size() > 2 && cmd.substr(0, 2) == "e ") {
        std::string new_file = cmd.substr(2);
        Canvas new_canvas;
        if (load_canvas(new_canvas, new_file)) {
            save_undo();
            canvas = std::move(new_canvas);
            filename = new_file;
            cursor.x = 0;
            cursor.y = 0;
            ensure_visible();
            status_msg = "\"" + filename + "\" loaded";
        } else {
            status_msg = "E: could not read \"" + new_file + "\"";
        }
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
    } else if (cmd == "color") {
        static const char* names[] = {"default","red","green","yellow","blue","magenta","cyan","white"};
        status_msg = std::string("Color: ") + names[active_color];
    } else if (cmd.size() > 6 && cmd.substr(0, 6) == "color ") {
        static const char* names[] = {"default","red","green","yellow","blue","magenta","cyan","white"};
        std::string name = cmd.substr(6);
        short cp = -1;
        if      (name == "default" || name == "0") cp = 0;
        else if (name == "red"     || name == "1") cp = 1;
        else if (name == "green"   || name == "2") cp = 2;
        else if (name == "yellow"  || name == "3") cp = 3;
        else if (name == "blue"    || name == "4") cp = 4;
        else if (name == "magenta" || name == "5") cp = 5;
        else if (name == "cyan"    || name == "6") cp = 6;
        else if (name == "white"   || name == "7") cp = 7;
        if (cp < 0) {
            status_msg = "Unknown color: " + name;
        } else {
            active_color = cp;
            status_msg = std::string("Color: ") + names[cp];
        }
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
    if (canvas.empty()) return false;

    std::vector<std::pair<int,int>> matches;
    for (auto& [y, row] : canvas) {
        if (row.empty()) continue;
        int min_x = row.begin()->first;
        int max_x = row.rbegin()->first;
        std::string s(max_x - min_x + 1, ' ');
        for (auto& [x, cell] : row) {
            chtype ch = cell.ch;
            if (ch >= 32 && ch <= 126) s[x - min_x] = (char)ch;
        }

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

    // matches are already sorted since canvas is a sorted map
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
    if (key == KEY_MOUSE)  { handle_mouse();   return; }
    switch (mode) {
        case Mode::NORMAL:  normal_key(key);  break;
        case Mode::INSERT:  insert_key(key);  break;
        case Mode::VISUAL:  visual_key(key);  break;
        case Mode::COMMAND: command_key(key); break;
        case Mode::SEARCH:  search_key(key);  break;
        case Mode::BOX:     box_key(key);     break;
    }
}

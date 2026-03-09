#include "renderer.hpp"
#include <ncurses.h>
#include <algorithm>
#include <set>
#include <string>

static constexpr int MM_W = 18; // minimap inner width
static constexpr int MM_H = 8;  // minimap inner height

void Renderer::init() {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    // Colors
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_RED,     -1);
        init_pair(2, COLOR_GREEN,   -1);
        init_pair(3, COLOR_YELLOW,  -1);
        init_pair(4, COLOR_BLUE,    -1);
        init_pair(5, COLOR_MAGENTA, -1);
        init_pair(6, COLOR_CYAN,    -1);
        init_pair(7, COLOR_WHITE,   -1);
    }

    // Mouse
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
}

void Renderer::cleanup() {
    endwin();
}

void Renderer::update_size(Viewport& vp) {
    getmaxyx(stdscr, vp.h, vp.w);
}

void Renderer::render_minimap(const Editor& ed) {
    // Require minimum terminal size to show the minimap
    if (ed.vp.w < MM_W + 6 || ed.vp.h < MM_H + 5) return;

    // Compute canvas bounding box, seeded with the current viewport
    int min_cx = ed.vp.x,              max_cx = ed.vp.x + ed.vp.w - 1;
    int min_cy = ed.vp.y,              max_cy = ed.vp.y + ed.vp.h - 1;
    for (auto& [y, row] : ed.canvas) {
        min_cy = std::min(min_cy, y);
        max_cy = std::max(max_cy, y);
        if (!row.empty()) {
            min_cx = std::min(min_cx, row.begin()->first);
            max_cx = std::max(max_cx, row.rbegin()->first);
        }
    }

    float sx = (float)MM_W / (max_cx - min_cx + 1);
    float sy = (float)MM_H / (max_cy - min_cy + 1);

    // Build set of occupied minimap pixels in one O(n) pass
    std::set<std::pair<int,int>> occ;
    for (auto& [y, row] : ed.canvas) {
        for (auto& [x, cell] : row) {
            int mx = (int)((x  - min_cx) * sx);
            int my = (int)((y  - min_cy) * sy);
            if (mx >= 0 && mx < MM_W && my >= 0 && my < MM_H)
                occ.insert({mx, my});
        }
    }

    // Viewport rectangle in minimap coordinates
    int vx0 = std::max(0,        (int)((ed.vp.x - min_cx) * sx));
    int vx1 = std::min(MM_W - 1, (int)((ed.vp.x + ed.vp.w - 1 - min_cx) * sx));
    int vy0 = std::max(0,        (int)((ed.vp.y - min_cy) * sy));
    int vy1 = std::min(MM_H - 1, (int)((ed.vp.y + ed.vp.h - 1 - min_cy) * sy));

    // Cursor in minimap coordinates
    int cmx = std::max(0, std::min(MM_W - 1, (int)((ed.cursor.x - min_cx) * sx)));
    int cmy = std::max(0, std::min(MM_H - 1, (int)((ed.cursor.y - min_cy) * sy)));

    // Screen position: top-right corner, inner area starts at col bx, row 1
    int bx = ed.vp.w - MM_W - 2; // left col of inner cells
    int by = 0;                   // top row (border row)

    // Draw border
    mvaddch(by,            bx - 1,    ACS_ULCORNER);
    mvaddch(by,            bx + MM_W, ACS_URCORNER);
    mvaddch(by + MM_H + 1, bx - 1,    ACS_LLCORNER);
    mvaddch(by + MM_H + 1, bx + MM_W, ACS_LRCORNER);
    for (int i = 0; i < MM_W; i++) {
        mvaddch(by,            bx + i, ACS_HLINE);
        mvaddch(by + MM_H + 1, bx + i, ACS_HLINE);
    }
    for (int i = 1; i <= MM_H; i++) {
        mvaddch(by + i, bx - 1,    ACS_VLINE);
        mvaddch(by + i, bx + MM_W, ACS_VLINE);
    }

    // Fill minimap cells
    for (int my = 0; my < MM_H; my++) {
        for (int mx = 0; mx < MM_W; mx++) {
            bool in_vp  = (mx >= vx0 && mx <= vx1 && my >= vy0 && my <= vy1);
            chtype ch   = occ.count({mx, my}) ? '.' : ' ';
            if (mx == cmx && my == cmy) ch = '@';

            if (in_vp) attron(A_REVERSE);
            mvaddch(by + 1 + my, bx + mx, ch);
            if (in_vp) attroff(A_REVERSE);
        }
    }
}

void Renderer::render(const Editor& ed) {
    erase();

    int canvas_rows = ed.vp.h - 1; // last row reserved for status bar

    // Compute visual selection rect if in VISUAL mode
    int vis_min_x = 0, vis_max_x = 0, vis_min_y = 0, vis_max_y = 0;
    bool in_visual = (ed.mode == Mode::VISUAL);
    if (in_visual) {
        vis_min_x = std::min(ed.visual_anchor.x, ed.cursor.x);
        vis_max_x = std::max(ed.visual_anchor.x, ed.cursor.x);
        vis_min_y = std::min(ed.visual_anchor.y, ed.cursor.y);
        vis_max_y = std::max(ed.visual_anchor.y, ed.cursor.y);
    }

    for (int sy = 0; sy < canvas_rows; sy++) {
        int cy = ed.vp.y + sy;
        auto rit = ed.canvas.find(cy);

        for (int sx = 0; sx < ed.vp.w; sx++) {
            int cx = ed.vp.x + sx;

            bool highlighted = in_visual &&
                cx >= vis_min_x && cx <= vis_max_x &&
                cy >= vis_min_y && cy <= vis_max_y;

            const Cell* cell = nullptr;
            if (rit != ed.canvas.end()) {
                auto cit = rit->second.find(cx);
                if (cit != rit->second.end())
                    cell = &cit->second;
            }

            if (cell || highlighted) {
                short  cp = cell ? cell->color_pair : 0;
                chtype ch = cell ? cell->ch : ' ';

                if (highlighted) attron(A_REVERSE);
                if (cp > 0)      attron(COLOR_PAIR(cp));
                mvaddch(sy, sx, ch);
                if (cp > 0)      attroff(COLOR_PAIR(cp));
                if (highlighted) attroff(A_REVERSE);
            }
        }
    }

    // Minimap overlay
    if (ed.show_minimap)
        render_minimap(ed);

    // Status bar
    attron(A_REVERSE);
    mvhline(ed.vp.h - 1, 0, ' ', ed.vp.w);

    if (ed.mode == Mode::COMMAND) {
        std::string cmd_line = ":" + ed.cmd_buf;
        mvaddstr(ed.vp.h - 1, 0, cmd_line.c_str());
        attroff(A_REVERSE);
        move(ed.vp.h - 1, (int)cmd_line.size());
    } else if (ed.mode == Mode::SEARCH) {
        char prefix = ed.search_forward ? '/' : '?';
        std::string search_line = prefix + ed.search_buf;
        mvaddstr(ed.vp.h - 1, 0, search_line.c_str());
        attroff(A_REVERSE);
        move(ed.vp.h - 1, (int)search_line.size());
    } else {
        std::string mode_str;
        switch (ed.mode) {
            case Mode::NORMAL: mode_str = "-- NORMAL --"; break;
            case Mode::INSERT: mode_str = "-- INSERT --"; break;
            case Mode::VISUAL: mode_str = "-- VISUAL --"; break;
            case Mode::BOX:    mode_str = "-- BOX --";    break;
            default: break;
        }
        // Active color indicator in mode string
        if (ed.active_color > 0) {
            static const char* cnames[] = {"","RED","GRN","YEL","BLU","MAG","CYN","WHT"};
            mode_str += " [";
            mode_str += cnames[ed.active_color];
            mode_str += "]";
        }
        if (!ed.filename.empty())
            mode_str += " " + ed.filename;

        std::string center_str = ed.status_msg;
        std::string pos_str = " x:" + std::to_string(ed.cursor.x)
                            + " y:" + std::to_string(ed.cursor.y) + " ";

        mvaddstr(ed.vp.h - 1, 0, mode_str.c_str());

        if (!center_str.empty()) {
            int col = ((int)ed.vp.w - (int)center_str.size()) / 2;
            if (col > (int)mode_str.size())
                mvaddstr(ed.vp.h - 1, col, center_str.c_str());
        }

        int pos_col = ed.vp.w - (int)pos_str.size();
        if (pos_col > (int)mode_str.size())
            mvaddstr(ed.vp.h - 1, pos_col, pos_str.c_str());

        attroff(A_REVERSE);

        // Place terminal cursor
        int screen_x = ed.cursor.x - ed.vp.x;
        int screen_y = ed.cursor.y - ed.vp.y;
        move(screen_y, screen_x);
    }

    refresh();
}

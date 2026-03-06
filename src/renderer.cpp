#include "renderer.hpp"
#include <ncurses.h>
#include <algorithm>
#include <string>

void Renderer::init() {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
}

void Renderer::cleanup() {
    endwin();
}

void Renderer::update_size(Viewport& vp) {
    getmaxyx(stdscr, vp.h, vp.w);
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
        for (int sx = 0; sx < ed.vp.w; sx++) {
            int cx = ed.vp.x + sx;
            int cy = ed.vp.y + sy;

            bool highlighted = in_visual &&
                cx >= vis_min_x && cx <= vis_max_x &&
                cy >= vis_min_y && cy <= vis_max_y;

            if (highlighted) attron(A_REVERSE);

            auto it = ed.canvas.find({cx, cy});
            if (it != ed.canvas.end())
                mvaddch(sy, sx, it->second.ch);
            else if (highlighted)
                mvaddch(sy, sx, ' ');

            if (highlighted) attroff(A_REVERSE);
        }
    }

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
            default: break;
        }
        std::string center_str = ed.status_msg.empty()
            ? ""
            : ed.status_msg;
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

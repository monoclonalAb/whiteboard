#pragma once
#include <ncurses.h>
#include <map>

struct Cell {
    chtype ch         = ' ';
    short  color_pair = 0;
};

// Row-major canvas: y → (x → Cell), sorted by both dimensions.
using Row    = std::map<int, Cell>;
using Canvas = std::map<int, Row>;

#include "serialization.hpp"
#include <fstream>
#include <sstream>

// Converts a chtype to a portable file token, or "" to skip the cell.
static std::string chtype_to_token(chtype ch) {
    if (ch == ACS_HLINE)    return "~HLINE";
    if (ch == ACS_VLINE)    return "~VLINE";
    if (ch == ACS_ULCORNER) return "~ULCORNER";
    if (ch == ACS_URCORNER) return "~URCORNER";
    if (ch == ACS_LLCORNER) return "~LLCORNER";
    if (ch == ACS_LRCORNER) return "~LRCORNER";
    if (ch == ACS_LTEE)     return "~LTEE";
    if (ch == ACS_RTEE)     return "~RTEE";
    if (ch == ACS_TTEE)     return "~TTEE";
    if (ch == ACS_BTEE)     return "~BTEE";
    if (ch == ACS_PLUS)     return "~PLUS";
    if (ch >= 33 && ch <= 126)
        return std::string(1, (char)ch);
    return ""; // skip spaces and unrepresentable chars
}

// Converts a file token back to a chtype, or 0 on failure.
static chtype token_to_chtype(const std::string& tok) {
    if (tok == "~HLINE")    return ACS_HLINE;
    if (tok == "~VLINE")    return ACS_VLINE;
    if (tok == "~ULCORNER") return ACS_ULCORNER;
    if (tok == "~URCORNER") return ACS_URCORNER;
    if (tok == "~LLCORNER") return ACS_LLCORNER;
    if (tok == "~LRCORNER") return ACS_LRCORNER;
    if (tok == "~LTEE")     return ACS_LTEE;
    if (tok == "~RTEE")     return ACS_RTEE;
    if (tok == "~TTEE")     return ACS_TTEE;
    if (tok == "~BTEE")     return ACS_BTEE;
    if (tok == "~PLUS")     return ACS_PLUS;
    if (tok.size() == 1)    return (chtype)(unsigned char)tok[0];
    return 0;
}

// File format: one cell per line — "x y token color_pair"
// token is the char for ASCII, or ~NAME for box-drawing chars.
// color_pair is optional (defaults to 0 for backward compatibility).
bool save_canvas(const Canvas& canvas, const std::string& filename) {
    std::ofstream f(filename);
    if (!f) return false;
    for (auto& [y, row] : canvas) {
        for (auto& [x, cell] : row) {
            std::string tok = chtype_to_token(cell.ch);
            if (tok.empty()) continue;
            f << x << ' ' << y << ' ' << tok << ' ' << cell.color_pair << '\n';
        }
    }
    return f.good();
}

bool load_canvas(Canvas& canvas, const std::string& filename) {
    std::ifstream f(filename);
    if (!f) return false;
    canvas.clear();
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        int x, y;
        std::string tok;
        short cp = 0;
        if (!(ss >> x >> y >> tok)) continue;
        ss >> cp; // optional; stays 0 if absent (backward compatible)
        chtype ch = token_to_chtype(tok);
        if (ch == 0) continue;
        canvas[y][x] = Cell{ch, cp};
    }
    return true;
}

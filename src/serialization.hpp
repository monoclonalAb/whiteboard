#pragma once
#include "canvas.hpp"
#include <string>

// Returns true on success.
// File format: one cell per line — "x y char"
bool save_canvas(const Canvas& canvas, const std::string& filename);
bool load_canvas(Canvas& canvas, const std::string& filename);

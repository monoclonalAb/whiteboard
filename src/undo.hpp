#pragma once
#include "canvas.hpp"
#include <vector>

struct UndoState {
    Canvas canvas;
    int cursor_x, cursor_y;
};

class UndoStack {
public:
    void save(const Canvas& c, int cx, int cy) {
        undo_.push_back({c, cx, cy});
        redo_.clear();
    }

    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }

    UndoState pop_undo(const Canvas& current_c, int cx, int cy) {
        UndoState s = undo_.back();
        undo_.pop_back();
        redo_.push_back({current_c, cx, cy});
        return s;
    }

    UndoState pop_redo(const Canvas& current_c, int cx, int cy) {
        UndoState s = redo_.back();
        redo_.pop_back();
        undo_.push_back({current_c, cx, cy});
        return s;
    }

private:
    std::vector<UndoState> undo_;
    std::vector<UndoState> redo_;
};

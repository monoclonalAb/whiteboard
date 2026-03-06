#pragma once
#include <unordered_map>
#include <utility>
#include <functional>

struct Cell {
    char ch;
};

struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const noexcept {
        std::size_t seed = std::hash<int>{}(p.first);
        seed ^= std::hash<int>{}(p.second) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }
};

using Canvas = std::unordered_map<std::pair<int,int>, Cell, PairHash>;

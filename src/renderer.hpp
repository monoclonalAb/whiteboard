#pragma once
#include "editor.hpp"

class Renderer {
public:
    void init();
    void cleanup();
    void render(const Editor& ed);
    void update_size(Viewport& vp);
};

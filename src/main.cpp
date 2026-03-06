#include <ncurses.h>
#include <csignal>
#include <cstdlib>
#include "editor.hpp"
#include "renderer.hpp"

static Renderer* g_renderer = nullptr;

static void signal_handler(int) {
    if (g_renderer) g_renderer->cleanup();
    std::exit(0);
}

int main() {
    Renderer renderer;
    g_renderer = &renderer;
    std::signal(SIGTERM, signal_handler);

    renderer.init();

    Editor editor;
    renderer.update_size(editor.vp);
    editor.ensure_visible();

    while (editor.running) {
        renderer.update_size(editor.vp);
        renderer.render(editor);
        int key = getch();
        editor.handle_key(key);
    }

    renderer.cleanup();
    return 0;
}

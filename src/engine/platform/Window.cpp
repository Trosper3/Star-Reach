#include "engine/platform/Window.h"

#include <raylib.h>

namespace sr::engine {

Window::~Window() {
    Close();
}

bool Window::Open(int width, int height, const std::string& title) {
    if (open_) {
        return true;
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(width, height, title.c_str());
    open_ = IsWindowReady();
    if (open_) {
        // The simulation owns its own pacing via FixedTimestep; capping here only bounds how
        // often we render and how large a real delta can get.
        SetTargetFPS(144);
    }
    return open_;
}

void Window::Close() {
    if (!open_) {
        return;
    }
    CloseWindow();
    open_ = false;
}

bool Window::ShouldClose() const {
    return !open_ || WindowShouldClose();
}

float Window::FrameTime() const {
    return open_ ? GetFrameTime() : 0.0f;
}

void Window::BeginFrame() {
    BeginDrawing();
    ClearBackground(Color{6, 8, 14, 255});
}

void Window::EndFrame() {
    EndDrawing();
}

int Window::Width() const {
    return open_ ? GetScreenWidth() : 0;
}

int Window::Height() const {
    return open_ ? GetScreenHeight() : 0;
}

}  // namespace sr::engine

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
    // FLAG_VSYNC_HINT deliberately omitted: it ties glfwSwapBuffers to the display's vblank,
    // and some Intel UHD Graphics driver versions stall indefinitely on that wait on Windows
    // (first EndDrawing() never returns). SetTargetFPS below still caps the frame rate without
    // the driver-level blocking wait.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, title.c_str());
    // raylib's default exit key is KEY_ESCAPE, which would close the window out from under
    // modes/space/ui/SystemMenu.cpp's own Esc handling (open/close the pause menu) the instant
    // it's pressed -- the menu would never get a frame to draw. SystemMenu owns Esc instead.
    SetExitKey(KEY_NULL);
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

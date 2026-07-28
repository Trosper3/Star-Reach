#pragma once

#include <string>

// engine/ is hardware abstraction with ZERO game knowledge (architecture.md section 3).
//
// It does not include core/, shared/, modes/, or net/, and CI enforces that by grep as well as
// by the link graph. The direction of the dependency is the whole point: engine/ is the layer
// a second game could be built on, so nothing in it may know what a rig or a faction is.
//
// raylib is deliberately not included in this header. Keeping <raylib.h> inside Window.cpp
// means the rest of the codebase never sees Color, Vector2, or Texture2D by accident -- which
// is what stops raylib types from leaking into components and quietly making sr_shared
// unlinkable in a headless test.
namespace sr::engine {

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    // Returns false if the window could not be created.
    bool Open(int width, int height, const std::string& title);
    void Close();

    bool IsOpen() const { return open_; }
    bool ShouldClose() const;

    // Real seconds since the previous frame. Feed straight into FixedTimestep::Advance.
    float FrameTime() const;

    void BeginFrame();
    void EndFrame();

    int Width() const;
    int Height() const;

private:
    bool open_ = false;
};

}  // namespace sr::engine

#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace ply {

enum class Key : uint16_t {
    Unknown = 0,
    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Digits
    D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,
    // Specials
    Space, Enter, Escape, Tab, Backspace,
    // Arrows
    Left, Right, Up, Down,
    // Modifiers
    LeftShift, RightShift, LeftCtrl, RightCtrl, LeftAlt, RightAlt,
    // Zoom / misc
    Minus, Equals,
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
};

enum class MouseButton : uint8_t {
    Left = 1,
    Middle = 2,
    Right = 3,
};

/// Per-frame keyboard/mouse state. Call update() once at the top of each frame.
class Input {
public:
    void update();

    /// True once the window is closed or Escape is pressed.
    bool quitRequested() const;

    bool isKeyDown(Key key) const;
    bool isKeyPressed(Key key) const;  // edge: pressed this frame

    bool isMouseDown(MouseButton button) const;
    bool isMousePressed(MouseButton button) const;

    /// Mouse position in window pixels, top-left origin.
    glm::vec2 mousePosition() const;

private:
    static constexpr int kKeyCount = 128;
    bool keyDown_[kKeyCount] = {};
    bool keyPressed_[kKeyCount] = {};
    bool mouseDown_[4] = {};
    bool mousePressed_[4] = {};
    glm::vec2 mousePos_{0.0f};
    bool quit_ = false;
};

}  // namespace ply

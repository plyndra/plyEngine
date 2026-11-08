#include "plyEngine/Input.hpp"

#include <SDL3/SDL.h>

namespace ply {

namespace {

Key keyFromSdl(SDL_Keycode k) {
    if (k >= SDLK_A && k <= SDLK_Z) {
        return static_cast<Key>(static_cast<int>(Key::A) + (k - SDLK_A));
    }
    if (k >= SDLK_0 && k <= SDLK_9) {
        return static_cast<Key>(static_cast<int>(Key::D0) + (k - SDLK_0));
    }
    switch (k) {
        case SDLK_SPACE: return Key::Space;
        case SDLK_RETURN: return Key::Enter;
        case SDLK_ESCAPE: return Key::Escape;
        case SDLK_TAB: return Key::Tab;
        case SDLK_BACKSPACE: return Key::Backspace;
        case SDLK_LEFT: return Key::Left;
        case SDLK_RIGHT: return Key::Right;
        case SDLK_UP: return Key::Up;
        case SDLK_DOWN: return Key::Down;
        case SDLK_LSHIFT: return Key::LeftShift;
        case SDLK_RSHIFT: return Key::RightShift;
        case SDLK_LCTRL: return Key::LeftCtrl;
        case SDLK_RCTRL: return Key::RightCtrl;
        case SDLK_LALT: return Key::LeftAlt;
        case SDLK_RALT: return Key::RightAlt;
        case SDLK_MINUS: return Key::Minus;
        case SDLK_KP_MINUS: return Key::Minus;
        case SDLK_EQUALS: return Key::Equals;
        case SDLK_PLUS: return Key::Equals;
        case SDLK_KP_PLUS: return Key::Equals;
        case SDLK_F1: return Key::F1;
        case SDLK_F2: return Key::F2;
        case SDLK_F3: return Key::F3;
        case SDLK_F4: return Key::F4;
        case SDLK_F5: return Key::F5;
        case SDLK_F6: return Key::F6;
        case SDLK_F7: return Key::F7;
        case SDLK_F8: return Key::F8;
        case SDLK_F9: return Key::F9;
        case SDLK_F10: return Key::F10;
        case SDLK_F11: return Key::F11;
        case SDLK_F12: return Key::F12;
        default: return Key::Unknown;
    }
}

}  // namespace

void Input::update() {
    for (int i = 0; i < kKeyCount; ++i) {
        keyPressed_[i] = false;
    }
    for (int i = 0; i < 4; ++i) {
        mousePressed_[i] = false;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                quit_ = true;
                break;
            case SDL_EVENT_KEY_DOWN: {
                if (event.key.key == SDLK_ESCAPE) {
                    quit_ = true;
                }
                Key key = keyFromSdl(event.key.key);
                if (key != Key::Unknown) {
                    const int idx = static_cast<int>(key);
                    if (!keyDown_[idx]) {
                        keyPressed_[idx] = true;
                    }
                    keyDown_[idx] = true;
                }
                break;
            }
            case SDL_EVENT_KEY_UP: {
                Key key = keyFromSdl(event.key.key);
                if (key != Key::Unknown) {
                    keyDown_[static_cast<int>(key)] = false;
                }
                break;
            }
            case SDL_EVENT_MOUSE_MOTION:
                mousePos_ = {event.motion.x, event.motion.y};
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                const int b = static_cast<int>(event.button.button);
                if (b >= 1 && b <= 3) {
                    if (!mouseDown_[b]) {
                        mousePressed_[b] = true;
                    }
                    mouseDown_[b] = true;
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                const int b = static_cast<int>(event.button.button);
                if (b >= 1 && b <= 3) {
                    mouseDown_[b] = false;
                }
                break;
            }
            default:
                break;
        }
    }
}

bool Input::quitRequested() const { return quit_; }

bool Input::isKeyDown(Key key) const { return keyDown_[static_cast<int>(key)]; }
bool Input::isKeyPressed(Key key) const { return keyPressed_[static_cast<int>(key)]; }

bool Input::isMouseDown(MouseButton button) const {
    return mouseDown_[static_cast<int>(button)];
}
bool Input::isMousePressed(MouseButton button) const {
    return mousePressed_[static_cast<int>(button)];
}

glm::vec2 Input::mousePosition() const { return mousePos_; }

}  // namespace ply

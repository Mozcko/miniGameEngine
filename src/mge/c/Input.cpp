// NOLINTBEGIN(readability-identifier-naming)
#include <mge/c/mge_c_internal.h>

#include <string>

namespace {

    mge::Key parseKeyName(const std::string& name) {
        if (name == "W" || name == "w") return mge::Key::W;
        if (name == "A" || name == "a") return mge::Key::A;
        if (name == "S" || name == "s") return mge::Key::S;
        if (name == "D" || name == "d") return mge::Key::D;
        if (name == "Space" || name == "space") return mge::Key::Space;
        if (name == "Escape" || name == "escape" || name == "Esc") return mge::Key::Escape;
        if (name == "Enter" || name == "Return") return mge::Key::Enter;
        if (name == "Tab") return mge::Key::Tab;
        if (name == "LeftShift" || name == "Shift") return mge::Key::LeftShift;
        if (name == "LeftCtrl" || name == "Ctrl") return mge::Key::LeftCtrl;
        if (name == "Up") return mge::Key::Up;
        if (name == "Down") return mge::Key::Down;
        if (name == "Left") return mge::Key::Left;
        if (name == "Right") return mge::Key::Right;
        if (name.size() == 1) {
            char c = name[0];
            if (c >= 'A' && c <= 'Z') return static_cast<mge::Key>(static_cast<int>(mge::Key::A) + (c - 'A'));
            if (c >= 'a' && c <= 'z') return static_cast<mge::Key>(static_cast<int>(mge::Key::A) + (c - 'a'));
            if (c >= '0' && c <= '9')
                return static_cast<mge::Key>(static_cast<int>(mge::Key::Num0) + (c - '0'));
        }
        return mge::Key::Unknown;
    }

    bool isMouseButtonName(const std::string& name, mge::MouseButton& outBtn) {
        if (name == "MouseLeft" || name == "mouse_left") {
            outBtn = mge::MouseButton::Left;
            return true;
        }
        if (name == "MouseRight" || name == "mouse_right") {
            outBtn = mge::MouseButton::Right;
            return true;
        }
        if (name == "MouseMiddle" || name == "mouse_middle") {
            outBtn = mge::MouseButton::Middle;
            return true;
        }
        return false;
    }

    bool checkKeyStatus(mge_app* app, const std::string& keyName, int checkType) {
        // checkType: 0 = down, 1 = pressed, 2 = released
        auto injIt = app->injectedKeys.find(keyName);
        if (injIt != app->injectedKeys.end() && injIt->second) {
            if (checkType == 0 || checkType == 1) return true;
        }

        if (app->platform) {
            mge::MouseButton btn;
            if (isMouseButtonName(keyName, btn)) {
                if (checkType == 0) return app->platform->input().isDown(btn);
                if (checkType == 1) return app->platform->input().wasPressed(btn);
                if (checkType == 2) return app->platform->input().wasReleased(btn);
            } else {
                mge::Key k = parseKeyName(keyName);
                if (k != mge::Key::Unknown) {
                    if (checkType == 0) return app->platform->input().isDown(k);
                    if (checkType == 1) return app->platform->input().wasPressed(k);
                    if (checkType == 2) return app->platform->input().wasReleased(k);
                }
            }
        }
        return false;
    }

} // namespace

extern "C" {

mge_result mge_input_bind_action(mge_app* app, const char* action, const char* key_name) {
    if (!app || !action || !key_name) return MGE_ERROR_INVALID_ARGUMENT;
    app->actionBindings[action] = key_name;
    return MGE_OK;
}

mge_result mge_input_bind_axis(mge_app* app, const char* axis, const char* negative_key,
                               const char* positive_key) {
    if (!app || !axis || !negative_key || !positive_key) return MGE_ERROR_INVALID_ARGUMENT;
    app->axisBindings[axis] = {negative_key, positive_key};
    return MGE_OK;
}

bool mge_input_is_action_pressed(mge_app* app, const char* action) {
    if (!app || !action) return false;
    auto it = app->actionBindings.find(action);
    if (it == app->actionBindings.end()) return false;
    return checkKeyStatus(app, it->second, 1);
}

bool mge_input_is_action_down(mge_app* app, const char* action) {
    if (!app || !action) return false;
    auto it = app->actionBindings.find(action);
    if (it == app->actionBindings.end()) return false;
    return checkKeyStatus(app, it->second, 0);
}

bool mge_input_is_action_released(mge_app* app, const char* action) {
    if (!app || !action) return false;
    auto it = app->actionBindings.find(action);
    if (it == app->actionBindings.end()) return false;
    return checkKeyStatus(app, it->second, 2);
}

float mge_input_get_axis(mge_app* app, const char* axis) {
    if (!app || !axis) return 0.0f;
    auto it = app->axisBindings.find(axis);
    if (it == app->axisBindings.end()) return 0.0f;

    float val = 0.0f;
    if (checkKeyStatus(app, it->second.posKey, 0)) val += 1.0f;
    if (checkKeyStatus(app, it->second.negKey, 0)) val -= 1.0f;
    return val;
}

void mge_input_mouse_delta(mge_app* app, float* dx, float* dy) {
    if (!app) {
        if (dx) *dx = 0.0f;
        if (dy) *dy = 0.0f;
        return;
    }
    if (dx) *dx = app->mouseDelta.x;
    if (dy) *dy = app->mouseDelta.y;
}

void mge_input_set_cursor_lock(mge_app* app, bool locked) {
    if (!app || !app->platform) return;
    app->platform->setCursorLocked(locked);
}

void mge_test_inject_key(mge_app* app, const char* key_name, bool down) {
    if (!app || !key_name) return;
    app->injectedKeys[key_name] = down;
    if (app->platform) {
        mge::Key k = parseKeyName(key_name);
        if (k != mge::Key::Unknown) {
            app->platform->input().setKey(k, down);
        }
    }
}

} // extern "C"
// NOLINTEND(readability-identifier-naming)

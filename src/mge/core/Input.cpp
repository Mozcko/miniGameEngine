#include <mge/core/Input.h>

#include <algorithm>

namespace mge {

    namespace {
        std::size_t index(Key key) {
            return static_cast<std::size_t>(key);
        }
        std::size_t index(MouseButton button) {
            return static_cast<std::size_t>(button);
        }
    }

    // --- InputState ---

    void InputState::beginFrame() {
        m_previousKeys = m_keys;
        m_previousButtons = m_buttons;
        m_mouseDelta = {};
        m_wheel = 0.0f;
    }

    void InputState::setKey(Key key, bool down) {
        if (key == Key::Unknown || key == Key::Count) return;
        m_keys[index(key)] = down;
    }

    void InputState::setMouseButton(MouseButton button, bool down) {
        if (button == MouseButton::Count) return;
        m_buttons[index(button)] = down;
    }

    void InputState::setMousePosition(float x, float y) {
        m_mousePosition = {x, y};
    }

    void InputState::addMouseDelta(float dx, float dy) {
        m_mouseDelta.x += dx;
        m_mouseDelta.y += dy;
    }

    void InputState::addWheel(float amount) {
        m_wheel += amount;
    }

    void InputState::clear() {
        m_keys.fill(false);
        m_buttons.fill(false);
        m_mouseDelta = {};
        m_wheel = 0.0f;
    }

    bool InputState::isDown(Key key) const {
        return key < Key::Count && m_keys[index(key)];
    }
    bool InputState::wasPressed(Key key) const {
        return key < Key::Count && m_keys[index(key)] && !m_previousKeys[index(key)];
    }
    bool InputState::wasReleased(Key key) const {
        return key < Key::Count && !m_keys[index(key)] && m_previousKeys[index(key)];
    }

    bool InputState::isDown(MouseButton button) const {
        return button < MouseButton::Count && m_buttons[index(button)];
    }
    bool InputState::wasPressed(MouseButton button) const {
        return button < MouseButton::Count && m_buttons[index(button)] && !m_previousButtons[index(button)];
    }
    bool InputState::wasReleased(MouseButton button) const {
        return button < MouseButton::Count && !m_buttons[index(button)] && m_previousButtons[index(button)];
    }

    // --- ActionMap ---

    void ActionMap::bind(const std::string& action, Key key) {
        m_actions[action].push_back({true, key, MouseButton::Left});
    }

    void ActionMap::bind(const std::string& action, MouseButton button) {
        m_actions[action].push_back({false, Key::Unknown, button});
    }

    void ActionMap::bindAxis(const std::string& axis, Key positive, Key negative) {
        m_axes[axis].push_back({positive, negative});
    }

    void ActionMap::clear() {
        m_actions.clear();
        m_axes.clear();
    }

    const std::vector<ActionMap::Binding>* ActionMap::find(const std::string& action) const {
        auto it = m_actions.find(action);
        return it == m_actions.end() ? nullptr : &it->second;
    }

    bool ActionMap::isDown(const InputState& input, const std::string& action) const {
        const auto* bindings = find(action);
        if (!bindings) return false;
        return std::any_of(bindings->begin(), bindings->end(), [&](const Binding& b) {
            return b.isKey ? input.isDown(b.key) : input.isDown(b.button);
        });
    }

    bool ActionMap::wasPressed(const InputState& input, const std::string& action) const {
        // Presionada en este frame por algún binding, y ningún otro binding ya la tenía apretada
        const auto* bindings = find(action);
        if (!bindings) return false;
        bool pressedNow = false;
        bool heldBefore = false;
        for (const Binding& b : *bindings) {
            bool pressed = b.isKey ? input.wasPressed(b.key) : input.wasPressed(b.button);
            bool down = b.isKey ? input.isDown(b.key) : input.isDown(b.button);
            pressedNow = pressedNow || pressed;
            heldBefore = heldBefore || (down && !pressed);
        }
        return pressedNow && !heldBefore;
    }

    bool ActionMap::wasReleased(const InputState& input, const std::string& action) const {
        // Soltada en este frame y ningún binding sigue apretado
        const auto* bindings = find(action);
        if (!bindings) return false;
        bool releasedNow = false;
        for (const Binding& b : *bindings) {
            if (b.isKey ? input.isDown(b.key) : input.isDown(b.button)) return false;
            releasedNow = releasedNow || (b.isKey ? input.wasReleased(b.key) : input.wasReleased(b.button));
        }
        return releasedNow;
    }

    float ActionMap::axis(const InputState& input, const std::string& axis) const {
        auto it = m_axes.find(axis);
        if (it == m_axes.end()) return 0.0f;
        float value = 0.0f;
        for (const AxisBinding& b : it->second) {
            if (input.isDown(b.positive)) value += 1.0f;
            if (input.isDown(b.negative)) value -= 1.0f;
        }
        return std::clamp(value, -1.0f, 1.0f);
    }
}

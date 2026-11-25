#include <mge/core/Input.h>

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>

namespace mge {

    namespace {
        std::size_t index(Key key) {
            return static_cast<std::size_t>(key);
        }
        std::size_t index(MouseButton button) {
            return static_cast<std::size_t>(button);
        }
        std::size_t index(GamepadButton button) {
            return static_cast<std::size_t>(button);
        }
        std::size_t index(GamepadAxis axis) {
            return static_cast<std::size_t>(axis);
        }

        bool isBindingDown(const InputState& input, const InputBinding& binding) {
            switch (binding.type) {
            case InputBindingType::Key: return input.isDown(binding.key);
            case InputBindingType::MouseButton: return input.isDown(binding.mouseButton);
            case InputBindingType::GamepadButton: return input.isDown(binding.gamepadButton, binding.gamepad);
            }
            return false;
        }

        bool wasBindingPressed(const InputState& input, const InputBinding& binding) {
            switch (binding.type) {
            case InputBindingType::Key: return input.wasPressed(binding.key);
            case InputBindingType::MouseButton: return input.wasPressed(binding.mouseButton);
            case InputBindingType::GamepadButton:
                return input.wasPressed(binding.gamepadButton, binding.gamepad);
            }
            return false;
        }

        bool wasBindingReleased(const InputState& input, const InputBinding& binding) {
            switch (binding.type) {
            case InputBindingType::Key: return input.wasReleased(binding.key);
            case InputBindingType::MouseButton: return input.wasReleased(binding.mouseButton);
            case InputBindingType::GamepadButton:
                return input.wasReleased(binding.gamepadButton, binding.gamepad);
            }
            return false;
        }

        bool isSpace(char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        std::string trim(const std::string& text) {
            std::size_t start = 0;
            while (start < text.size() && isSpace(text[start])) ++start;
            std::size_t end = text.size();
            while (end > start && isSpace(text[end - 1])) --end;
            return text.substr(start, end - start);
        }

        bool parseGamepadPrefix(const std::string& prefix, std::size_t& outSlot) {
            constexpr const char* kTag = "Gamepad";
            constexpr std::size_t kTagLen = 7;
            if (prefix.compare(0, kTagLen, kTag) != 0) return false;
            if (prefix.size() == kTagLen) {
                outSlot = 0;
                return true;
            }
            std::size_t slot = 0;
            for (std::size_t i = kTagLen; i < prefix.size(); ++i) {
                char c = prefix[i];
                if (c < '0' || c > '9') return false;
                slot = slot * 10 + static_cast<std::size_t>(c - '0');
                if (slot >= InputState::kMaxGamepads) return false;
            }
            outSlot = slot;
            return true;
        }

        bool parseFloat(const std::string& text, float& outValue) {
            std::istringstream iss(text);
            iss.imbue(std::locale::classic());
            float value = 0.0f;
            iss >> value;
            if (iss.fail()) return false;
            char extra = '\0';
            if (iss >> extra) return false;
            outValue = value;
            return true;
        }
    }

    // --- Deadzone helpers ---

    float applyAxisDeadzone(float value, float deadzone, float exponent) {
        const float clampedDeadzone = std::clamp(deadzone, 0.0f, 0.99f);
        const float magnitude = std::fabs(value);
        if (magnitude <= clampedDeadzone) return 0.0f;

        float rescaled = std::min(1.0f, (magnitude - clampedDeadzone) / (1.0f - clampedDeadzone));
        if (exponent > 0.0f && exponent != 1.0f) {
            rescaled = std::pow(rescaled, exponent);
        }
        return value < 0.0f ? -rescaled : rescaled;
    }

    Vec2f applyStickDeadzone(Vec2f stick, float deadzone, float exponent) {
        const float clampedDeadzone = std::clamp(deadzone, 0.0f, 0.99f);
        const float length = std::sqrt(stick.x * stick.x + stick.y * stick.y);
        if (length <= clampedDeadzone || length <= 0.0f) return {0.0f, 0.0f};

        float rescaled = std::min(1.0f, (length - clampedDeadzone) / (1.0f - clampedDeadzone));
        if (exponent > 0.0f && exponent != 1.0f) {
            rescaled = std::pow(rescaled, exponent);
        }
        const float factor = rescaled / length;
        return {stick.x * factor, stick.y * factor};
    }

    // --- InputState ---

    void InputState::beginFrame() {
        m_previousKeys = m_keys;
        m_previousButtons = m_buttons;
        m_mouseDelta = {};
        m_wheel = 0.0f;
        m_textInput.clear();
        for (GamepadState& pad : m_gamepads) {
            pad.previousButtons = pad.buttons;
        }
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

    void InputState::setGamepadConnected(std::size_t gamepad, bool connected, const std::string& name) {
        if (gamepad >= kMaxGamepads) return;
        GamepadState& pad = m_gamepads[gamepad];
        pad.connected = connected;
        if (connected) {
            pad.name = name;
        } else {
            pad.name.clear();
            pad.buttons.fill(false);
            pad.axes.fill(0.0f);
        }
    }

    void InputState::setGamepadButton(std::size_t gamepad, GamepadButton button, bool down) {
        if (gamepad >= kMaxGamepads || button == GamepadButton::Count) return;
        m_gamepads[gamepad].buttons[index(button)] = down;
    }

    void InputState::setGamepadAxis(std::size_t gamepad, GamepadAxis axis, float value) {
        if (gamepad >= kMaxGamepads || axis == GamepadAxis::Count) return;
        const bool isTrigger = (axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger);
        m_gamepads[gamepad].axes[index(axis)] = isTrigger ? std::clamp(value, 0.0f, 1.0f)
                                                          : std::clamp(value, -1.0f, 1.0f);
    }

    void InputState::setGamepadDeadzone(float deadzone) {
        m_gamepadDeadzone = std::clamp(deadzone, 0.0f, 0.99f);
    }

    void InputState::addTextInput(const std::string& utf8) {
        m_textInput.append(utf8);
    }

    void InputState::setTextComposition(const std::string& utf8, std::int32_t cursor,
                                        std::int32_t selectionLength) {
        m_textComposition.text = utf8;
        m_textComposition.cursor = cursor;
        m_textComposition.selectionLength = selectionLength;
    }

    void InputState::clearTextComposition() {
        m_textComposition = {};
    }

    void InputState::clear() {
        m_keys.fill(false);
        m_buttons.fill(false);
        m_mouseDelta = {};
        m_wheel = 0.0f;
        m_textInput.clear();
        m_textComposition = {};
        for (GamepadState& pad : m_gamepads) {
            pad.buttons.fill(false);
            pad.axes.fill(0.0f);
        }
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

    bool InputState::isGamepadConnected(std::size_t gamepad) const {
        return gamepad < kMaxGamepads && m_gamepads[gamepad].connected;
    }

    std::size_t InputState::connectedGamepadCount() const {
        std::size_t count = 0;
        for (const GamepadState& pad : m_gamepads) {
            if (pad.connected) ++count;
        }
        return count;
    }

    const std::string& InputState::gamepadName(std::size_t gamepad) const {
        static const std::string kEmpty;
        if (gamepad >= kMaxGamepads || !m_gamepads[gamepad].connected) return kEmpty;
        return m_gamepads[gamepad].name;
    }

    bool InputState::isDown(GamepadButton button, std::size_t gamepad) const {
        return gamepad < kMaxGamepads && button < GamepadButton::Count &&
               m_gamepads[gamepad].buttons[index(button)];
    }

    bool InputState::wasPressed(GamepadButton button, std::size_t gamepad) const {
        return gamepad < kMaxGamepads && button < GamepadButton::Count &&
               m_gamepads[gamepad].buttons[index(button)] &&
               !m_gamepads[gamepad].previousButtons[index(button)];
    }

    bool InputState::wasReleased(GamepadButton button, std::size_t gamepad) const {
        return gamepad < kMaxGamepads && button < GamepadButton::Count &&
               !m_gamepads[gamepad].buttons[index(button)] &&
               m_gamepads[gamepad].previousButtons[index(button)];
    }

    float InputState::gamepadAxisRaw(GamepadAxis axis, std::size_t gamepad) const {
        if (gamepad >= kMaxGamepads || axis == GamepadAxis::Count) return 0.0f;
        return m_gamepads[gamepad].axes[index(axis)];
    }

    float InputState::gamepadAxis(GamepadAxis axis, std::size_t gamepad) const {
        return applyAxisDeadzone(gamepadAxisRaw(axis, gamepad), m_gamepadDeadzone);
    }

    Vec2f InputState::gamepadStick(bool left, std::size_t gamepad) const {
        if (gamepad >= kMaxGamepads) return {0.0f, 0.0f};
        const GamepadAxis xAxis = left ? GamepadAxis::LeftX : GamepadAxis::RightX;
        const GamepadAxis yAxis = left ? GamepadAxis::LeftY : GamepadAxis::RightY;
        return applyStickDeadzone({gamepadAxisRaw(xAxis, gamepad), gamepadAxisRaw(yAxis, gamepad)},
                                  m_gamepadDeadzone);
    }

    // --- InputBinding & AxisBinding ---

    InputBinding InputBinding::fromKey(Key k) {
        InputBinding b;
        b.type = InputBindingType::Key;
        b.key = k;
        return b;
    }

    InputBinding InputBinding::fromMouse(MouseButton b) {
        InputBinding binding;
        binding.type = InputBindingType::MouseButton;
        binding.mouseButton = b;
        return binding;
    }

    InputBinding InputBinding::fromGamepad(GamepadButton b, std::size_t pad) {
        InputBinding binding;
        binding.type = InputBindingType::GamepadButton;
        binding.gamepadButton = b;
        binding.gamepad = pad;
        return binding;
    }

    bool InputBinding::operator==(const InputBinding& other) const {
        if (type != other.type) return false;
        switch (type) {
        case InputBindingType::Key: return key == other.key;
        case InputBindingType::MouseButton: return mouseButton == other.mouseButton;
        case InputBindingType::GamepadButton:
            return gamepadButton == other.gamepadButton && gamepad == other.gamepad;
        }
        return false;
    }

    AxisBinding AxisBinding::fromKeys(Key pos, Key neg) {
        AxisBinding b;
        b.type = AxisBindingType::DigitalKeys;
        b.positive = pos;
        b.negative = neg;
        return b;
    }

    AxisBinding AxisBinding::fromGamepadAxis(GamepadAxis ax, float axisScale, std::size_t pad) {
        AxisBinding b;
        b.type = AxisBindingType::GamepadAxis;
        b.gamepadAxis = ax;
        b.gamepad = pad;
        b.scale = axisScale;
        return b;
    }

    bool AxisBinding::operator==(const AxisBinding& other) const {
        if (type != other.type) return false;
        if (type == AxisBindingType::DigitalKeys) {
            return positive == other.positive && negative == other.negative;
        }
        return gamepadAxis == other.gamepadAxis && gamepad == other.gamepad && scale == other.scale;
    }

    // --- Name conversion & parsing ---

    const char* keyName(Key key) {
        switch (key) {
        case Key::A: return "A";
        case Key::B: return "B";
        case Key::C: return "C";
        case Key::D: return "D";
        case Key::E: return "E";
        case Key::F: return "F";
        case Key::G: return "G";
        case Key::H: return "H";
        case Key::I: return "I";
        case Key::J: return "J";
        case Key::K: return "K";
        case Key::L: return "L";
        case Key::M: return "M";
        case Key::N: return "N";
        case Key::O: return "O";
        case Key::P: return "P";
        case Key::Q: return "Q";
        case Key::R: return "R";
        case Key::S: return "S";
        case Key::T: return "T";
        case Key::U: return "U";
        case Key::V: return "V";
        case Key::W: return "W";
        case Key::X: return "X";
        case Key::Y: return "Y";
        case Key::Z: return "Z";
        case Key::Num0: return "Num0";
        case Key::Num1: return "Num1";
        case Key::Num2: return "Num2";
        case Key::Num3: return "Num3";
        case Key::Num4: return "Num4";
        case Key::Num5: return "Num5";
        case Key::Num6: return "Num6";
        case Key::Num7: return "Num7";
        case Key::Num8: return "Num8";
        case Key::Num9: return "Num9";
        case Key::Space: return "Space";
        case Key::Enter: return "Enter";
        case Key::Escape: return "Escape";
        case Key::Tab: return "Tab";
        case Key::Backspace: return "Backspace";
        case Key::LeftShift: return "LeftShift";
        case Key::RightShift: return "RightShift";
        case Key::LeftCtrl: return "LeftCtrl";
        case Key::RightCtrl: return "RightCtrl";
        case Key::LeftAlt: return "LeftAlt";
        case Key::RightAlt: return "RightAlt";
        case Key::Up: return "Up";
        case Key::Down: return "Down";
        case Key::Left: return "Left";
        case Key::Right: return "Right";
        case Key::F1: return "F1";
        case Key::F2: return "F2";
        case Key::F3: return "F3";
        case Key::F4: return "F4";
        case Key::F5: return "F5";
        case Key::F6: return "F6";
        case Key::F7: return "F7";
        case Key::F8: return "F8";
        case Key::F9: return "F9";
        case Key::F10: return "F10";
        case Key::F11: return "F11";
        case Key::F12: return "F12";
        case Key::Unknown:
        case Key::Count: return "Unknown";
        }
        return "Unknown";
    }

    Key keyFromName(const std::string& name) {
        const std::size_t count = static_cast<std::size_t>(Key::Count);
        for (std::size_t i = 1; i < count; ++i) {
            const Key k = static_cast<Key>(i);
            if (name == keyName(k)) return k;
        }
        return Key::Unknown;
    }

    const char* mouseButtonName(MouseButton button) {
        switch (button) {
        case MouseButton::Left: return "Left";
        case MouseButton::Middle: return "Middle";
        case MouseButton::Right: return "Right";
        case MouseButton::X1: return "X1";
        case MouseButton::X2: return "X2";
        case MouseButton::Count: return "Unknown";
        }
        return "Unknown";
    }

    MouseButton mouseButtonFromName(const std::string& name) {
        const std::size_t count = static_cast<std::size_t>(MouseButton::Count);
        for (std::size_t i = 0; i < count; ++i) {
            const MouseButton b = static_cast<MouseButton>(i);
            if (name == mouseButtonName(b)) return b;
        }
        return MouseButton::Count;
    }

    const char* gamepadButtonName(GamepadButton button) {
        switch (button) {
        case GamepadButton::South: return "South";
        case GamepadButton::East: return "East";
        case GamepadButton::West: return "West";
        case GamepadButton::North: return "North";
        case GamepadButton::Back: return "Back";
        case GamepadButton::Guide: return "Guide";
        case GamepadButton::Start: return "Start";
        case GamepadButton::LeftStick: return "LeftStick";
        case GamepadButton::RightStick: return "RightStick";
        case GamepadButton::LeftShoulder: return "LeftShoulder";
        case GamepadButton::RightShoulder: return "RightShoulder";
        case GamepadButton::DpadUp: return "DpadUp";
        case GamepadButton::DpadDown: return "DpadDown";
        case GamepadButton::DpadLeft: return "DpadLeft";
        case GamepadButton::DpadRight: return "DpadRight";
        case GamepadButton::Count: return "Unknown";
        }
        return "Unknown";
    }

    GamepadButton gamepadButtonFromName(const std::string& name) {
        const std::size_t count = static_cast<std::size_t>(GamepadButton::Count);
        for (std::size_t i = 0; i < count; ++i) {
            const GamepadButton b = static_cast<GamepadButton>(i);
            if (name == gamepadButtonName(b)) return b;
        }
        return GamepadButton::Count;
    }

    const char* gamepadAxisName(GamepadAxis axis) {
        switch (axis) {
        case GamepadAxis::LeftX: return "LeftX";
        case GamepadAxis::LeftY: return "LeftY";
        case GamepadAxis::RightX: return "RightX";
        case GamepadAxis::RightY: return "RightY";
        case GamepadAxis::LeftTrigger: return "LeftTrigger";
        case GamepadAxis::RightTrigger: return "RightTrigger";
        case GamepadAxis::Count: return "Unknown";
        }
        return "Unknown";
    }

    GamepadAxis gamepadAxisFromName(const std::string& name) {
        const std::size_t count = static_cast<std::size_t>(GamepadAxis::Count);
        for (std::size_t i = 0; i < count; ++i) {
            const GamepadAxis a = static_cast<GamepadAxis>(i);
            if (name == gamepadAxisName(a)) return a;
        }
        return GamepadAxis::Count;
    }

    std::string bindingToString(const InputBinding& binding) {
        switch (binding.type) {
        case InputBindingType::Key: return std::string("Key:") + keyName(binding.key);
        case InputBindingType::MouseButton:
            return std::string("Mouse:") + mouseButtonName(binding.mouseButton);
        case InputBindingType::GamepadButton:
            return "Gamepad" + std::to_string(binding.gamepad) + ":" +
                   gamepadButtonName(binding.gamepadButton);
        }
        return {};
    }

    std::optional<InputBinding> bindingFromString(const std::string& text) {
        const std::string cleaned = trim(text);
        const std::size_t colon = cleaned.find(':');
        if (colon == std::string::npos) return std::nullopt;

        const std::string prefix = cleaned.substr(0, colon);
        const std::string value = cleaned.substr(colon + 1);

        if (prefix == "Key") {
            const Key k = keyFromName(value);
            if (k == Key::Unknown) return std::nullopt;
            return InputBinding::fromKey(k);
        }
        if (prefix == "Mouse") {
            const MouseButton b = mouseButtonFromName(value);
            if (b == MouseButton::Count) return std::nullopt;
            return InputBinding::fromMouse(b);
        }
        std::size_t slot = 0;
        if (parseGamepadPrefix(prefix, slot)) {
            const GamepadButton b = gamepadButtonFromName(value);
            if (b == GamepadButton::Count) return std::nullopt;
            return InputBinding::fromGamepad(b, slot);
        }
        return std::nullopt;
    }

    std::optional<InputBinding> capturePressedInput(const InputState& input) {
        const std::size_t keyCount = static_cast<std::size_t>(Key::Count);
        for (std::size_t i = 1; i < keyCount; ++i) {
            const Key k = static_cast<Key>(i);
            if (input.wasPressed(k)) return InputBinding::fromKey(k);
        }
        const std::size_t mouseCount = static_cast<std::size_t>(MouseButton::Count);
        for (std::size_t i = 0; i < mouseCount; ++i) {
            const MouseButton b = static_cast<MouseButton>(i);
            if (input.wasPressed(b)) return InputBinding::fromMouse(b);
        }
        const std::size_t padButtonCount = static_cast<std::size_t>(GamepadButton::Count);
        for (std::size_t pad = 0; pad < InputState::kMaxGamepads; ++pad) {
            for (std::size_t i = 0; i < padButtonCount; ++i) {
                const GamepadButton b = static_cast<GamepadButton>(i);
                if (input.wasPressed(b, pad)) return InputBinding::fromGamepad(b, pad);
            }
        }
        return std::nullopt;
    }

    // --- ActionMap ---

    void ActionMap::bind(const std::string& action, Key key) {
        bind(action, InputBinding::fromKey(key));
    }

    void ActionMap::bind(const std::string& action, MouseButton button) {
        bind(action, InputBinding::fromMouse(button));
    }

    void ActionMap::bind(const std::string& action, GamepadButton button, std::size_t gamepad) {
        bind(action, InputBinding::fromGamepad(button, gamepad));
    }

    void ActionMap::bind(const std::string& action, const InputBinding& binding) {
        m_actions[action].push_back(binding);
    }

    void ActionMap::bindAxis(const std::string& axis, Key positive, Key negative) {
        bindAxis(axis, AxisBinding::fromKeys(positive, negative));
    }

    void ActionMap::bindAxis(const std::string& axis, GamepadAxis gamepadAxis, float scale,
                             std::size_t gamepad) {
        bindAxis(axis, AxisBinding::fromGamepadAxis(gamepadAxis, scale, gamepad));
    }

    void ActionMap::bindAxis(const std::string& axis, const AxisBinding& binding) {
        m_axes[axis].push_back(binding);
    }

    void ActionMap::rebind(const std::string& action, const InputBinding& binding) {
        m_actions[action] = {binding};
    }

    void ActionMap::rebind(const std::string& action, std::size_t bindingIndex, const InputBinding& binding) {
        auto& list = m_actions[action];
        if (bindingIndex < list.size()) {
            list[bindingIndex] = binding;
        } else {
            list.push_back(binding);
        }
    }

    void ActionMap::unbind(const std::string& action, const InputBinding& binding) {
        auto it = m_actions.find(action);
        if (it == m_actions.end()) return;
        auto& list = it->second;
        list.erase(std::remove(list.begin(), list.end(), binding), list.end());
        if (list.empty()) m_actions.erase(it);
    }

    void ActionMap::clearAction(const std::string& action) {
        m_actions.erase(action);
    }

    void ActionMap::clearAxis(const std::string& axis) {
        m_axes.erase(axis);
    }

    void ActionMap::clear() {
        m_actions.clear();
        m_axes.clear();
    }

    std::vector<InputBinding> ActionMap::bindingsFor(const std::string& action) const {
        const auto* list = find(action);
        return list ? *list : std::vector<InputBinding>{};
    }

    std::vector<AxisBinding> ActionMap::axisBindingsFor(const std::string& axis) const {
        auto it = m_axes.find(axis);
        return it == m_axes.end() ? std::vector<AxisBinding>{} : it->second;
    }

    std::vector<std::string> ActionMap::actionNames() const {
        std::vector<std::string> names;
        names.reserve(m_actions.size());
        for (const auto& entry : m_actions) {
            names.push_back(entry.first);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    std::vector<std::string> ActionMap::axisNames() const {
        std::vector<std::string> names;
        names.reserve(m_axes.size());
        for (const auto& entry : m_axes) {
            names.push_back(entry.first);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    std::string ActionMap::serialize() const {
        std::ostringstream out;
        out.imbue(std::locale::classic());

        for (const std::string& name : actionNames()) {
            for (const InputBinding& b : m_actions.at(name)) {
                out << "action " << name << " = " << bindingToString(b) << '\n';
            }
        }
        for (const std::string& name : axisNames()) {
            for (const AxisBinding& b : m_axes.at(name)) {
                if (b.type == AxisBindingType::DigitalKeys) {
                    out << "axis " << name << " = Keys:" << keyName(b.positive) << ',' << keyName(b.negative)
                        << '\n';
                } else {
                    out << "axis " << name << " = Gamepad" << b.gamepad << ':'
                        << gamepadAxisName(b.gamepadAxis) << ',' << b.scale << '\n';
                }
            }
        }
        return out.str();
    }

    bool ActionMap::deserialize(const std::string& data) {
        std::unordered_map<std::string, std::vector<InputBinding>> newActions;
        std::unordered_map<std::string, std::vector<AxisBinding>> newAxes;

        std::istringstream in(data);
        std::string rawLine;
        while (std::getline(in, rawLine)) {
            const std::string line = trim(rawLine);
            if (line.empty() || line[0] == '#') continue;

            const std::size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) return false;

            const std::string lhs = trim(line.substr(0, eqPos));
            const std::string rhs = trim(line.substr(eqPos + 1));

            constexpr const char* kActionPrefix = "action ";
            constexpr const char* kAxisPrefix = "axis ";
            if (lhs.compare(0, 7, kActionPrefix) == 0) {
                const std::string name = trim(lhs.substr(7));
                if (name.empty()) return false;
                auto binding = bindingFromString(rhs);
                if (!binding) return false;
                newActions[name].push_back(*binding);
            } else if (lhs.compare(0, 5, kAxisPrefix) == 0) {
                const std::string name = trim(lhs.substr(5));
                if (name.empty()) return false;

                const std::size_t colon = rhs.find(':');
                if (colon == std::string::npos) return false;
                const std::string kind = rhs.substr(0, colon);
                const std::string payload = rhs.substr(colon + 1);

                if (kind == "Keys") {
                    const std::size_t comma = payload.find(',');
                    if (comma == std::string::npos) return false;
                    const Key pos = keyFromName(trim(payload.substr(0, comma)));
                    const Key neg = keyFromName(trim(payload.substr(comma + 1)));
                    if (pos == Key::Unknown || neg == Key::Unknown) return false;
                    newAxes[name].push_back(AxisBinding::fromKeys(pos, neg));
                } else {
                    std::size_t slot = 0;
                    if (!parseGamepadPrefix(kind, slot)) return false;
                    const std::size_t comma = payload.find(',');
                    const std::string axisStr =
                        trim(comma == std::string::npos ? payload : payload.substr(0, comma));
                    const GamepadAxis ga = gamepadAxisFromName(axisStr);
                    if (ga == GamepadAxis::Count) return false;

                    float axisScale = 1.0f;
                    if (comma != std::string::npos) {
                        if (!parseFloat(trim(payload.substr(comma + 1)), axisScale)) return false;
                    }
                    newAxes[name].push_back(AxisBinding::fromGamepadAxis(ga, axisScale, slot));
                }
            } else {
                return false;
            }
        }

        m_actions = std::move(newActions);
        m_axes = std::move(newAxes);
        return true;
    }

    const std::vector<InputBinding>* ActionMap::find(const std::string& action) const {
        auto it = m_actions.find(action);
        return it == m_actions.end() ? nullptr : &it->second;
    }

    bool ActionMap::isDown(const InputState& input, const std::string& action) const {
        const auto* bindings = find(action);
        if (!bindings) return false;
        return std::any_of(bindings->begin(), bindings->end(),
                           [&](const InputBinding& b) { return isBindingDown(input, b); });
    }

    bool ActionMap::wasPressed(const InputState& input, const std::string& action) const {
        // Pressed this frame by some binding, and no other binding was already holding it down
        const auto* bindings = find(action);
        if (!bindings) return false;
        bool pressedNow = false;
        bool heldBefore = false;
        for (const InputBinding& b : *bindings) {
            const bool pressed = wasBindingPressed(input, b);
            const bool down = isBindingDown(input, b);
            pressedNow = pressedNow || pressed;
            heldBefore = heldBefore || (down && !pressed);
        }
        return pressedNow && !heldBefore;
    }

    bool ActionMap::wasReleased(const InputState& input, const std::string& action) const {
        // Released this frame and no binding is still held down
        const auto* bindings = find(action);
        if (!bindings) return false;
        bool releasedNow = false;
        for (const InputBinding& b : *bindings) {
            if (isBindingDown(input, b)) return false;
            releasedNow = releasedNow || wasBindingReleased(input, b);
        }
        return releasedNow;
    }

    float ActionMap::axis(const InputState& input, const std::string& axis) const {
        auto it = m_axes.find(axis);
        if (it == m_axes.end()) return 0.0f;
        float value = 0.0f;
        for (const AxisBinding& b : it->second) {
            if (b.type == AxisBindingType::DigitalKeys) {
                if (input.isDown(b.positive)) value += 1.0f;
                if (input.isDown(b.negative)) value -= 1.0f;
            } else {
                value += input.gamepadAxis(b.gamepadAxis, b.gamepad) * b.scale;
            }
        }
        return std::clamp(value, -1.0f, 1.0f);
    }
}

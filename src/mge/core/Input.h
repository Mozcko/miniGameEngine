#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mge {

    // Physical keys (keyboard position, not character). Independent of SDL: the platform module
    // translates its events to these. Tests can inject input directly.
    enum class Key : std::uint16_t {
        Unknown = 0,
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        Num0,
        Num1,
        Num2,
        Num3,
        Num4,
        Num5,
        Num6,
        Num7,
        Num8,
        Num9,
        Space,
        Enter,
        Escape,
        Tab,
        Backspace,
        LeftShift,
        RightShift,
        LeftCtrl,
        RightCtrl,
        LeftAlt,
        RightAlt,
        Up,
        Down,
        Left,
        Right,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        Count
    };

    enum class MouseButton : std::uint8_t {
        Left = 0,
        Middle,
        Right,
        X1,
        X2,
        Count
    };

    // Standard console-style gamepad buttons (positions independent of controller brand).
    enum class GamepadButton : std::uint8_t {
        South = 0, // Bottom face button (Xbox A, PlayStation Cross)
        East,      // Right face button (Xbox B, PlayStation Circle)
        West,      // Left face button (Xbox X, PlayStation Square)
        North,     // Top face button (Xbox Y, PlayStation Triangle)
        Back,
        Guide,
        Start,
        LeftStick,
        RightStick,
        LeftShoulder,
        RightShoulder,
        DpadUp,
        DpadDown,
        DpadLeft,
        DpadRight,
        Count
    };

    // Standard gamepad analog axes. Sticks are in [-1, 1]; triggers are in [0, 1].
    enum class GamepadAxis : std::uint8_t {
        LeftX = 0,
        LeftY,
        RightX,
        RightY,
        LeftTrigger,
        RightTrigger,
        Count
    };

    struct Vec2f {
        float x = 0.0f;
        float y = 0.0f;
    };

    // IME pre-edit composition text (UTF-8) while the user is composing characters.
    struct TextComposition {
        std::string text;
        std::int32_t cursor = 0;          // Start cursor in UTF-8 characters
        std::int32_t selectionLength = 0; // Length of selected text in UTF-8 characters

        bool active() const { return !text.empty(); }
    };

    // Keyboard, mouse, gamepad, and text input state for the current frame, with per-frame edges.
    class InputState {
    public:
        static constexpr std::size_t kMaxGamepads = 4;

        // Call at the start of each frame, BEFORE applying the frame's events
        void beginFrame();

        // --- Input (called by the platform backend or a test) ---
        void setKey(Key key, bool down);
        void setMouseButton(MouseButton button, bool down);
        void setMousePosition(float x, float y);
        void addMouseDelta(float dx, float dy);
        void addWheel(float amount);

        // Gamepad input
        void setGamepadConnected(std::size_t gamepad, bool connected, const std::string& name = {});
        void setGamepadButton(std::size_t gamepad, GamepadButton button, bool down);
        void setGamepadAxis(std::size_t gamepad, GamepadAxis axis, float value);

        // Text input (UTF-8)
        void addTextInput(const std::string& utf8);
        void setTextComposition(const std::string& utf8, std::int32_t cursor = 0,
                                std::int32_t selectionLength = 0);
        void clearTextComposition();

        // Release everything (e.g., window lost focus, to avoid "stuck" keys)
        void clear();

        // --- Queries ---
        bool isDown(Key key) const;
        bool wasPressed(Key key) const;  // Went down this frame
        bool wasReleased(Key key) const; // Went up this frame
        bool isDown(MouseButton button) const;
        bool wasPressed(MouseButton button) const;
        bool wasReleased(MouseButton button) const;

        Vec2f mousePosition() const { return m_mousePosition; } // Pixels, y pointing down
        Vec2f mouseDelta() const { return m_mouseDelta; }       // Accumulated movement this frame
        float wheel() const { return m_wheel; }                 // Accumulated wheel this frame (+ = up)

        // Gamepad queries
        bool isGamepadConnected(std::size_t gamepad = 0) const;
        std::size_t connectedGamepadCount() const;
        const std::string& gamepadName(std::size_t gamepad = 0) const;
        bool isDown(GamepadButton button, std::size_t gamepad = 0) const;
        bool wasPressed(GamepadButton button, std::size_t gamepad = 0) const;
        bool wasReleased(GamepadButton button, std::size_t gamepad = 0) const;
        float gamepadAxisRaw(GamepadAxis axis, std::size_t gamepad = 0) const;
        float gamepadAxis(GamepadAxis axis, std::size_t gamepad = 0) const;
        Vec2f gamepadStick(bool left, std::size_t gamepad = 0) const;

        // Text input queries (committed UTF-8 text received this frame, plus active IME composition)
        const std::string& textInput() const { return m_textInput; }
        const TextComposition& textComposition() const { return m_textComposition; }

    private:
        static constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::Count);
        static constexpr std::size_t kButtonCount = static_cast<std::size_t>(MouseButton::Count);
        static constexpr std::size_t kGamepadButtonCount = static_cast<std::size_t>(GamepadButton::Count);
        static constexpr std::size_t kGamepadAxisCount = static_cast<std::size_t>(GamepadAxis::Count);

        struct GamepadState {
            bool connected = false;
            std::string name;
            std::array<bool, kGamepadButtonCount> buttons{};
            std::array<bool, kGamepadButtonCount> previousButtons{};
            std::array<float, kGamepadAxisCount> axes{};
        };

        std::array<bool, kKeyCount> m_keys{};
        std::array<bool, kKeyCount> m_previousKeys{};
        std::array<bool, kButtonCount> m_buttons{};
        std::array<bool, kButtonCount> m_previousButtons{};
        Vec2f m_mousePosition;
        Vec2f m_mouseDelta;
        float m_wheel = 0.0f;

        std::array<GamepadState, kMaxGamepads> m_gamepads{};

        std::string m_textInput;
        TextComposition m_textComposition;
    };

    enum class InputBindingType : std::uint8_t {
        Key = 0,
        MouseButton,
        GamepadButton
    };

    // A single digital input binding (keyboard key, mouse button, or gamepad button).
    struct InputBinding {
        InputBindingType type = InputBindingType::Key;
        Key key = Key::Unknown;
        MouseButton mouseButton = MouseButton::Left;
        GamepadButton gamepadButton = GamepadButton::South;
        std::size_t gamepad = 0;

        static InputBinding fromKey(Key k);
        static InputBinding fromMouse(MouseButton b);
        static InputBinding fromGamepad(GamepadButton b, std::size_t pad = 0);

        bool operator==(const InputBinding& other) const;
        bool operator!=(const InputBinding& other) const { return !(*this == other); }
    };

    enum class AxisBindingType : std::uint8_t {
        DigitalKeys = 0,
        GamepadAxis
    };

    // An axis binding: either a positive/negative key pair or an analog gamepad axis.
    struct AxisBinding {
        AxisBindingType type = AxisBindingType::DigitalKeys;
        Key positive = Key::Unknown;
        Key negative = Key::Unknown;
        GamepadAxis gamepadAxis = GamepadAxis::LeftX;
        std::size_t gamepad = 0;
        float scale = 1.0f;

        static AxisBinding fromKeys(Key pos, Key neg);
        static AxisBinding fromGamepadAxis(GamepadAxis ax, float axisScale = 1.0f, std::size_t pad = 0);

        bool operator==(const AxisBinding& other) const;
        bool operator!=(const AxisBinding& other) const { return !(*this == other); }
    };

    // Human-readable names and parsing for rebinding UI and serialization.
    const char* keyName(Key key);
    Key keyFromName(const std::string& name);
    const char* mouseButtonName(MouseButton button);
    MouseButton mouseButtonFromName(const std::string& name);
    const char* gamepadButtonName(GamepadButton button);
    GamepadButton gamepadButtonFromName(const std::string& name);
    const char* gamepadAxisName(GamepadAxis axis);
    GamepadAxis gamepadAxisFromName(const std::string& name);

    std::string bindingToString(const InputBinding& binding);
    std::optional<InputBinding> bindingFromString(const std::string& text);

    // Returns the first key, mouse button, or gamepad button pressed this frame (for rebind prompts).
    std::optional<InputBinding> capturePressedInput(const InputState& input);

    // Named actions and axes ("Jump", "MoveForward") bound to keys, mouse buttons, or gamepad controls.
    // The game queries actions; rebinding controls only requires changing the bindings.
    class ActionMap {
    public:
        void bind(const std::string& action, Key key);
        void bind(const std::string& action, MouseButton button);
        void bind(const std::string& action, GamepadButton button, std::size_t gamepad = 0);
        void bind(const std::string& action, const InputBinding& binding);

        // Axis in [-1, 1]: positive adds +1, negative adds -1 (both = 0)
        void bindAxis(const std::string& axis, Key positive, Key negative);
        void bindAxis(const std::string& axis, GamepadAxis gamepadAxis, float scale = 1.0f,
                      std::size_t gamepad = 0);
        void bindAxis(const std::string& axis, const AxisBinding& binding);

        // Rebinding and removal
        void rebind(const std::string& action, const InputBinding& binding);
        void rebind(const std::string& action, std::size_t bindingIndex, const InputBinding& binding);
        void unbind(const std::string& action, const InputBinding& binding);
        void clearAction(const std::string& action);
        void clearAxis(const std::string& axis);
        void clear();

        // Introspection
        std::vector<InputBinding> bindingsFor(const std::string& action) const;
        std::vector<AxisBinding> axisBindingsFor(const std::string& axis) const;
        std::vector<std::string> actionNames() const;
        std::vector<std::string> axisNames() const;

        // Line-based serialization for saving/loading user control schemes
        std::string serialize() const;
        bool deserialize(const std::string& data);

        bool isDown(const InputState& input, const std::string& action) const;
        bool wasPressed(const InputState& input, const std::string& action) const;
        bool wasReleased(const InputState& input, const std::string& action) const;
        float axis(const InputState& input, const std::string& axis) const;

    private:
        const std::vector<InputBinding>* find(const std::string& action) const;

        std::unordered_map<std::string, std::vector<InputBinding>> m_actions;
        std::unordered_map<std::string, std::vector<AxisBinding>> m_axes;
    };
}

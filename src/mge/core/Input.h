#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace mge {

    // Teclas físicas (posición en el teclado, no el carácter). Independiente de SDL: el módulo de
    // plataforma traduce sus eventos a esto. Los tests pueden inyectar input directamente.
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

    struct Vec2f {
        float x = 0.0f;
        float y = 0.0f;
    };

    // Estado del teclado y el ratón para el frame actual, con flancos (pressed/released) por frame.
    class InputState {
    public:
        // Llamar al inicio de cada frame, ANTES de aplicar los eventos del frame
        void beginFrame();

        // --- Entrada (la llama el backend de plataforma o un test) ---
        void setKey(Key key, bool down);
        void setMouseButton(MouseButton button, bool down);
        void setMousePosition(float x, float y);
        void addMouseDelta(float dx, float dy);
        void addWheel(float amount);
        // Suelta todo (ej: la ventana perdió el foco, para no quedar con teclas "pegadas")
        void clear();

        // --- Consultas ---
        bool isDown(Key key) const;
        bool wasPressed(Key key) const;  // Bajó en este frame
        bool wasReleased(Key key) const; // Subió en este frame
        bool isDown(MouseButton button) const;
        bool wasPressed(MouseButton button) const;
        bool wasReleased(MouseButton button) const;

        Vec2f mousePosition() const { return m_mousePosition; } // Píxeles, y hacia abajo
        Vec2f mouseDelta() const { return m_mouseDelta; }       // Movimiento acumulado del frame
        float wheel() const { return m_wheel; }                 // Rueda acumulada del frame (+ = arriba)

    private:
        static constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::Count);
        static constexpr std::size_t kButtonCount = static_cast<std::size_t>(MouseButton::Count);

        std::array<bool, kKeyCount> m_keys{};
        std::array<bool, kKeyCount> m_previousKeys{};
        std::array<bool, kButtonCount> m_buttons{};
        std::array<bool, kButtonCount> m_previousButtons{};
        Vec2f m_mousePosition;
        Vec2f m_mouseDelta;
        float m_wheel = 0.0f;
    };

    // Acciones y ejes con nombre ("Jump", "MoveForward") enlazados a teclas o botones.
    // El juego consulta acciones; cambiar los controles es solo cambiar los bindings.
    class ActionMap {
    public:
        void bind(const std::string& action, Key key);
        void bind(const std::string& action, MouseButton button);
        // Eje en [-1, 1]: positive suma +1, negative suma -1 (ambas = 0)
        void bindAxis(const std::string& axis, Key positive, Key negative);
        void clear();

        bool isDown(const InputState& input, const std::string& action) const;
        bool wasPressed(const InputState& input, const std::string& action) const;
        bool wasReleased(const InputState& input, const std::string& action) const;
        float axis(const InputState& input, const std::string& axis) const;

    private:
        struct Binding {
            bool isKey;
            Key key;
            MouseButton button;
        };
        struct AxisBinding {
            Key positive;
            Key negative;
        };

        const std::vector<Binding>* find(const std::string& action) const;

        std::unordered_map<std::string, std::vector<Binding>> m_actions;
        std::unordered_map<std::string, std::vector<AxisBinding>> m_axes;
    };
}

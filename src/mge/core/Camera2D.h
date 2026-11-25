#pragma once

#include <mge/core/Random.h>

#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

namespace mge {

    // Axis-aligned 2D rectangle in world coordinates: [min, max]
    struct Bounds2D {
        glm::vec2 min{0.0f};
        glm::vec2 max{0.0f};

        glm::vec2 size() const { return max - min; }
        glm::vec2 center() const { return (min + max) * 0.5f; }
        bool empty() const { return min.x >= max.x || min.y >= max.y; }
        bool contains(const glm::vec2& point) const {
            return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
        }
        bool intersects(const Bounds2D& other) const {
            return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y &&
                   max.y >= other.min.y;
        }
    };

    // Headless 2D camera: world <-> screen transforms, zoom (with cursor-anchored zoom), optional rotation
    // and world bounds clamping, deadzone target following with frame-rate independent smoothing, and
    // deterministic trauma-based screen shake.
    //
    // Convention: screen space has (0, 0) at top-left, +X right, +Y down (matching Renderer2D / Batch2D).
    // position() is the world point mapped to the center of the viewport (viewport * 0.5).
    class Camera2D {
    public:
        static constexpr float kMinSafeZoom = 1e-4f;
        static constexpr std::uint64_t kDefaultShakeSeed = 0xC4EA2D5A1E70001ull;

        explicit Camera2D(const glm::vec2& position = glm::vec2(0.0f), float zoom = 1.0f);

        // --- Core state ---
        const glm::vec2& position() const { return m_position; }
        void setPosition(const glm::vec2& position) { m_position = position; }

        float zoom() const { return m_zoom; }
        void setZoom(float zoom);
        void zoomBy(float factor);
        // Zooms by factor while keeping the world point under screenPoint fixed on screen
        void zoomAt(float factor, const glm::vec2& screenPoint, const glm::vec2& viewport);

        float rotation() const { return m_rotation; } // Radians
        void setRotation(float radians) { m_rotation = radians; }

        // Effective pose including active screen shake (used by transforms and matrices)
        glm::vec2 effectivePosition() const { return m_position + m_shakeOffset; }
        float effectiveRotation() const { return m_rotation + m_shakeAngle; }

        // --- Transforms & matrices ---
        glm::vec2 worldToScreen(const glm::vec2& worldPos, const glm::vec2& viewport) const;
        glm::vec2 screenToWorld(const glm::vec2& screenPos, const glm::vec2& viewport) const;
        // Maps 2D world coordinates (z = 0) into screen pixels [0, viewport]
        glm::mat4 viewMatrix(const glm::vec2& viewport) const;
        // Orthographic (Z in [0, 1], top-left origin) * viewMatrix, ready for SDL GPU shaders
        glm::mat4 viewProjectionMatrix(const glm::vec2& viewport) const;
        // Axis-aligned world bounds enclosing the visible viewport (accounts for zoom, rotation, shake)
        Bounds2D visibleBounds(const glm::vec2& viewport) const;

        // --- World bounds clamping ---
        const std::optional<Bounds2D>& bounds() const { return m_bounds; }
        void setBounds(const Bounds2D& bounds) { m_bounds = bounds; }
        void clearBounds() { m_bounds.reset(); }
        // Clamps position() so the visible view stays inside bounds() (centers if bounds are smaller)
        void clampToBounds(const glm::vec2& viewport);

        // --- Target follow with deadzone ---
        // Moves position() toward target when target exits the deadzone centered on position().
        // Uses exponential damping when followSmoothing > 0, or snaps when followSmoothing <= 0.
        // dt <= 0 (paused) does nothing.
        void follow(const glm::vec2& target, float dt);

        // --- Screen shake (trauma-based, deterministic PCG32) ---
        void addTrauma(float amount); // Adds to trauma in [0, 1]; ignores amount <= 0
        void stopShake();
        void setShakeSeed(std::uint64_t seed) { m_rng.reseed(seed); }

        float trauma() const { return m_trauma; }
        const glm::vec2& shakeOffset() const { return m_shakeOffset; }
        float shakeAngle() const { return m_shakeAngle; }

        // Advances screen shake decay and updates shakeOffset() / shakeAngle(). dt <= 0 does nothing.
        void update(float dt);

        // --- Tuning parameters ---
        float minZoom = 0.05f;
        float maxZoom = 20.0f;
        // Half-size of the deadzone rectangle around position() in world units (0 = no deadzone)
        glm::vec2 deadzoneHalfSize{0.0f};
        // Exponential follow rate in 1/s (0 = instant snap; higher = faster convergence)
        float followSmoothing = 0.0f;
        // Maximum shake displacement in world units and angle in radians at trauma = 1
        glm::vec2 maxShakeOffset{16.0f, 16.0f};
        float maxShakeAngle = 0.0f;
        float shakeDecay = 1.5f; // Trauma lost per second

    private:
        float safeZoom() const;

        glm::vec2 m_position{0.0f};
        float m_zoom = 1.0f;
        float m_rotation = 0.0f;
        std::optional<Bounds2D> m_bounds;

        float m_trauma = 0.0f;
        glm::vec2 m_shakeOffset{0.0f};
        float m_shakeAngle = 0.0f;
        Pcg32 m_rng{kDefaultShakeSeed};
    };
}

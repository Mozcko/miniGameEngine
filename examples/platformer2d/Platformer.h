#pragma once

#include <examples/platformer2d/Level.h>

#include <vector>

#include <glm/glm.hpp>

namespace platformer {

    // What the player wants to do on this step
    struct PlayerInput {
        float move = 0.0f; // -1 left .. 1 right
        bool jump = false; // While held (jumps upon touching the ground)
    };

    // What happened during a step (for sounds and level transitions)
    struct StepEvents {
        int coins = 0;
        bool jumped = false;
        bool died = false;     // Fell into a pit or touched an enemy: respawns at start
        bool finished = false; // Reached the end of the level
    };

    // Platformer simulation (headless, testable): a box player against the level's solid tiles.
    // Units: tiles; y points down, matching the file rows. Fixed timestep (onFixedUpdate).
    class Platformer {
    public:
        static constexpr glm::vec2 kPlayerSize{0.75f, 0.9f};
        static constexpr float kRunSpeed = 7.0f;   // tiles/s
        static constexpr float kJumpSpeed = 13.0f; // tiles/s (≈ 2.4 tiles high)
        static constexpr float kGravity = 35.0f;   // tiles/s²
        static constexpr float kMaxFallSpeed = 20.0f;

        explicit Platformer(Level level);

        StepEvents step(const PlayerInput& input, float dt);
        void respawn();

        const Level& level() const { return m_level; }
        glm::vec2 playerPosition() const { return m_position; } // Top-left corner
        glm::vec2 playerVelocity() const { return m_velocity; }
        bool grounded() const { return m_grounded; }
        bool collected(std::size_t item) const { return m_collected[item]; }
        int coins() const { return m_coins; }
        int deaths() const { return m_deaths; }

    private:
        // Does the box (top-left, size) touch any solid tile?
        bool overlapsSolid(const glm::vec2& pos) const;
        // Moves along one axis and stops against tiles; returns true on collision
        bool moveAxis(int axis, float delta);

        Level m_level;
        glm::vec2 m_position{0.0f};
        glm::vec2 m_velocity{0.0f};
        bool m_grounded = false;
        std::vector<bool> m_collected;
        int m_coins = 0;
        int m_deaths = 0;
    };
}

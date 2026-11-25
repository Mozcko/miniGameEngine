#pragma once

#include <mge/synth/Music.h>
#include <mge/synth/Sfx.h>

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace sdf_glide {

    // --- Procedural canyon SDF functions (identical to glide.frag) ---

    // Deterministic per-section pseudorandom hash (32-bit, identical on CPU and GPU)
    uint32_t sectionHashU32(int32_t s, uint32_t salt);
    float sectionHash(int32_t s, uint32_t salt);

    // Analytical primitives equivalent to GLSL and mge::sdf
    float sdSphere(const glm::vec3& p, float radius);
    float sdBox(const glm::vec3& p, const glm::vec3& halfExtents);
    float sdTorus(const glm::vec3& p, float majorRadius, float minorRadius);
    float opSmoothUnion(float d1, float d2, float k);
    glm::vec3 opTwist(const glm::vec3& p, float radiansPerUnit);

    // Signed distance to the procedural canyon at any point p
    float canyonDistance(const glm::vec3& p);

    // Surface normal of the canyon (central-differences gradient)
    glm::vec3 canyonNormal(const glm::vec3& p, float eps = 1e-3f);

    // --- Entities and Game Logic ---

    struct Mote {
        glm::vec3 position{0.0f};
        bool collected = false;
        int section = 0;
    };

    struct GlideInput {
        float steerX = 0.0f; // -1 (left) .. +1 (right)
        float steerY = 0.0f; // -1 (down) .. +1 (up)
        bool boost = false;
        bool restart = false;
    };

    class GlideMouseSteering {
    public:
        void record(const glm::vec2& delta);
        void apply(GlideInput& input);

    private:
        glm::vec2 m_pendingDelta{0.0f};
    };

    enum class AudioEvent : std::uint8_t {
        None,
        CollectMote,
        HitWall,
        BoostStart,
        GameOver
    };

    class GlideGame {
    public:
        static constexpr float kSectionLength = 24.0f;
        static constexpr float kPlayerRadius = 0.38f;
        static constexpr float kOrbRadius = kPlayerRadius;
        static constexpr int kMaxShields = 3;
        static constexpr float kMaxEnergy = 100.0f;

        GlideGame();

        void reset();
        void update(float dt, const GlideInput& input);

        const glm::vec3& playerPosition() const { return m_playerPos; }
        const glm::vec3& previousPosition() const { return m_prevPlayerPos; }
        const glm::vec3& playerVelocity() const { return m_playerVel; }
        void setPlayerPosition(const glm::vec3& pos) {
            m_playerPos = pos;
            m_prevPlayerPos = pos;
        }
        void setPlayerVelocity(const glm::vec3& vel) { m_playerVel = vel; }
        int shields() const { return m_shields; }
        int score() const { return m_score; }
        int bestScore() const { return m_bestScore; }
        int combo() const { return m_combo; }
        float energy() const { return m_energy; }
        bool isBoosting() const { return m_boosting; }
        bool isGameOver() const { return m_gameOver; }
        float invulnerableTimer() const { return m_invulnerableTimer; }
        float distanceTraveled() const { return -m_playerPos.z; }

        const std::vector<Mote>& motes() const { return m_motes; }

        // Audio events emitted during the last simulation step
        const std::vector<AudioEvent>& audioEvents() const { return m_audioEvents; }

    private:
        void updateMotes();

        glm::vec3 m_playerPos{0.0f, 0.0f, 0.0f};
        glm::vec3 m_prevPlayerPos{0.0f, 0.0f, 0.0f};
        glm::vec3 m_playerVel{0.0f, 0.0f, -16.0f};
        int m_shields = kMaxShields;
        int m_score = 0;
        int m_bestScore = 0;
        int m_combo = 1;
        float m_comboTimer = 0.0f;
        float m_energy = kMaxEnergy;
        float m_invulnerableTimer = 0.0f;
        bool m_boosting = false;
        bool m_wasBoosting = false;
        bool m_gameOver = false;

        std::vector<Mote> m_motes;
        int m_farthestSectionGenerated = 0;
        std::vector<AudioEvent> m_audioEvents;
    };

    // Synthesized sound effects and music parameters for sdf_glide
    struct GlideAudioParams {
        std::array<mge::SfxParams, 8> moteSounds; // Pitched by combo 1..8
        mge::SfxParams hitSound;
        mge::SfxParams boostSound;
        mge::Song themeSong;

        static GlideAudioParams create();
    };

} // namespace sdf_glide

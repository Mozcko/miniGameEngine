#include "SdfGlideLogic.h"

#include <algorithm>
#include <cmath>

namespace sdf_glide {

    uint32_t sectionHashU32(int32_t s, uint32_t salt) {
        uint32_t x = static_cast<uint32_t>(s) ^ (salt * 0x9e3779b9u);
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    float sectionHash(int32_t s, uint32_t salt) {
        return static_cast<float>(sectionHashU32(s, salt) & 0x00ffffffu) / 16777216.0f;
    }

    float sdSphere(const glm::vec3& p, float radius) {
        return glm::length(p) - radius;
    }

    float sdBox(const glm::vec3& p, const glm::vec3& halfExtents) {
        const glm::vec3 q = glm::abs(p) - halfExtents;
        return glm::length(glm::max(q, glm::vec3(0.0f))) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
    }

    float sdTorus(const glm::vec3& p, float majorRadius, float minorRadius) {
        const glm::vec2 q(glm::length(glm::vec2(p.x, p.z)) - majorRadius, p.y);
        return glm::length(q) - minorRadius;
    }

    float opSmoothUnion(float d1, float d2, float k) {
        if (k <= 0.0f) return std::min(d1, d2);
        const float h = glm::clamp(0.5f + 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
        return glm::mix(d2, d1, h) - k * h * (1.0f - h);
    }

    glm::vec3 opTwist(const glm::vec3& p, float radiansPerUnit) {
        const float angle = radiansPerUnit * p.y;
        const float c = std::cos(angle), s = std::sin(angle);
        return glm::vec3(c * p.x - s * p.z, p.y, s * p.x + c * p.z);
    }

    float canyonDistance(const glm::vec3& p) {
        // Paredes laterales y piso del cañón
        float d = std::min(std::min(p.x + 6.0f, 6.0f - p.x), p.y + 3.0f);

        constexpr float kL = GlideGame::kSectionLength;
        const int32_t baseS = static_cast<int32_t>(std::floor((p.z + kL * 0.5f) / kL));

        for (int32_t ds = -1; ds <= 1; ++ds) {
            const int32_t s = baseS + ds;
            const float sz = static_cast<float>(s) * kL;
            const float h0 = sectionHash(s, 0u);
            const float h1 = sectionHash(s, 1u);
            const float h2 = sectionHash(s, 2u);
            const float h3 = sectionHash(s, 3u);

            // A. Rocas laterales con unión suave (smooth-union)
            const glm::vec3 r1Pos(-4.8f + h0 * 0.6f, -2.0f + h1 * 0.8f, sz - 4.0f + (h2 - 0.5f) * 3.0f);
            const float dRock1 = sdSphere(p - r1Pos, 1.6f + h3 * 0.5f);
            d = opSmoothUnion(d, dRock1, 1.2f);

            const glm::vec3 r2Pos(4.8f - h2 * 0.6f, -2.0f + h3 * 0.8f, sz + 4.0f + (h0 - 0.5f) * 3.0f);
            const float dRock2 = sdSphere(p - r2Pos, 1.5f + h1 * 0.5f);
            d = opSmoothUnion(d, dRock2, 1.2f);

            // B. Pilares con torsión en los costados
            const float p1x = -3.8f + (h1 - 0.5f) * 0.6f;
            const float p1z = sz - 7.0f;
            const glm::vec3 tp1 = opTwist(p - glm::vec3(p1x, 0.0f, p1z), (h2 > 0.5f ? 0.35f : -0.35f));
            const float dPil1 = sdBox(tp1, glm::vec3(0.55f, 6.0f, 0.55f)) - 0.12f;
            d = opSmoothUnion(d, dPil1, 0.35f);

            const float p2x = 3.8f - (h3 - 0.5f) * 0.6f;
            const float p2z = sz + 7.0f;
            const glm::vec3 tp2 = opTwist(p - glm::vec3(p2x, 0.0f, p2z), (h0 > 0.5f ? -0.35f : 0.35f));
            const float dPil2 = sdBox(tp2, glm::vec3(0.55f, 6.0f, 0.55f)) - 0.12f;
            d = opSmoothUnion(d, dPil2, 0.35f);

            // C. Arcos que cruzan el cañón
            const float archZ = sz + (h1 - 0.5f) * 4.0f;
            const float archX = (h0 - 0.5f) * 1.0f;
            const float archMajorR = 5.2f + h2 * 0.6f;
            const float archMinorR = 0.55f + h3 * 0.15f;
            const glm::vec3 pArch = p - glm::vec3(archX, -1.0f, archZ);
            const float dArch = sdTorus(glm::vec3(pArch.x, pArch.z, pArch.y), archMajorR, archMinorR);
            d = opSmoothUnion(d, dArch, 0.35f);
        }
        return d;
    }

    glm::vec3 canyonNormal(const glm::vec3& p, float eps) {
        const float dx =
            canyonDistance(p + glm::vec3(eps, 0.0f, 0.0f)) - canyonDistance(p - glm::vec3(eps, 0.0f, 0.0f));
        const float dy =
            canyonDistance(p + glm::vec3(0.0f, eps, 0.0f)) - canyonDistance(p - glm::vec3(0.0f, eps, 0.0f));
        const float dz =
            canyonDistance(p + glm::vec3(0.0f, 0.0f, eps)) - canyonDistance(p - glm::vec3(0.0f, 0.0f, eps));
        const glm::vec3 n(dx, dy, dz);
        const float len = glm::length(n);
        return len > 1e-6f ? (n / len) : glm::vec3(0.0f, 1.0f, 0.0f);
    }

    GlideGame::GlideGame() {
        reset();
    }

    void GlideGame::reset() {
        m_playerPos = glm::vec3(0.0f, 0.0f, 0.0f);
        m_playerVel = glm::vec3(0.0f, 0.0f, -16.0f);
        m_shields = kMaxShields;
        m_score = 0;
        m_combo = 1;
        m_comboTimer = 0.0f;
        m_energy = kMaxEnergy;
        m_invulnerableTimer = 0.0f;
        m_boosting = false;
        m_wasBoosting = false;
        m_gameOver = false;
        m_motes.clear();
        m_farthestSectionGenerated = 1;
        m_audioEvents.clear();

        // Generar motes iniciales para las primeras secciones
        for (int s = 0; s >= -4; --s) {
            const float sz = static_cast<float>(s) * kSectionLength;
            const float h0 = sectionHash(s, 4u);
            const float h1 = sectionHash(s, 5u);
            const float h2 = sectionHash(s, 6u);
            const float h3 = sectionHash(s, 7u);

            m_motes.push_back({glm::vec3(-2.2f + h0 * 4.4f, -0.6f + h1 * 2.2f, sz - 6.0f), false, s});
            m_motes.push_back({glm::vec3(-2.2f + h2 * 4.4f, -0.6f + h3 * 2.2f, sz + 6.0f), false, s});
        }
        m_farthestSectionGenerated = -4;
    }

    void GlideGame::updateMotes() {
        const int currentSection =
            static_cast<int>(std::floor((m_playerPos.z + kSectionLength * 0.5f) / kSectionLength));
        const int targetSection = currentSection - 5; // Generar 5 secciones adelante

        while (m_farthestSectionGenerated > targetSection) {
            const int s = --m_farthestSectionGenerated;
            const float sz = static_cast<float>(s) * kSectionLength;
            const float h0 = sectionHash(s, 4u);
            const float h1 = sectionHash(s, 5u);
            const float h2 = sectionHash(s, 6u);
            const float h3 = sectionHash(s, 7u);

            m_motes.push_back({glm::vec3(-2.2f + h0 * 4.4f, -0.6f + h1 * 2.2f, sz - 6.0f), false, s});
            m_motes.push_back({glm::vec3(-2.2f + h2 * 4.4f, -0.6f + h3 * 2.2f, sz + 6.0f), false, s});
        }

        // Eliminar motes ya superados por más de 30 unidades
        m_motes.erase(std::remove_if(m_motes.begin(), m_motes.end(),
                                     [this](const Mote& m) { return m.position.z > m_playerPos.z + 30.0f; }),
                      m_motes.end());
    }

    void GlideGame::update(float dt, const GlideInput& input) {
        m_audioEvents.clear();

        if (m_gameOver) {
            if (input.restart) reset();
            return;
        }

        // 1. Impulso (Boost) y gestión de energía
        if (input.boost && m_energy > 0.0f) {
            m_boosting = true;
            m_energy = std::max(0.0f, m_energy - 35.0f * dt);
            if (!m_wasBoosting) {
                m_audioEvents.push_back(AudioEvent::BoostStart);
            }
        } else {
            m_boosting = false;
            m_energy = std::min(kMaxEnergy, m_energy + 15.0f * dt);
        }
        m_wasBoosting = m_boosting;

        // 2. Velocidad hacia adelante (aumenta suavemente con la distancia)
        const float dist = -m_playerPos.z;
        const float baseSpeed = 16.0f + dist * 0.012f;
        const float forwardSpeed = baseSpeed * (m_boosting ? 1.6f : 1.0f);
        m_playerVel.z = -forwardSpeed;

        // 3. Dirección lateral y vertical (WASD o ratón)
        const float targetVelX = input.steerX * 12.0f;
        const float targetVelY = input.steerY * 10.0f;
        m_playerVel.x = glm::mix(m_playerVel.x, targetVelX, std::clamp(12.0f * dt, 0.0f, 1.0f));
        m_playerVel.y = glm::mix(m_playerVel.y, targetVelY, std::clamp(12.0f * dt, 0.0f, 1.0f));

        m_playerPos += m_playerVel * dt;

        // 4. Temporizador de invulnerabilidad tras colisión
        if (m_invulnerableTimer > 0.0f) {
            m_invulnerableTimer = std::max(0.0f, m_invulnerableTimer - dt);
        }

        // 5. Detección y respuesta de colisión con el cañón
        const float d = canyonDistance(m_playerPos);
        if (d < kPlayerRadius) {
            const glm::vec3 normal = canyonNormal(m_playerPos);
            // Empujar hacia afuera para evitar que quede atascado
            m_playerPos += normal * (kPlayerRadius - d + 0.15f);

            // Rebote de velocidad en el plano normal
            m_playerVel.x += normal.x * 10.0f;
            m_playerVel.y += normal.y * 8.0f;

            if (m_invulnerableTimer <= 0.0f) {
                m_shields--;
                m_combo = 1; // El impacto resetea el multiplicador
                m_invulnerableTimer = 1.2f;
                m_audioEvents.push_back(AudioEvent::HitWall);

                if (m_shields <= 0) {
                    m_shields = 0;
                    m_gameOver = true;
                    if (m_score > m_bestScore) m_bestScore = m_score;
                    m_audioEvents.push_back(AudioEvent::GameOver);
                }
            }
        }

        // 6. Temporizador de combo
        if (m_comboTimer > 0.0f) {
            m_comboTimer -= dt;
            if (m_comboTimer <= 0.0f) {
                m_combo = 1;
            }
        }

        // 7. Generación y recolección de motes
        updateMotes();

        constexpr float kCollectDistSq = 0.95f * 0.95f;
        for (auto& mote : m_motes) {
            if (mote.collected) continue;
            const glm::vec3 diff = mote.position - m_playerPos;
            if (glm::dot(diff, diff) < kCollectDistSq) {
                mote.collected = true;
                m_score += 100 * m_combo;
                if (m_score > m_bestScore) m_bestScore = m_score;
                m_combo = std::min(m_combo + 1, 8);
                m_comboTimer = 3.5f;
                m_energy = std::min(kMaxEnergy, m_energy + 15.0f);
                m_audioEvents.push_back(AudioEvent::CollectMote);
            }
        }
    }

    GlideAudioParams GlideAudioParams::create() {
        GlideAudioParams params;

        // Sonidos de motes: moneda con frecuencia base escalada según combo (1..8)
        for (int c = 1; c <= 8; ++c) {
            mge::SfxParams coin = mge::sfx::coin(0x42u + static_cast<uint64_t>(c));
            coin.baseFrequency = 880.0f * (1.0f + 0.09f * static_cast<float>(c - 1));
            coin.arpeggioMultiplier = 1.25f;
            coin.gain = 0.32f;
            params.moteSounds[static_cast<size_t>(c - 1)] = coin;
        }

        // Sonido de impacto contra la pared
        params.hitSound = mge::sfx::hurt(101u);
        params.hitSound.gain = 0.48f;

        // Sonido de propulsión / boost (whoosh de ruido con deslizamiento)
        params.boostSound.wave = mge::Waveform::Noise;
        params.boostSound.baseFrequency = 200.0f;
        params.boostSound.slide = 3.0f;
        params.boostSound.deltaSlide = -6.0f;
        params.boostSound.attack = 0.03f;
        params.boostSound.sustain = 0.16f;
        params.boostSound.decay = 0.16f;
        params.boostSound.lowPassCutoff = 2200.0f;
        params.boostSound.lowPassSweep = -2.0f;
        params.boostSound.gain = 0.35f;

        // Tema musical de vuelo sintetizado (128 BPM, dinámico y rítmico)
        mge::Song song(128.0f, 4);

        mge::Instrument lead;
        lead.wave = mge::Waveform::Square;
        lead.pulseWidth = 0.35f;
        lead.envelope = {0.005f, 0.06f, 0.4f, 0.12f};
        lead.vibratoDepth = 0.1f;
        lead.vibratoRate = 5.0f;
        lead.delaySend = 0.3f;
        lead.gain = 0.22f;

        mge::Instrument bass;
        bass.wave = mge::Waveform::Saw;
        bass.envelope = {0.005f, 0.08f, 0.5f, 0.08f};
        bass.filter = mge::FilterMode::LowPass;
        bass.cutoff = 900.0f;
        bass.gain = 0.28f;

        mge::Instrument hat;
        hat.wave = mge::Waveform::Noise;
        hat.envelope = {0.001f, 0.04f, 0.0f, 0.02f};
        hat.filter = mge::FilterMode::HighPass;
        hat.cutoff = 4000.0f;
        hat.gain = 0.10f;

        mge::Instrument kick;
        kick.wave = mge::Waveform::Sine;
        kick.envelope = {0.001f, 0.10f, 0.0f, 0.02f};
        kick.pitchSlide = -45.0f;
        kick.gain = 0.38f;

        const int iLead = song.addInstrument(lead);
        const int iBass = song.addInstrument(bass);
        const int iHat = song.addInstrument(hat);
        const int iKick = song.addInstrument(kick);

        const int melodyA = song.pattern("A4 . C5 . | E5 - D5 . | C5 . A4 . | G4 - - . "
                                         "F4 . A4 . | C5 - B4 . | G4 . E4 . | A4 - - . ");
        const int bassA = song.pattern("A2 . A2 . | A2 . A2 . | F2 . F2 . | G2 . G2 . "
                                       "D2 . D2 . | F2 . F2 . | E2 . E2 . | A2 . E2 . ");
        const int hats = song.pattern(". x . x | . x . x | . x . x | . x x x", 1.0f, 96.0f);
        const int kicks = song.pattern("x . . . | x . . . | x . . . | x . x .", 1.0f, 52.0f);

        song.addTrack(iLead, {melodyA, melodyA});
        song.addTrack(iBass, {bassA, bassA});
        song.addTrack(iHat, {hats, hats, hats, hats});
        song.addTrack(iKick, {kicks, kicks, kicks, kicks});

        params.themeSong = song;
        return params;
    }

} // namespace sdf_glide

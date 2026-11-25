#include <mge/graphics/DayNight.h>

#include <algorithm>
#include <cmath>

namespace mge {

    namespace {
        struct SkyKeyframe {
            float hour;
            glm::vec3 zenithColor;
            glm::vec3 horizonColor;
            glm::vec3 groundColor;
            glm::vec3 sunColor;
            float sunSize;
            float sunGlow;
            float starsIntensity;
        };

        // Keyframes across the 24-hour cycle.
        // 0:00 and 24:00 are identical to allow smooth, continuous cyclic interpolation.
        constexpr SkyKeyframe kKeyframes[] = {
            // 0:00 Deep midnight
            {0.0f,
             {0.02f, 0.02f, 0.05f},
             {0.04f, 0.05f, 0.10f},
             {0.02f, 0.02f, 0.03f},
             {0.0f, 0.0f, 0.0f},
             0.9995f,
             8.0f,
             1.0f},
            // 4:00 Late night
            {4.0f,
             {0.02f, 0.02f, 0.05f},
             {0.04f, 0.05f, 0.10f},
             {0.02f, 0.02f, 0.03f},
             {0.0f, 0.0f, 0.0f},
             0.9995f,
             8.0f,
             1.0f},
            // 5:00 First light of dawn
            {5.0f,
             {0.05f, 0.06f, 0.14f},
             {0.18f, 0.12f, 0.18f},
             {0.04f, 0.04f, 0.05f},
             {0.20f, 0.10f, 0.05f},
             0.9995f,
             8.0f,
             0.7f},
            // 6:00 Sunrise (golden horizon)
            {6.0f,
             {0.18f, 0.28f, 0.55f},
             {0.90f, 0.45f, 0.25f},
             {0.15f, 0.12f, 0.10f},
             {1.0f, 0.65f, 0.35f},
             0.9995f,
             8.0f,
             0.0f},
            // 7:00 Early morning
            {7.0f,
             {0.25f, 0.45f, 0.80f},
             {0.65f, 0.70f, 0.85f},
             {0.20f, 0.22f, 0.20f},
             {0.95f, 0.85f, 0.70f},
             0.9995f,
             8.0f,
             0.0f},
            // 9:00 Mid-morning
            {9.0f,
             {0.20f, 0.45f, 0.85f},
             {0.55f, 0.72f, 0.95f},
             {0.25f, 0.25f, 0.22f},
             {1.0f, 0.98f, 0.90f},
             0.9995f,
             8.0f,
             0.0f},
            // 12:00 Noon
            {12.0f,
             {0.18f, 0.42f, 0.85f},
             {0.55f, 0.72f, 0.95f},
             {0.25f, 0.25f, 0.22f},
             {1.0f, 0.98f, 0.92f},
             0.9995f,
             8.0f,
             0.0f},
            // 15:00 Afternoon
            {15.0f,
             {0.20f, 0.45f, 0.85f},
             {0.55f, 0.72f, 0.95f},
             {0.25f, 0.25f, 0.22f},
             {1.0f, 0.98f, 0.90f},
             0.9995f,
             8.0f,
             0.0f},
            // 17:00 Early evening
            {17.0f,
             {0.22f, 0.40f, 0.75f},
             {0.75f, 0.60f, 0.45f},
             {0.22f, 0.20f, 0.18f},
             {1.0f, 0.80f, 0.50f},
             0.9995f,
             8.0f,
             0.0f},
            // 18:00 Sunset (sun at horizon, red/orange sky)
            {18.0f,
             {0.15f, 0.22f, 0.50f},
             {0.95f, 0.38f, 0.18f},
             {0.14f, 0.10f, 0.10f},
             {1.0f, 0.45f, 0.15f},
             0.9995f,
             8.0f,
             0.0f},
            // 19:00 Evening twilight
            {19.0f,
             {0.06f, 0.08f, 0.22f},
             {0.30f, 0.15f, 0.25f},
             {0.05f, 0.04f, 0.05f},
             {0.20f, 0.08f, 0.05f},
             0.9995f,
             8.0f,
             0.5f},
            // 20:00 Nightfall
            {20.0f,
             {0.03f, 0.03f, 0.08f},
             {0.06f, 0.06f, 0.12f},
             {0.02f, 0.02f, 0.03f},
             {0.0f, 0.0f, 0.0f},
             0.9995f,
             8.0f,
             0.9f},
            // 21:00 Full night
            {21.0f,
             {0.02f, 0.02f, 0.05f},
             {0.04f, 0.05f, 0.10f},
             {0.02f, 0.02f, 0.03f},
             {0.0f, 0.0f, 0.0f},
             0.9995f,
             8.0f,
             1.0f},
            // 24:00 Midnight (same values as 0:00)
            {24.0f,
             {0.02f, 0.02f, 0.05f},
             {0.04f, 0.05f, 0.10f},
             {0.02f, 0.02f, 0.03f},
             {0.0f, 0.0f, 0.0f},
             0.9995f,
             8.0f,
             1.0f},
        };
        constexpr std::size_t kKeyframeCount = sizeof(kKeyframes) / sizeof(kKeyframes[0]);
    } // namespace

    void DayNight::advance(float dt) {
        if (dayLengthSeconds > 0.0f) {
            hour += (dt / dayLengthSeconds) * 24.0f;
            hour = std::fmod(hour, 24.0f);
            if (hour < 0.0f) hour += 24.0f;
        }
    }

    SkySettings DayNight::sky() const {
        float wrappedHour = std::fmod(hour, 24.0f);
        if (wrappedHour < 0.0f) wrappedHour += 24.0f;

        // Find the matching keyframe segment
        std::size_t idx = 0;
        while (idx + 1 < kKeyframeCount && kKeyframes[idx + 1].hour <= wrappedHour) {
            idx++;
        }
        const SkyKeyframe& a = kKeyframes[idx];
        const SkyKeyframe& b = (idx + 1 < kKeyframeCount) ? kKeyframes[idx + 1] : kKeyframes[idx];
        const float duration = b.hour - a.hour;
        const float t = (duration > 0.0f) ? (wrappedHour - a.hour) / duration : 0.0f;

        SkySettings s;
        s.zenithColor = glm::mix(a.zenithColor, b.zenithColor, t);
        s.horizonColor = glm::mix(a.horizonColor, b.horizonColor, t);
        s.groundColor = glm::mix(a.groundColor, b.groundColor, t);
        s.sunColor = glm::mix(a.sunColor, b.sunColor, t);
        s.sunSize = glm::mix(a.sunSize, b.sunSize, t);
        s.sunGlow = glm::mix(a.sunGlow, b.sunGlow, t);
        s.starsIntensity = glm::mix(a.starsIntensity, b.starsIntensity, t);

        // Solar path: rises at 6:00, zenith at 12:00, sets at 18:00, nadir at 0:00
        constexpr float kPi = 3.14159265358979323846f;
        const float angle = (wrappedHour - 6.0f) * (2.0f * kPi / 24.0f);
        constexpr float kTilt = 0.26f; // Slight tilt relative to the meridian (~15 degrees)
        const glm::vec3 sunPos = glm::normalize(glm::vec3(-std::cos(angle), std::sin(angle) * std::cos(kTilt),
                                                          std::sin(angle) * std::sin(kTilt)));
        s.sunDirection = -sunPos; // Direction light travels toward

        return s;
    }

    Lighting DayNight::lighting(const Lighting& base) const {
        Lighting l = base;
        const SkySettings s = sky();
        l.sunDirection = s.sunDirection;
        l.sunColor = s.sunColor;
        l.skyAmbient = glm::max(s.zenithColor * 0.7f + s.horizonColor * 0.3f, glm::vec3(0.04f, 0.04f, 0.06f));
        l.groundAmbient = glm::max(s.groundColor, glm::vec3(0.02f, 0.02f, 0.03f));
        l.fogColor = s.horizonColor; // Matches the sky horizon exactly
        return l;
    }

} // namespace mge

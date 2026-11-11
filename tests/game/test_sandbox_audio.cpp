#include <doctest/doctest.h>
#include <game/SandboxBlocks.h>
#include <game/SandboxSounds.h>
#include <mge/synth/Mixer.h>
#include <mge/synth/Music.h>
#include <mge/synth/Offline.h>
#include <mge/synth/Sfx.h>

#include <cmath>
#include <vector>

using namespace mge;
using namespace sandbox;
namespace an = mge::analysis;

TEST_CASE("SandboxSounds: clasificacion de materiales por nombre y bloque") {
    BlockRegistry reg;
    const SandboxBlocks b = registerSandboxBlocks(reg);

    CHECK(soundMaterialForBlock("grass") == SoundMaterial::Grass);
    CHECK(soundMaterialForBlock("dirt") == SoundMaterial::Grass);
    CHECK(soundMaterialForBlock("sand") == SoundMaterial::Sand);
    CHECK(soundMaterialForBlock("gravel") == SoundMaterial::Sand);
    CHECK(soundMaterialForBlock("planks") == SoundMaterial::Wood);
    CHECK(soundMaterialForBlock("log") == SoundMaterial::Wood);
    CHECK(soundMaterialForBlock("leaves") == SoundMaterial::Leaves);
    CHECK(soundMaterialForBlock("flower") == SoundMaterial::Leaves);
    CHECK(soundMaterialForBlock("stone") == SoundMaterial::Stone);
    CHECK(soundMaterialForBlock("cobblestone") == SoundMaterial::Stone);
    CHECK(soundMaterialForBlock("glass") == SoundMaterial::Stone);

    CHECK(soundMaterialForBlock(b.grass, reg) == SoundMaterial::Grass);
    CHECK(soundMaterialForBlock(b.sand, reg) == SoundMaterial::Sand);
    CHECK(soundMaterialForBlock(b.planks, reg) == SoundMaterial::Wood);
    CHECK(soundMaterialForBlock(b.leaves, reg) == SoundMaterial::Leaves);
    CHECK(soundMaterialForBlock(b.stone, reg) == SoundMaterial::Stone);
    CHECK(soundMaterialForBlock(kAir, reg) == SoundMaterial::Grass);
}

TEST_CASE("SandboxSounds: cada muestra se sintetiza sin NaN y con rms > 0") {
    const SandboxSounds sounds;
    const std::vector<SfxParams> all = sounds.allParams();
    REQUIRE_FALSE(all.empty());

    constexpr int kSampleRate = 48000;
    for (const auto& p : all) {
        const Sample sample = renderSfx(p, kSampleRate);
        REQUIRE_FALSE(sample.empty());
        for (float v : sample.data) {
            CHECK_FALSE(std::isnan(v));
            CHECK_FALSE(std::isinf(v));
        }
        CHECK(an::rms(sample.data) > 0.0f);
    }
}

TEST_CASE("SandboxSounds: las 4 variantes de pisada difieren entre si") {
    const SandboxSounds sounds;
    constexpr int kSampleRate = 48000;

    for (std::size_t m = 0; m < SandboxSounds::kMaterialCount; ++m) {
        const auto mat = static_cast<SoundMaterial>(m);
        std::array<Sample, SandboxSounds::kFootstepVariants> variants;
        for (std::size_t v = 0; v < SandboxSounds::kFootstepVariants; ++v) {
            variants[v] = renderSfx(sounds.footstep(mat, v), kSampleRate);
            CHECK(an::rms(variants[v].data) > 0.0f);
        }

        // Cada par de variantes debe tener muestras diferentes
        for (std::size_t i = 0; i < SandboxSounds::kFootstepVariants; ++i) {
            for (std::size_t j = i + 1; j < SandboxSounds::kFootstepVariants; ++j) {
                CHECK(variants[i].data != variants[j].data);
            }
        }
    }
}

TEST_CASE("sandboxNight: se sintetiza en offline y detecta notas (onsets > 0)") {
    const Song night = songs::sandboxNight();
    REQUIRE(night.valid());

    constexpr int kRate = 48000;
    Mixer mixer(kRate);
    mixer.setDelay({0.3f, 0.35f, 0.4f});
    const SongId id = mixer.addSong(night);
    REQUIRE(id != kInvalidSong);
    CHECK(mixer.playSong(id));

    // Renderizar los primeros 8 segundos (incluye notas de campanilla)
    const std::vector<float> rendered = renderOffline(mixer, 8.0f);
    const std::vector<float> left = an::channel(rendered, 2, 0);

    CHECK(an::rms(left) > 0.005f);
    CHECK(an::peak(left) < 1.0f);

    const std::vector<std::size_t> noteStarts = an::onsets(left, kRate, 0.02f);
    CHECK(noteStarts.size() > 0);
}

TEST_CASE("SandboxSounds: registro de samples en Mixer asigna ids validos") {
    const SandboxSounds sounds;
    Mixer mixer(48000);
    const RegisteredSounds reg = registerSandboxSounds(mixer, sounds);

    CHECK(reg.jump != kInvalidSample);
    CHECK(reg.land != kInvalidSample);
    CHECK(reg.splash != kInvalidSample);
    CHECK(reg.throwSound != kInvalidSample);

    for (std::size_t m = 0; m < SandboxSounds::kMaterialCount; ++m) {
        for (std::size_t v = 0; v < SandboxSounds::kFootstepVariants; ++v) {
            CHECK(reg.materials[m].footsteps[v] != kInvalidSample);
        }
        for (std::size_t v = 0; v < SandboxSounds::kBreakVariants; ++v) {
            CHECK(reg.materials[m].breakVariants[v] != kInvalidSample);
        }
        for (std::size_t v = 0; v < SandboxSounds::kPlaceVariants; ++v) {
            CHECK(reg.materials[m].placeVariants[v] != kInvalidSample);
        }
    }
}

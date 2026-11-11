#include <game/SandboxSounds.h>
#include <mge/audio/AudioModule.h>

namespace sandbox {

    SoundMaterial soundMaterialForBlock(std::string_view name) {
        if (name == "grass" || name == "dirt") return SoundMaterial::Grass;
        if (name == "sand" || name == "gravel") return SoundMaterial::Sand;
        if (name == "planks" || name == "log" || name == "tnt") return SoundMaterial::Wood;
        if (name == "leaves" || name == "rose" || name == "flower" || name == "sapling" || name == "cobweb") {
            return SoundMaterial::Leaves;
        }
        return SoundMaterial::Stone;
    }

    SoundMaterial soundMaterialForBlock(mge::BlockId id, const mge::BlockRegistry& registry) {
        if (id == mge::kAir) return SoundMaterial::Grass;
        return soundMaterialForBlock(registry.get(id).name);
    }

    SandboxSounds::SandboxSounds() {
        // --- 1. Pasto (Grass) ---
        mge::SfxParams grassStep;
        grassStep.wave = mge::Waveform::Noise;
        grassStep.baseFrequency = 450.0f;
        grassStep.attack = 0.005f;
        grassStep.sustain = 0.02f;
        grassStep.punch = 0.2f;
        grassStep.decay = 0.05f;
        grassStep.lowPassCutoff = 1000.0f;
        grassStep.lowPassSweep = -2.0f;
        grassStep.gain = 0.25f;

        mge::SfxParams grassBreak;
        grassBreak.wave = mge::Waveform::Noise;
        grassBreak.baseFrequency = 500.0f;
        grassBreak.attack = 0.005f;
        grassBreak.sustain = 0.04f;
        grassBreak.punch = 0.35f;
        grassBreak.decay = 0.12f;
        grassBreak.lowPassCutoff = 1200.0f;
        grassBreak.lowPassSweep = -3.0f;
        grassBreak.gain = 0.35f;

        mge::SfxParams grassPlace;
        grassPlace.wave = mge::Waveform::Noise;
        grassPlace.baseFrequency = 420.0f;
        grassPlace.attack = 0.005f;
        grassPlace.sustain = 0.02f;
        grassPlace.punch = 0.25f;
        grassPlace.decay = 0.07f;
        grassPlace.lowPassCutoff = 900.0f;
        grassPlace.gain = 0.3f;

        // --- 2. Piedra (Stone) ---
        mge::SfxParams stoneStep;
        stoneStep.wave = mge::Waveform::Noise;
        stoneStep.baseFrequency = 850.0f;
        stoneStep.attack = 0.002f;
        stoneStep.sustain = 0.015f;
        stoneStep.punch = 0.4f;
        stoneStep.decay = 0.045f;
        stoneStep.highPassCutoff = 350.0f;
        stoneStep.lowPassCutoff = 3200.0f;
        stoneStep.gain = 0.25f;

        mge::SfxParams stoneBreak;
        stoneBreak.wave = mge::Waveform::Noise;
        stoneBreak.baseFrequency = 750.0f;
        stoneBreak.slide = -1.5f;
        stoneBreak.attack = 0.002f;
        stoneBreak.sustain = 0.04f;
        stoneBreak.punch = 0.5f;
        stoneBreak.decay = 0.14f;
        stoneBreak.highPassCutoff = 200.0f;
        stoneBreak.lowPassCutoff = 2800.0f;
        stoneBreak.lowPassSweep = -2.0f;
        stoneBreak.gain = 0.4f;

        mge::SfxParams stonePlace;
        stonePlace.wave = mge::Waveform::Noise;
        stonePlace.baseFrequency = 900.0f;
        stonePlace.attack = 0.002f;
        stonePlace.sustain = 0.02f;
        stonePlace.punch = 0.45f;
        stonePlace.decay = 0.06f;
        stonePlace.highPassCutoff = 300.0f;
        stonePlace.lowPassCutoff = 3000.0f;
        stonePlace.gain = 0.35f;

        // --- 3. Arena (Sand) ---
        mge::SfxParams sandStep;
        sandStep.wave = mge::Waveform::Noise;
        sandStep.baseFrequency = 550.0f;
        sandStep.attack = 0.01f;
        sandStep.sustain = 0.025f;
        sandStep.punch = 0.15f;
        sandStep.decay = 0.06f;
        sandStep.lowPassCutoff = 1600.0f;
        sandStep.lowPassSweep = -1.0f;
        sandStep.gain = 0.22f;

        mge::SfxParams sandBreak;
        sandBreak.wave = mge::Waveform::Noise;
        sandBreak.baseFrequency = 600.0f;
        sandBreak.attack = 0.01f;
        sandBreak.sustain = 0.05f;
        sandBreak.punch = 0.2f;
        sandBreak.decay = 0.13f;
        sandBreak.lowPassCutoff = 1500.0f;
        sandBreak.lowPassSweep = -2.0f;
        sandBreak.gain = 0.32f;

        mge::SfxParams sandPlace;
        sandPlace.wave = mge::Waveform::Noise;
        sandPlace.baseFrequency = 520.0f;
        sandPlace.attack = 0.01f;
        sandPlace.sustain = 0.025f;
        sandPlace.punch = 0.2f;
        sandPlace.decay = 0.08f;
        sandPlace.lowPassCutoff = 1400.0f;
        sandPlace.gain = 0.28f;

        // --- 4. Madera (Wood) ---
        mge::SfxParams woodStep;
        woodStep.wave = mge::Waveform::Triangle;
        woodStep.baseFrequency = 180.0f;
        woodStep.slide = -4.0f;
        woodStep.attack = 0.003f;
        woodStep.sustain = 0.02f;
        woodStep.punch = 0.35f;
        woodStep.decay = 0.06f;
        woodStep.lowPassCutoff = 1500.0f;
        woodStep.gain = 0.3f;

        mge::SfxParams woodBreak;
        woodBreak.wave = mge::Waveform::Square;
        woodBreak.duty = 0.25f;
        woodBreak.baseFrequency = 220.0f;
        woodBreak.slide = -5.0f;
        woodBreak.attack = 0.003f;
        woodBreak.sustain = 0.04f;
        woodBreak.punch = 0.45f;
        woodBreak.decay = 0.13f;
        woodBreak.lowPassCutoff = 2000.0f;
        woodBreak.lowPassSweep = -3.0f;
        woodBreak.gain = 0.38f;

        mge::SfxParams woodPlace;
        woodPlace.wave = mge::Waveform::Triangle;
        woodPlace.baseFrequency = 230.0f;
        woodPlace.slide = -4.0f;
        woodPlace.attack = 0.003f;
        woodPlace.sustain = 0.02f;
        woodPlace.punch = 0.4f;
        woodPlace.decay = 0.07f;
        woodPlace.lowPassCutoff = 1800.0f;
        woodPlace.gain = 0.35f;

        // --- 5. Hojas / Plantas (Leaves) ---
        mge::SfxParams leavesStep;
        leavesStep.wave = mge::Waveform::Noise;
        leavesStep.baseFrequency = 1100.0f;
        leavesStep.attack = 0.005f;
        leavesStep.sustain = 0.02f;
        leavesStep.punch = 0.1f;
        leavesStep.decay = 0.05f;
        leavesStep.highPassCutoff = 900.0f;
        leavesStep.gain = 0.2f;

        mge::SfxParams leavesBreak;
        leavesBreak.wave = mge::Waveform::Noise;
        leavesBreak.baseFrequency = 1300.0f;
        leavesBreak.attack = 0.005f;
        leavesBreak.sustain = 0.035f;
        leavesBreak.punch = 0.25f;
        leavesBreak.decay = 0.10f;
        leavesBreak.highPassCutoff = 800.0f;
        leavesBreak.gain = 0.3f;

        mge::SfxParams leavesPlace;
        leavesPlace.wave = mge::Waveform::Noise;
        leavesPlace.baseFrequency = 1000.0f;
        leavesPlace.attack = 0.005f;
        leavesPlace.sustain = 0.02f;
        leavesPlace.punch = 0.2f;
        leavesPlace.decay = 0.06f;
        leavesPlace.highPassCutoff = 850.0f;
        leavesPlace.gain = 0.25f;

        const struct BaseMaterial {
            mge::SfxParams step;
            mge::SfxParams brk;
            mge::SfxParams plc;
        } bases[kMaterialCount] = {
            {grassStep, grassBreak, grassPlace},    {stoneStep, stoneBreak, stonePlace},
            {sandStep, sandBreak, sandPlace},       {woodStep, woodBreak, woodPlace},
            {leavesStep, leavesBreak, leavesPlace},
        };

        // Semillas fijas para mutar variantes de manera determinista y diferenciada
        constexpr std::uint64_t kFootstepSeeds[kFootstepVariants] = {0x101u, 0x102u, 0x103u, 0x104u};
        constexpr std::uint64_t kBreakSeeds[kBreakVariants] = {0x201u, 0x202u, 0x203u, 0x204u};
        constexpr std::uint64_t kPlaceSeeds[kPlaceVariants] = {0x301u, 0x302u, 0x303u, 0x304u};

        for (std::size_t m = 0; m < kMaterialCount; ++m) {
            auto& mat = m_materials[m];
            for (std::size_t v = 0; v < kFootstepVariants; ++v) {
                mat.footsteps[v] = mge::sfx::mutate(bases[m].step, 0.15f, kFootstepSeeds[v] + m * 16u);
            }
            for (std::size_t v = 0; v < kBreakVariants; ++v) {
                mat.breakVariants[v] = mge::sfx::mutate(bases[m].brk, 0.15f, kBreakSeeds[v] + m * 16u);
            }
            for (std::size_t v = 0; v < kPlaceVariants; ++v) {
                mat.placeVariants[v] = mge::sfx::mutate(bases[m].plc, 0.15f, kPlaceSeeds[v] + m * 16u);
            }
        }

        // --- Acciones globales ---
        // Salto: tono ascendente
        m_jump.wave = mge::Waveform::Square;
        m_jump.duty = 0.35f;
        m_jump.baseFrequency = 300.0f;
        m_jump.slide = 6.0f;
        m_jump.attack = 0.005f;
        m_jump.sustain = 0.05f;
        m_jump.decay = 0.14f;
        m_jump.highPassCutoff = 120.0f;
        m_jump.gain = 0.35f;

        // Aterrizaje: impacto grave
        m_land.wave = mge::Waveform::Noise;
        m_land.baseFrequency = 120.0f;
        m_land.slide = -2.0f;
        m_land.attack = 0.002f;
        m_land.sustain = 0.03f;
        m_land.punch = 0.5f;
        m_land.decay = 0.15f;
        m_land.lowPassCutoff = 800.0f;
        m_land.lowPassSweep = -3.0f;
        m_land.gain = 0.5f;

        // Splash al entrar en agua: ruido con phaser y caída
        m_splash.wave = mge::Waveform::Noise;
        m_splash.baseFrequency = 500.0f;
        m_splash.slide = -2.5f;
        m_splash.attack = 0.01f;
        m_splash.sustain = 0.08f;
        m_splash.punch = 0.4f;
        m_splash.decay = 0.25f;
        m_splash.phaserOffset = 2.5f;
        m_splash.phaserSweep = 5.0f;
        m_splash.lowPassCutoff = 2000.0f;
        m_splash.lowPassSweep = -3.0f;
        m_splash.gain = 0.45f;

        // Lanzamiento de cuerpo rígido: siseo/whoosh rápido
        m_throw.wave = mge::Waveform::Noise;
        m_throw.baseFrequency = 240.0f;
        m_throw.slide = 2.0f;
        m_throw.deltaSlide = -8.0f;
        m_throw.attack = 0.02f;
        m_throw.sustain = 0.04f;
        m_throw.decay = 0.12f;
        m_throw.lowPassCutoff = 1600.0f;
        m_throw.lowPassSweep = -2.0f;
        m_throw.gain = 0.35f;
    }

    std::vector<mge::SfxParams> SandboxSounds::allParams() const {
        std::vector<mge::SfxParams> all;
        all.reserve(kMaterialCount * (kFootstepVariants + kBreakVariants + kPlaceVariants) + 4);
        for (std::size_t m = 0; m < kMaterialCount; ++m) {
            const auto& mat = m_materials[m];
            for (std::size_t v = 0; v < kFootstepVariants; ++v) all.push_back(mat.footsteps[v]);
            for (std::size_t v = 0; v < kBreakVariants; ++v) all.push_back(mat.breakVariants[v]);
            for (std::size_t v = 0; v < kPlaceVariants; ++v) all.push_back(mat.placeVariants[v]);
        }
        all.push_back(m_jump);
        all.push_back(m_land);
        all.push_back(m_splash);
        all.push_back(m_throw);
        return all;
    }

    RegisteredSounds registerSandboxSounds(mge::Mixer& mixer, const SandboxSounds& sounds) {
        RegisteredSounds reg;
        const int rate = mixer.sampleRate();
        for (std::size_t m = 0; m < SandboxSounds::kMaterialCount; ++m) {
            const auto& mat = sounds.material(static_cast<SoundMaterial>(m));
            for (std::size_t v = 0; v < SandboxSounds::kFootstepVariants; ++v) {
                reg.materials[m].footsteps[v] = mixer.addSample(mge::renderSfx(mat.footsteps[v], rate));
            }
            for (std::size_t v = 0; v < SandboxSounds::kBreakVariants; ++v) {
                reg.materials[m].breakVariants[v] =
                    mixer.addSample(mge::renderSfx(mat.breakVariants[v], rate));
            }
            for (std::size_t v = 0; v < SandboxSounds::kPlaceVariants; ++v) {
                reg.materials[m].placeVariants[v] =
                    mixer.addSample(mge::renderSfx(mat.placeVariants[v], rate));
            }
        }
        reg.jump = mixer.addSample(mge::renderSfx(sounds.jump(), rate));
        reg.land = mixer.addSample(mge::renderSfx(sounds.land(), rate));
        reg.splash = mixer.addSample(mge::renderSfx(sounds.splash(), rate));
        reg.throwSound = mixer.addSample(mge::renderSfx(sounds.throwSound(), rate));
        return reg;
    }

    RegisteredSounds registerSandboxSounds(mge::AudioModule& audio, const SandboxSounds& sounds) {
        return registerSandboxSounds(audio.mixer(), sounds);
    }

} // namespace sandbox

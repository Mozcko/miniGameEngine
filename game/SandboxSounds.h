#pragma once

#include <mge/synth/Mixer.h>
#include <mge/synth/Sfx.h>
#include <mge/voxel/Block.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace mge {
    class AudioModule;
}

namespace sandbox {

    // Materiales sonoros para pasos y manipulación de bloques
    enum class SoundMaterial : std::uint8_t {
        Grass = 0,
        Stone = 1,
        Sand = 2,
        Wood = 3,
        Leaves = 4,
        Count = 5
    };

    // Clasificación de un bloque a partir de su nombre o id
    SoundMaterial soundMaterialForBlock(std::string_view name);
    SoundMaterial soundMaterialForBlock(mge::BlockId id, const mge::BlockRegistry& registry);

    // Conjunto de efectos de sonido sintetizados del sandbox (sin archivos)
    class SandboxSounds {
    public:
        static constexpr std::size_t kFootstepVariants = 4;
        static constexpr std::size_t kBreakVariants = 4;
        static constexpr std::size_t kPlaceVariants = 4;
        static constexpr std::size_t kMaterialCount = static_cast<std::size_t>(SoundMaterial::Count);

        struct MaterialSounds {
            std::array<mge::SfxParams, kFootstepVariants> footsteps;
            std::array<mge::SfxParams, kBreakVariants> breakVariants;
            std::array<mge::SfxParams, kPlaceVariants> placeVariants;
        };

        SandboxSounds();

        const MaterialSounds& material(SoundMaterial mat) const {
            return m_materials[static_cast<std::size_t>(mat)];
        }

        const mge::SfxParams& footstep(SoundMaterial mat, std::size_t variant) const {
            return m_materials[static_cast<std::size_t>(mat)].footsteps[variant % kFootstepVariants];
        }

        const mge::SfxParams& breakSound(SoundMaterial mat, std::size_t variant = 0) const {
            return m_materials[static_cast<std::size_t>(mat)].breakVariants[variant % kBreakVariants];
        }

        const mge::SfxParams& placeSound(SoundMaterial mat, std::size_t variant = 0) const {
            return m_materials[static_cast<std::size_t>(mat)].placeVariants[variant % kPlaceVariants];
        }

        const mge::SfxParams& jump() const { return m_jump; }
        const mge::SfxParams& land() const { return m_land; }
        const mge::SfxParams& splash() const { return m_splash; }
        const mge::SfxParams& throwSound() const { return m_throw; }

        // Retorna todos los parámetros de efectos para verificación o precarga
        std::vector<mge::SfxParams> allParams() const;

    private:
        std::array<MaterialSounds, kMaterialCount> m_materials{};
        mge::SfxParams m_jump;
        mge::SfxParams m_land;
        mge::SfxParams m_splash;
        mge::SfxParams m_throw;
    };

    // Identificadores de samples registrados en el mezclador/módulo de audio
    struct RegisteredSounds {
        struct MaterialIds {
            std::array<mge::SampleId, SandboxSounds::kFootstepVariants> footsteps{};
            std::array<mge::SampleId, SandboxSounds::kBreakVariants> breakVariants{};
            std::array<mge::SampleId, SandboxSounds::kPlaceVariants> placeVariants{};
        };
        std::array<MaterialIds, SandboxSounds::kMaterialCount> materials{};
        mge::SampleId jump = mge::kInvalidSample;
        mge::SampleId land = mge::kInvalidSample;
        mge::SampleId splash = mge::kInvalidSample;
        mge::SampleId throwSound = mge::kInvalidSample;
    };

    // Registra todos los sonidos en el Mixer o AudioModule sin cargar archivos
    RegisteredSounds registerSandboxSounds(mge::Mixer& mixer, const SandboxSounds& sounds);
    RegisteredSounds registerSandboxSounds(mge::AudioModule& audio, const SandboxSounds& sounds);

} // namespace sandbox

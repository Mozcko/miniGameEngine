#pragma once

#include <mge/gltf/GltfModel.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>

namespace mge {

    struct GltfLoadOptions {
        // glTF UVs have (0, 0) at top-left, while engine MeshData / Texture2D use bottom-left.
        // Flipping V (v = 1 - v) makes loaded meshes match Texture2D automatically.
        bool flipUvY = true;
        // Recompute smooth normals when a primitive omits the NORMAL attribute.
        bool generateMissingNormals = true;
    };

    // Loads a glTF 2.0 model from memory (auto-detects binary .glb vs JSON .gltf).
    // baseDir is used to resolve external .bin buffer URIs in JSON .gltf files (optional).
    // Returns std::nullopt on error (reason is logged via MGE_LOG_ERROR).
    std::optional<GltfModel> loadGltfFromMemory(const std::uint8_t* data, std::size_t size,
                                                const std::filesystem::path& baseDir = {},
                                                const GltfLoadOptions& options = {});

    // Loads a glTF 2.0 (.gltf or .glb) model from disk.
    // Relative paths are resolved through mge::assetPath (ADR 0004).
    // Returns std::nullopt on error (reason is logged via MGE_LOG_ERROR).
    std::optional<GltfModel> loadGltfFile(const std::filesystem::path& path,
                                          const GltfLoadOptions& options = {});
}

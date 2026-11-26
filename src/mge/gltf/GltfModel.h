#pragma once

#include <mge/core/ecs/Transform.h>
#include <mge/core/math/Geometry.h>
#include <mge/procgen/MeshData.h>

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    // glTF 2.0 material alpha rendering mode
    enum class GltfAlphaMode {
        Opaque,
        Mask,
        Blend
    };

    // glTF 2.0 sampler wrapping modes (GL constants from the glTF 2.0 specification)
    enum class GltfWrapMode : std::uint32_t {
        Repeat = 10497,
        ClampToEdge = 33071,
        MirroredRepeat = 33648
    };

    // glTF 2.0 sampler filter modes (GL constants from the glTF 2.0 specification)
    enum class GltfFilterMode : std::uint32_t {
        Default = 0,
        Nearest = 9728,
        Linear = 9729,
        NearestMipmapNearest = 9984,
        LinearMipmapNearest = 9985,
        NearestMipmapLinear = 9986,
        LinearMipmapLinear = 9987
    };

    struct GltfSampler {
        GltfFilterMode magFilter = GltfFilterMode::Default;
        GltfFilterMode minFilter = GltfFilterMode::Default;
        GltfWrapMode wrapS = GltfWrapMode::Repeat;
        GltfWrapMode wrapT = GltfWrapMode::Repeat;
    };

    // Raw image reference from glTF. Headless: stores URI or encoded PNG/JPEG bytes from a GLB
    // bufferView or data URI. Decoding to RGBA pixels happens in the graphics module when uploading.
    struct GltfImage {
        std::string name;
        std::string uri;
        std::string mimeType;
        std::vector<std::uint8_t> encodedBytes;
    };

    struct GltfTexture {
        std::string name;
        int imageIndex = -1;
        int samplerIndex = -1;
    };

    // Metallic-roughness PBR material (glTF 2.0 core)
    struct GltfMaterial {
        std::string name;
        glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
        int baseColorTexture = -1;
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        int metallicRoughnessTexture = -1;
        int normalTexture = -1;
        float normalScale = 1.0f;
        glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f};
        int emissiveTexture = -1;
        GltfAlphaMode alphaMode = GltfAlphaMode::Opaque;
        float alphaCutoff = 0.5f;
        bool doubleSided = false;
    };

    // Single draw primitive inside a GltfMesh.
    // Core geometry lives in MeshData (positions, normals, colors, uvs, indices).
    // Optional tangents and skinning attributes (joints, weights) are preserved when present.
    struct GltfPrimitive {
        MeshData mesh;
        int materialIndex = -1;
        std::vector<glm::vec4> tangents;
        std::vector<glm::u16vec4> joints;
        std::vector<glm::vec4> weights;
    };

    struct GltfMesh {
        std::string name;
        std::vector<GltfPrimitive> primitives;
    };

    // Node in the glTF scene graph
    struct GltfNode {
        std::string name;
        Transform transform;
        int meshIndex = -1;
        int skinIndex = -1;
        std::vector<int> children;
    };

    struct GltfScene {
        std::string name;
        std::vector<int> rootNodes;
    };

    // Headless CPU representation of a loaded glTF 2.0 / GLB model.
    struct GltfModel {
        std::vector<GltfScene> scenes;
        int defaultScene = -1;
        std::vector<GltfNode> nodes;
        std::vector<GltfMesh> meshes;
        std::vector<GltfMaterial> materials;
        std::vector<GltfTexture> textures;
        std::vector<GltfSampler> samplers;
        std::vector<GltfImage> images;

        bool empty() const { return meshes.empty() && nodes.empty(); }
        bool valid() const { return !empty(); }

        // Computes one world-space transform matrix per node (index matches nodes[]).
        // Traverses the active scene's root nodes (or all root nodes if no scenes are present).
        // Protects against cycles in malformed files.
        std::vector<glm::mat4> computeWorldTransforms() const;

        // World-space bounding box of all mesh primitives referenced by nodes (or all meshes if
        // there are no nodes). Empty Aabb if there are no vertices.
        Aabb bounds() const;
    };
}

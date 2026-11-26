#pragma once

#include <mge/core/ecs/Entity.h>
#include <mge/core/ecs/Registry.h>
#include <mge/core/ecs/Transform.h>
#include <mge/core/ecs/World.h>
#include <mge/core/math/Geometry.h>
#include <mge/gltf/GltfModel.h>
#include <mge/gpu/GpuDevice.h>
#include <mge/gpu/GpuMesh.h>
#include <mge/graphics/RenderQueue.h>
#include <mge/graphics/Texture2D.h>

#include <filesystem>
#include <string>
#include <vector>

namespace mge {

    // One uploaded draw primitive inside a GpuModelMesh.
    struct GpuModelPrimitive {
        GpuMesh mesh;
        int textureIndex = -1; // Index into GpuModel::textures (-1 = untextured)
        MeshPass pass = MeshPass::Opaque;
        bool doubleSided = false;
        Aabb localBounds;
    };

    // Uploaded mesh (one or more primitives) inside a GpuModel.
    struct GpuModelMesh {
        std::string name;
        std::vector<GpuModelPrimitive> primitives;
    };

    // Scene-graph node preserved for ECS instantiation.
    struct GpuModelNode {
        std::string name;
        Transform transform;
        int meshIndex = -1;
        std::vector<int> children;
    };

    // GPU-resident representation of a loaded GltfModel (meshes + decoded Texture2D images + node hierarchy).
    // Move-only because it owns GpuMesh and Texture2D handles. Release (or call clear()) before GpuDevice
    // destruction (e.g. in Application::onShutdown()).
    struct GpuModel {
        std::vector<Texture2D> textures;
        std::vector<GpuModelMesh> meshes;
        std::vector<GpuModelNode> nodes;
        std::vector<int> rootNodes;

        bool empty() const { return meshes.empty() && nodes.empty(); }
        bool valid() const { return !empty(); }
        void clear();
    };

    // Uploads a CPU GltfModel to the GPU: decodes embedded/external images to Texture2D via stb_image,
    // bakes material baseColorFactor into vertex colors, uploads each primitive to GpuMesh, and maps
    // GltfAlphaMode to MeshPass (Opaque / Cutout / Transparent).
    // baseDir is used to resolve relative external image URIs (optional).
    GpuModel uploadModel(GpuDevice& device, const GltfModel& model,
                         const std::filesystem::path& baseDir = {});

    // ECS drawable component referencing one mesh inside a GpuModel.
    // The GpuModel must outlive entities that reference it and be released before GpuDevice shuts down.
    struct ModelMeshComponent {
        const GpuModel* model = nullptr;
        int meshIndex = -1;
        MeshMaterial material;
    };

    // Entities created by instantiateModel(): `entities` is indexed 1-to-1 with `model.nodes`.
    struct ModelInstance {
        std::vector<Entity> entities;
        std::vector<Entity> rootEntities;
    };

    // Creates one entity per node in `model` with Transform, Parent (for child nodes), and
    // ModelMeshComponent (for nodes with a mesh). Run TransformSystem::update() before rendering.
    ModelInstance instantiateModel(Registry& registry, const GpuModel& model);
    ModelInstance instantiateModel(World& world, const GpuModel& model);

    // Enqueues all entities that have <WorldTransform, ModelMeshComponent> into `queue`.
    void enqueueModelEntities(Registry& registry, RenderQueue& queue);
    void enqueueModelEntities(World& world, RenderQueue& queue);

} // namespace mge

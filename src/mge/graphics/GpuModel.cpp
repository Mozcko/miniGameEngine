#include <mge/graphics/GpuModel.h>

#include <mge/core/Log.h>

#include <cstddef>

namespace mge {

    namespace {

        MeshPass toMeshPass(GltfAlphaMode mode) {
            switch (mode) {
            case GltfAlphaMode::Mask: return MeshPass::Cutout;
            case GltfAlphaMode::Blend: return MeshPass::Transparent;
            case GltfAlphaMode::Opaque: break;
            }
            return MeshPass::Opaque;
        }

    } // namespace

    void GpuModel::clear() {
        meshes.clear();
        textures.clear();
        nodes.clear();
        rootNodes.clear();
    }

    GpuModel uploadModel(GpuDevice& device, const GltfModel& model, const std::filesystem::path& baseDir) {
        GpuModel out;
        if (model.empty()) return out;

        // 1. Decode embedded or external images into GPU Texture2D objects
        out.textures.reserve(model.images.size());
        for (const GltfImage& img : model.images) {
            Texture2D tex;
            if (!img.encodedBytes.empty()) {
                tex = Texture2D::fromMemory(device, img.encodedBytes.data(), img.encodedBytes.size());
            } else if (!img.uri.empty()) {
                const std::filesystem::path uriPath(img.uri);
                const std::filesystem::path resolved =
                    (!baseDir.empty() && uriPath.is_relative()) ? (baseDir / uriPath) : uriPath;
                tex = Texture2D::fromFile(device, resolved);
            }
            out.textures.push_back(std::move(tex));
        }

        // 2. Upload mesh primitives with material baseColorFactor baked into vertex colors
        out.meshes.reserve(model.meshes.size());
        for (const GltfMesh& srcMesh : model.meshes) {
            GpuModelMesh dstMesh;
            dstMesh.name = srcMesh.name;
            dstMesh.primitives.reserve(srcMesh.primitives.size());

            for (const GltfPrimitive& srcPrim : srcMesh.primitives) {
                if (srcPrim.mesh.empty()) continue;

                GpuModelPrimitive dstPrim;
                glm::vec4 factor(1.0f);
                if (srcPrim.materialIndex >= 0 &&
                    static_cast<std::size_t>(srcPrim.materialIndex) < model.materials.size()) {
                    const GltfMaterial& mat =
                        model.materials[static_cast<std::size_t>(srcPrim.materialIndex)];
                    factor = mat.baseColorFactor;
                    dstPrim.pass = toMeshPass(mat.alphaMode);
                    dstPrim.doubleSided = mat.doubleSided;

                    if (mat.baseColorTexture >= 0 &&
                        static_cast<std::size_t>(mat.baseColorTexture) < model.textures.size()) {
                        const int imgIdx =
                            model.textures[static_cast<std::size_t>(mat.baseColorTexture)].imageIndex;
                        if (imgIdx >= 0 && static_cast<std::size_t>(imgIdx) < out.textures.size() &&
                            out.textures[static_cast<std::size_t>(imgIdx)]) {
                            dstPrim.textureIndex = imgIdx;
                        }
                    }
                }

                dstPrim.localBounds = srcPrim.mesh.bounds();
                if (factor != glm::vec4(1.0f)) {
                    MeshData tinted = srcPrim.mesh;
                    for (glm::vec4& c : tinted.colors) {
                        c *= factor;
                    }
                    dstPrim.mesh = device.uploadMesh(tinted);
                } else {
                    dstPrim.mesh = device.uploadMesh(srcPrim.mesh);
                }

                if (!dstPrim.mesh.empty()) {
                    dstMesh.primitives.push_back(std::move(dstPrim));
                }
            }
            out.meshes.push_back(std::move(dstMesh));
        }

        // 3. Copy node hierarchy and root node list
        out.nodes.reserve(model.nodes.size());
        std::vector<bool> hasParent(model.nodes.size(), false);
        for (const GltfNode& srcNode : model.nodes) {
            GpuModelNode dstNode;
            dstNode.name = srcNode.name;
            dstNode.transform = srcNode.transform;
            dstNode.meshIndex = srcNode.meshIndex;
            dstNode.children = srcNode.children;
            for (const int childIdx : srcNode.children) {
                if (childIdx >= 0 && static_cast<std::size_t>(childIdx) < hasParent.size()) {
                    hasParent[static_cast<std::size_t>(childIdx)] = true;
                }
            }
            out.nodes.push_back(std::move(dstNode));
        }

        if (model.defaultScene >= 0 && static_cast<std::size_t>(model.defaultScene) < model.scenes.size()) {
            out.rootNodes = model.scenes[static_cast<std::size_t>(model.defaultScene)].rootNodes;
        } else if (!model.scenes.empty()) {
            out.rootNodes = model.scenes[0].rootNodes;
        } else {
            for (std::size_t i = 0; i < out.nodes.size(); ++i) {
                if (!hasParent[i]) out.rootNodes.push_back(static_cast<int>(i));
            }
        }

        return out;
    }

    ModelInstance instantiateModel(Registry& registry, const GpuModel& model) {
        ModelInstance instance;
        if (model.nodes.empty()) return instance;

        instance.entities.resize(model.nodes.size());
        for (std::size_t i = 0; i < model.nodes.size(); ++i) {
            const GpuModelNode& node = model.nodes[i];
            const Entity e = registry.create();
            registry.add<Transform>(e, node.transform);
            if (node.meshIndex >= 0 && static_cast<std::size_t>(node.meshIndex) < model.meshes.size()) {
                registry.add<ModelMeshComponent>(e, ModelMeshComponent{&model, node.meshIndex, {}});
            }
            instance.entities[i] = e;
        }

        for (std::size_t i = 0; i < model.nodes.size(); ++i) {
            const Entity parentEntity = instance.entities[i];
            for (const int childIdx : model.nodes[i].children) {
                if (childIdx < 0 || static_cast<std::size_t>(childIdx) >= instance.entities.size()) {
                    continue;
                }
                const Entity childEntity = instance.entities[static_cast<std::size_t>(childIdx)];
                if (!registry.has<Parent>(childEntity)) {
                    registry.add<Parent>(childEntity, Parent{parentEntity});
                }
            }
        }

        for (const int rootIdx : model.rootNodes) {
            if (rootIdx >= 0 && static_cast<std::size_t>(rootIdx) < instance.entities.size()) {
                instance.rootEntities.push_back(instance.entities[static_cast<std::size_t>(rootIdx)]);
            }
        }

        return instance;
    }

    ModelInstance instantiateModel(World& world, const GpuModel& model) {
        return instantiateModel(world.registry(), model);
    }

    void enqueueModelEntities(Registry& registry, RenderQueue& queue) {
        registry.each<WorldTransform, ModelMeshComponent>(
            [&](Entity, const WorldTransform& worldTf, const ModelMeshComponent& comp) {
                if (!comp.model || comp.meshIndex < 0 ||
                    static_cast<std::size_t>(comp.meshIndex) >= comp.model->meshes.size()) {
                    return;
                }
                const GpuModelMesh& gpuMesh = comp.model->meshes[static_cast<std::size_t>(comp.meshIndex)];
                for (const GpuModelPrimitive& prim : gpuMesh.primitives) {
                    if (prim.mesh.empty()) continue;
                    const Texture2D* tex = nullptr;
                    if (prim.textureIndex >= 0 &&
                        static_cast<std::size_t>(prim.textureIndex) < comp.model->textures.size()) {
                        tex = &comp.model->textures[static_cast<std::size_t>(prim.textureIndex)];
                    }
                    const glm::vec3 center =
                        glm::vec3(worldTf.matrix * glm::vec4(prim.localBounds.center(), 1.0f));
                    queue.add(DrawItem{&prim.mesh, worldTf.matrix, tex, prim.pass, center, comp.material});
                }
            });
    }

    void enqueueModelEntities(World& world, RenderQueue& queue) {
        enqueueModelEntities(world.registry(), queue);
    }

} // namespace mge

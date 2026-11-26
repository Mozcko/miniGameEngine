#include <mge/gltf/GltfLoader.h>

#include <mge/core/Assets.h>
#include <mge/core/Log.h>
#include <mge/procgen/MeshOps.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace mge {

    namespace {

        struct CgltfGuard {
            cgltf_data* data = nullptr;
            ~CgltfGuard() {
                if (data) cgltf_free(data);
            }
            CgltfGuard() = default;
            CgltfGuard(const CgltfGuard&) = delete;
            CgltfGuard& operator=(const CgltfGuard&) = delete;
        };

        const char* cgltfResultName(cgltf_result r) {
            switch (r) {
            case cgltf_result_success: return "success";
            case cgltf_result_data_too_short: return "data too short";
            case cgltf_result_unknown_format: return "unknown format";
            case cgltf_result_invalid_json: return "invalid JSON";
            case cgltf_result_invalid_gltf: return "invalid glTF";
            case cgltf_result_invalid_options: return "invalid options";
            case cgltf_result_file_not_found: return "file not found";
            case cgltf_result_io_error: return "I/O error";
            case cgltf_result_out_of_memory: return "out of memory";
            case cgltf_result_legacy_gltf: return "legacy glTF (version < 2.0)";
            default: return "unknown error";
            }
        }

        template <typename T> int indexOfPtr(const T* ptr, const T* arrayBase, cgltf_size count) {
            if (!ptr || !arrayBase) return -1;
            const std::ptrdiff_t idx = ptr - arrayBase;
            if (idx < 0 || static_cast<cgltf_size>(idx) >= count) return -1;
            return static_cast<int>(idx);
        }

        GltfWrapMode toWrapMode(cgltf_int mode) {
            switch (mode) {
            case 33071: return GltfWrapMode::ClampToEdge;
            case 33648: return GltfWrapMode::MirroredRepeat;
            default: return GltfWrapMode::Repeat;
            }
        }

        GltfFilterMode toFilterMode(cgltf_int mode) {
            switch (mode) {
            case 9728: return GltfFilterMode::Nearest;
            case 9729: return GltfFilterMode::Linear;
            case 9984: return GltfFilterMode::NearestMipmapNearest;
            case 9985: return GltfFilterMode::LinearMipmapNearest;
            case 9986: return GltfFilterMode::NearestMipmapLinear;
            case 9987: return GltfFilterMode::LinearMipmapLinear;
            default: return GltfFilterMode::Default;
            }
        }

        GltfAlphaMode toAlphaMode(cgltf_alpha_mode mode) {
            switch (mode) {
            case cgltf_alpha_mode_mask: return GltfAlphaMode::Mask;
            case cgltf_alpha_mode_blend: return GltfAlphaMode::Blend;
            default: return GltfAlphaMode::Opaque;
            }
        }

        // Decomposes a column-major 4x4 affine matrix into TRS (translation, rotation, scale).
        Transform decomposeMatrix(const float m[16]) {
            Transform tf;
            const glm::mat4 mat = glm::make_mat4(m);
            tf.position = glm::vec3(mat[3]);

            glm::vec3 col0(mat[0]);
            glm::vec3 col1(mat[1]);
            glm::vec3 col2(mat[2]);
            float sx = glm::length(col0);
            float sy = glm::length(col1);
            float sz = glm::length(col2);

            const glm::mat3 upper(col0, col1, col2);
            if (glm::determinant(upper) < 0.0f) {
                sx = -sx;
            }
            tf.scale = glm::vec3(sx, sy, sz);

            glm::mat3 rotMat(1.0f);
            if (std::abs(sx) > 1e-6f) rotMat[0] = col0 / sx;
            if (std::abs(sy) > 1e-6f) rotMat[1] = col1 / sy;
            if (std::abs(sz) > 1e-6f) rotMat[2] = col2 / sz;
            tf.rotation = glm::normalize(glm::quat_cast(rotMat));
            return tf;
        }

        // Decodes a data:<mime>;base64,<payload> URI into raw bytes (returns false if not a base64 data URI).
        bool decodeDataUri(std::string_view uri, std::string& outMime, std::vector<std::uint8_t>& outBytes) {
            if (uri.rfind("data:", 0) != 0) return false;
            const std::size_t comma = uri.find(',');
            if (comma == std::string_view::npos) return false;
            const std::string_view header = uri.substr(5, comma - 5);
            const std::size_t semi = header.find(";base64");
            if (semi == std::string_view::npos) return false;
            outMime = std::string(header.substr(0, semi));

            const std::string_view payload = uri.substr(comma + 1);
            if (payload.empty()) {
                outBytes.clear();
                return true;
            }
            // Compute exact decoded byte length
            std::size_t padding = 0;
            if (!payload.empty() && payload.back() == '=') ++padding;
            if (payload.size() >= 2 && payload[payload.size() - 2] == '=') ++padding;
            if (payload.size() % 4 != 0) return false;
            const std::size_t byteLen = (payload.size() / 4) * 3 - padding;

            cgltf_options opts{};
            void* decoded = nullptr;
            const cgltf_result res = cgltf_load_buffer_base64(&opts, byteLen, payload.data(), &decoded);
            if (res != cgltf_result_success || !decoded) return false;
            outBytes.resize(byteLen);
            if (byteLen > 0) std::memcpy(outBytes.data(), decoded, byteLen);
            std::free(decoded);
            return true;
        }

        bool loadPrimitive(const cgltf_primitive& src, const cgltf_data& gltf, const GltfLoadOptions& options,
                           GltfPrimitive& outPrim) {
            if (src.type != cgltf_primitive_type_triangles) {
                MGE_LOG_WARN("Gltf", "Skipping non-triangle primitive (mode ", static_cast<int>(src.type),
                             ")");
                return false;
            }

            const cgltf_accessor* posAcc = nullptr;
            const cgltf_accessor* normAcc = nullptr;
            const cgltf_accessor* uvAcc = nullptr;
            const cgltf_accessor* colorAcc = nullptr;
            const cgltf_accessor* tangentAcc = nullptr;
            const cgltf_accessor* jointsAcc = nullptr;
            const cgltf_accessor* weightsAcc = nullptr;

            for (cgltf_size i = 0; i < src.attributes_count; ++i) {
                const cgltf_attribute& attr = src.attributes[i];
                if (!attr.data) continue;
                switch (attr.type) {
                case cgltf_attribute_type_position: posAcc = attr.data; break;
                case cgltf_attribute_type_normal: normAcc = attr.data; break;
                case cgltf_attribute_type_texcoord:
                    if (attr.index == 0) uvAcc = attr.data;
                    break;
                case cgltf_attribute_type_color:
                    if (attr.index == 0) colorAcc = attr.data;
                    break;
                case cgltf_attribute_type_tangent: tangentAcc = attr.data; break;
                case cgltf_attribute_type_joints:
                    if (attr.index == 0) jointsAcc = attr.data;
                    break;
                case cgltf_attribute_type_weights:
                    if (attr.index == 0) weightsAcc = attr.data;
                    break;
                default: break;
                }
            }

            if (!posAcc || posAcc->count == 0 || posAcc->type != cgltf_type_vec3) {
                MGE_LOG_ERROR("Gltf", "Primitive is missing a valid VEC3 POSITION accessor");
                return false;
            }

            const std::size_t vertCount = static_cast<std::size_t>(posAcc->count);
            MeshData& mesh = outPrim.mesh;
            mesh.positions.resize(vertCount);
            mesh.normals.assign(vertCount, glm::vec3(0.0f, 1.0f, 0.0f));
            mesh.colors.assign(vertCount, glm::vec4(1.0f));
            mesh.uvs.assign(vertCount, glm::vec2(0.0f));

            std::vector<cgltf_float> tmp(vertCount * 4);

            if (cgltf_accessor_unpack_floats(posAcc, tmp.data(), vertCount * 3) < vertCount * 3) {
                MGE_LOG_ERROR("Gltf", "Failed to unpack POSITION accessor");
                return false;
            }
            for (std::size_t i = 0; i < vertCount; ++i) {
                mesh.positions[i] = {tmp[i * 3 + 0], tmp[i * 3 + 1], tmp[i * 3 + 2]};
            }

            bool hasNormals = false;
            if (normAcc && normAcc->count == posAcc->count && normAcc->type == cgltf_type_vec3) {
                if (cgltf_accessor_unpack_floats(normAcc, tmp.data(), vertCount * 3) == vertCount * 3) {
                    for (std::size_t i = 0; i < vertCount; ++i) {
                        mesh.normals[i] = {tmp[i * 3 + 0], tmp[i * 3 + 1], tmp[i * 3 + 2]};
                    }
                    hasNormals = true;
                }
            }

            if (uvAcc && uvAcc->count == posAcc->count && uvAcc->type == cgltf_type_vec2) {
                if (cgltf_accessor_unpack_floats(uvAcc, tmp.data(), vertCount * 2) == vertCount * 2) {
                    for (std::size_t i = 0; i < vertCount; ++i) {
                        const float u = tmp[i * 2 + 0];
                        const float v = options.flipUvY ? (1.0f - tmp[i * 2 + 1]) : tmp[i * 2 + 1];
                        mesh.uvs[i] = {u, v};
                    }
                }
            }

            if (colorAcc && colorAcc->count == posAcc->count) {
                if (colorAcc->type == cgltf_type_vec4 &&
                    cgltf_accessor_unpack_floats(colorAcc, tmp.data(), vertCount * 4) == vertCount * 4) {
                    for (std::size_t i = 0; i < vertCount; ++i) {
                        mesh.colors[i] = {tmp[i * 4 + 0], tmp[i * 4 + 1], tmp[i * 4 + 2], tmp[i * 4 + 3]};
                    }
                } else if (colorAcc->type == cgltf_type_vec3 &&
                           cgltf_accessor_unpack_floats(colorAcc, tmp.data(), vertCount * 3) ==
                               vertCount * 3) {
                    for (std::size_t i = 0; i < vertCount; ++i) {
                        mesh.colors[i] = {tmp[i * 3 + 0], tmp[i * 3 + 1], tmp[i * 3 + 2], 1.0f};
                    }
                }
            }

            if (tangentAcc && tangentAcc->count == posAcc->count && tangentAcc->type == cgltf_type_vec4) {
                if (cgltf_accessor_unpack_floats(tangentAcc, tmp.data(), vertCount * 4) == vertCount * 4) {
                    outPrim.tangents.resize(vertCount);
                    for (std::size_t i = 0; i < vertCount; ++i) {
                        outPrim.tangents[i] = {tmp[i * 4 + 0], tmp[i * 4 + 1], tmp[i * 4 + 2],
                                               tmp[i * 4 + 3]};
                    }
                }
            }

            if (jointsAcc && jointsAcc->count == posAcc->count && jointsAcc->type == cgltf_type_vec4) {
                outPrim.joints.resize(vertCount);
                for (std::size_t i = 0; i < vertCount; ++i) {
                    cgltf_uint j[4] = {0, 0, 0, 0};
                    if (cgltf_accessor_read_uint(jointsAcc, static_cast<cgltf_size>(i), j, 4)) {
                        outPrim.joints[i] = {
                            static_cast<std::uint16_t>(j[0]), static_cast<std::uint16_t>(j[1]),
                            static_cast<std::uint16_t>(j[2]), static_cast<std::uint16_t>(j[3])};
                    }
                }
            }

            if (weightsAcc && weightsAcc->count == posAcc->count && weightsAcc->type == cgltf_type_vec4) {
                if (cgltf_accessor_unpack_floats(weightsAcc, tmp.data(), vertCount * 4) == vertCount * 4) {
                    outPrim.weights.resize(vertCount);
                    for (std::size_t i = 0; i < vertCount; ++i) {
                        outPrim.weights[i] = {tmp[i * 4 + 0], tmp[i * 4 + 1], tmp[i * 4 + 2], tmp[i * 4 + 3]};
                    }
                }
            }

            if (src.indices) {
                const std::size_t idxCount = static_cast<std::size_t>(src.indices->count);
                if (idxCount == 0 || idxCount % 3 != 0) {
                    MGE_LOG_ERROR("Gltf", "Primitive index count is not a non-zero multiple of 3");
                    return false;
                }
                mesh.indices.resize(idxCount);
                for (std::size_t i = 0; i < idxCount; ++i) {
                    const cgltf_size idx = cgltf_accessor_read_index(src.indices, static_cast<cgltf_size>(i));
                    if (idx >= vertCount) {
                        MGE_LOG_ERROR("Gltf", "Primitive index out of vertex bounds");
                        return false;
                    }
                    mesh.indices[i] = static_cast<std::uint32_t>(idx);
                }
            } else {
                if (vertCount % 3 != 0) {
                    MGE_LOG_ERROR("Gltf",
                                  "Non-indexed triangle primitive vertex count is not a multiple of 3");
                    return false;
                }
                mesh.indices.resize(vertCount);
                for (std::size_t i = 0; i < vertCount; ++i) {
                    mesh.indices[i] = static_cast<std::uint32_t>(i);
                }
            }

            if (!hasNormals && options.generateMissingNormals) {
                recomputeNormals(mesh, NormalMode::Smooth);
            }

            outPrim.materialIndex = indexOfPtr(src.material, gltf.materials, gltf.materials_count);
            return true;
        }

    } // namespace

    std::optional<GltfModel> loadGltfFromMemory(const std::uint8_t* data, std::size_t size,
                                                const std::filesystem::path& baseDir,
                                                const GltfLoadOptions& options) {
        if (!data || size == 0) {
            MGE_LOG_ERROR("Gltf", "loadGltfFromMemory: empty input buffer");
            return std::nullopt;
        }

        cgltf_options cgltfOpts{};
        CgltfGuard guard;
        cgltf_result res = cgltf_parse(&cgltfOpts, data, size, &guard.data);
        if (res != cgltf_result_success || !guard.data) {
            MGE_LOG_ERROR("Gltf", "cgltf_parse failed: ", cgltfResultName(res));
            return std::nullopt;
        }

        const cgltf_data& gltf = *guard.data;
        if (!gltf.asset.version || std::strncmp(gltf.asset.version, "2.", 2) != 0) {
            MGE_LOG_ERROR("Gltf", "Unsupported glTF asset version: ",
                          gltf.asset.version ? gltf.asset.version : "(missing)");
            return std::nullopt;
        }

        const std::string basePath = baseDir.empty() ? std::string() : (baseDir / "_dummy.gltf").string();
        res = cgltf_load_buffers(&cgltfOpts, guard.data, basePath.c_str());
        if (res != cgltf_result_success) {
            MGE_LOG_ERROR("Gltf", "cgltf_load_buffers failed: ", cgltfResultName(res));
            return std::nullopt;
        }

        res = cgltf_validate(guard.data);
        if (res != cgltf_result_success) {
            MGE_LOG_ERROR("Gltf", "cgltf_validate failed: ", cgltfResultName(res));
            return std::nullopt;
        }

        GltfModel model;

        // 1. Samplers
        model.samplers.reserve(gltf.samplers_count);
        for (cgltf_size i = 0; i < gltf.samplers_count; ++i) {
            const cgltf_sampler& s = gltf.samplers[i];
            GltfSampler out;
            out.magFilter = toFilterMode(s.mag_filter);
            out.minFilter = toFilterMode(s.min_filter);
            out.wrapS = toWrapMode(s.wrap_s);
            out.wrapT = toWrapMode(s.wrap_t);
            model.samplers.push_back(out);
        }

        // 2. Images
        model.images.reserve(gltf.images_count);
        for (cgltf_size i = 0; i < gltf.images_count; ++i) {
            const cgltf_image& img = gltf.images[i];
            GltfImage out;
            if (img.name) out.name = img.name;
            if (img.mime_type) out.mimeType = img.mime_type;

            if (img.buffer_view && img.buffer_view->buffer && img.buffer_view->buffer->data) {
                const auto* base = static_cast<const std::uint8_t*>(img.buffer_view->buffer->data);
                const cgltf_size offset = img.buffer_view->offset;
                const cgltf_size len = img.buffer_view->size;
                if (offset + len <= img.buffer_view->buffer->size) {
                    out.encodedBytes.assign(base + offset, base + offset + len);
                }
            } else if (img.uri) {
                std::string dataMime;
                std::vector<std::uint8_t> dataBytes;
                if (decodeDataUri(img.uri, dataMime, dataBytes)) {
                    if (out.mimeType.empty()) out.mimeType = std::move(dataMime);
                    out.encodedBytes = std::move(dataBytes);
                } else {
                    out.uri = img.uri;
                }
            }
            model.images.push_back(std::move(out));
        }

        // 3. Textures
        model.textures.reserve(gltf.textures_count);
        for (cgltf_size i = 0; i < gltf.textures_count; ++i) {
            const cgltf_texture& tex = gltf.textures[i];
            GltfTexture out;
            if (tex.name) out.name = tex.name;
            out.imageIndex = indexOfPtr(tex.image, gltf.images, gltf.images_count);
            out.samplerIndex = indexOfPtr(tex.sampler, gltf.samplers, gltf.samplers_count);
            model.textures.push_back(std::move(out));
        }

        // 4. Materials
        model.materials.reserve(gltf.materials_count);
        for (cgltf_size i = 0; i < gltf.materials_count; ++i) {
            const cgltf_material& mat = gltf.materials[i];
            GltfMaterial out;
            if (mat.name) out.name = mat.name;
            if (mat.has_pbr_metallic_roughness) {
                const cgltf_pbr_metallic_roughness& pbr = mat.pbr_metallic_roughness;
                out.baseColorFactor = {pbr.base_color_factor[0], pbr.base_color_factor[1],
                                       pbr.base_color_factor[2], pbr.base_color_factor[3]};
                out.baseColorTexture =
                    indexOfPtr(pbr.base_color_texture.texture, gltf.textures, gltf.textures_count);
                out.metallicFactor = pbr.metallic_factor;
                out.roughnessFactor = pbr.roughness_factor;
                out.metallicRoughnessTexture =
                    indexOfPtr(pbr.metallic_roughness_texture.texture, gltf.textures, gltf.textures_count);
            }
            out.normalTexture = indexOfPtr(mat.normal_texture.texture, gltf.textures, gltf.textures_count);
            out.normalScale = mat.normal_texture.scale;
            out.emissiveFactor = {mat.emissive_factor[0], mat.emissive_factor[1], mat.emissive_factor[2]};
            out.emissiveTexture =
                indexOfPtr(mat.emissive_texture.texture, gltf.textures, gltf.textures_count);
            out.alphaMode = toAlphaMode(mat.alpha_mode);
            out.alphaCutoff = mat.alpha_cutoff;
            out.doubleSided = mat.double_sided != 0;
            model.materials.push_back(std::move(out));
        }

        // 5. Meshes
        model.meshes.reserve(gltf.meshes_count);
        for (cgltf_size i = 0; i < gltf.meshes_count; ++i) {
            const cgltf_mesh& m = gltf.meshes[i];
            GltfMesh outMesh;
            if (m.name) outMesh.name = m.name;
            outMesh.primitives.reserve(m.primitives_count);
            for (cgltf_size p = 0; p < m.primitives_count; ++p) {
                GltfPrimitive outPrim;
                if (loadPrimitive(m.primitives[p], gltf, options, outPrim)) {
                    outMesh.primitives.push_back(std::move(outPrim));
                }
            }
            model.meshes.push_back(std::move(outMesh));
        }

        // 6. Nodes
        model.nodes.reserve(gltf.nodes_count);
        for (cgltf_size i = 0; i < gltf.nodes_count; ++i) {
            const cgltf_node& n = gltf.nodes[i];
            GltfNode outNode;
            if (n.name) outNode.name = n.name;
            if (n.has_matrix) {
                outNode.transform = decomposeMatrix(n.matrix);
            } else {
                if (n.has_translation) {
                    outNode.transform.position = {n.translation[0], n.translation[1], n.translation[2]};
                }
                if (n.has_rotation) {
                    // glTF stores quaternion as [x, y, z, w]; glm::quat constructor is (w, x, y, z)
                    outNode.transform.rotation =
                        glm::normalize(glm::quat(n.rotation[3], n.rotation[0], n.rotation[1], n.rotation[2]));
                }
                if (n.has_scale) {
                    outNode.transform.scale = {n.scale[0], n.scale[1], n.scale[2]};
                }
            }
            outNode.meshIndex = indexOfPtr(n.mesh, gltf.meshes, gltf.meshes_count);
            outNode.skinIndex = indexOfPtr(n.skin, gltf.skins, gltf.skins_count);
            outNode.children.reserve(n.children_count);
            for (cgltf_size c = 0; c < n.children_count; ++c) {
                const int childIdx = indexOfPtr(n.children[c], gltf.nodes, gltf.nodes_count);
                if (childIdx >= 0) outNode.children.push_back(childIdx);
            }
            model.nodes.push_back(std::move(outNode));
        }

        // 7. Scenes
        model.scenes.reserve(gltf.scenes_count);
        for (cgltf_size i = 0; i < gltf.scenes_count; ++i) {
            const cgltf_scene& sc = gltf.scenes[i];
            GltfScene outScene;
            if (sc.name) outScene.name = sc.name;
            outScene.rootNodes.reserve(sc.nodes_count);
            for (cgltf_size r = 0; r < sc.nodes_count; ++r) {
                const int nodeIdx = indexOfPtr(sc.nodes[r], gltf.nodes, gltf.nodes_count);
                if (nodeIdx >= 0) outScene.rootNodes.push_back(nodeIdx);
            }
            model.scenes.push_back(std::move(outScene));
        }
        model.defaultScene = indexOfPtr(gltf.scene, gltf.scenes, gltf.scenes_count);

        return model;
    }

    std::optional<GltfModel> loadGltfFile(const std::filesystem::path& path, const GltfLoadOptions& options) {
        const std::filesystem::path resolved =
            path.is_absolute() ? path : std::filesystem::path(assetPath(path.string()));
        std::ifstream file(resolved, std::ios::binary | std::ios::ate);
        if (!file) {
            MGE_LOG_ERROR("Gltf", "Failed to open glTF file: ", resolved.string());
            return std::nullopt;
        }
        const std::streamsize fileSize = file.tellg();
        if (fileSize <= 0) {
            MGE_LOG_ERROR("Gltf", "glTF file is empty: ", resolved.string());
            return std::nullopt;
        }
        file.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), fileSize)) {
            MGE_LOG_ERROR("Gltf", "Failed to read glTF file: ", resolved.string());
            return std::nullopt;
        }
        return loadGltfFromMemory(bytes.data(), bytes.size(), resolved.parent_path(), options);
    }

} // namespace mge

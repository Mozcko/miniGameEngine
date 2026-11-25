#include <mge/gltf/GltfLoader.h>
#include <mge/gltf/detail/GltfJson.h>

#include <mge/core/Assets.h>
#include <mge/core/Log.h>
#include <mge/procgen/MeshOps.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <glm/gtc/quaternion.hpp>

namespace mge {

    namespace {
        using detail::GltfJsonArray;
        using detail::GltfJsonValue;

        constexpr std::uint32_t kGlbMagic = 0x46546C67u; // "glTF"
        constexpr std::uint32_t kGlbVersion = 2u;
        constexpr std::uint32_t kChunkTypeJson = 0x4E4F534Au; // "JSON"
        constexpr std::uint32_t kChunkTypeBin = 0x004E4942u;  // "BIN\0"
        constexpr std::size_t kGlbHeaderSize = 12;
        constexpr std::size_t kGlbChunkHeaderSize = 8;

        constexpr int kComponentByte = 5120;
        constexpr int kComponentUnsignedByte = 5121;
        constexpr int kComponentShort = 5122;
        constexpr int kComponentUnsignedShort = 5123;
        constexpr int kComponentUnsignedInt = 5125;
        constexpr int kComponentFloat = 5126;

        constexpr int kModeTriangles = 4;

        std::uint32_t readU32Le(const std::uint8_t* ptr) {
            return static_cast<std::uint32_t>(ptr[0]) | (static_cast<std::uint32_t>(ptr[1]) << 8) |
                   (static_cast<std::uint32_t>(ptr[2]) << 16) | (static_cast<std::uint32_t>(ptr[3]) << 24);
        }

        int base64CharValue(char c) {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        }

        bool decodeBase64(std::string_view input, std::vector<std::uint8_t>& out) {
            out.clear();
            out.reserve((input.size() / 4) * 3);
            std::uint32_t accum = 0;
            int bits = 0;
            for (char c : input) {
                if (c == '=' || c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
                const int val = base64CharValue(c);
                if (val < 0) return false;
                accum = (accum << 6) | static_cast<std::uint32_t>(val);
                bits += 6;
                if (bits >= 8) {
                    bits -= 8;
                    out.push_back(static_cast<std::uint8_t>((accum >> bits) & 0xFFu));
                }
            }
            return true;
        }

        bool isDataUri(std::string_view uri) { return uri.rfind("data:", 0) == 0; }

        bool decodeDataUri(std::string_view uri, std::vector<std::uint8_t>& out,
                           std::string* mimeOut = nullptr) {
            constexpr std::string_view kPrefix = "data:";
            if (uri.rfind(kPrefix, 0) != 0) return false;
            const std::size_t commaPos = uri.find(',');
            if (commaPos == std::string_view::npos) return false;
            const std::string_view meta = uri.substr(kPrefix.size(), commaPos - kPrefix.size());
            constexpr std::string_view kBase64Tag = ";base64";
            if (meta.size() < kBase64Tag.size() ||
                meta.substr(meta.size() - kBase64Tag.size()) != kBase64Tag) {
                return false;
            }
            if (mimeOut) {
                *mimeOut = std::string(meta.substr(0, meta.size() - kBase64Tag.size()));
            }
            return decodeBase64(uri.substr(commaPos + 1), out);
        }

        bool isSafeRelativeUri(std::string_view uri) {
            if (uri.empty() || uri[0] == '/' || uri[0] == '\\') return false;
            if (uri.find("://") != std::string_view::npos) return false;
            const std::filesystem::path relPath(uri);
            if (relPath.is_absolute()) return false;
            for (const auto& part : relPath) {
                if (part == "..") return false;
            }
            return true;
        }

        bool readFileBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& out) {
            std::ifstream file(path, std::ios::binary);
            if (!file) return false;
            out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            return true;
        }

        std::size_t componentByteSize(int componentType) {
            switch (componentType) {
            case kComponentByte:
            case kComponentUnsignedByte: return 1;
            case kComponentShort:
            case kComponentUnsignedShort: return 2;
            case kComponentUnsignedInt:
            case kComponentFloat: return 4;
            default: return 0;
            }
        }

        std::size_t accessorTypeComponentCount(const std::string& type) {
            if (type == "SCALAR") return 1;
            if (type == "VEC2") return 2;
            if (type == "VEC3") return 3;
            if (type == "VEC4") return 4;
            if (type == "MAT2") return 4;
            if (type == "MAT3") return 9;
            if (type == "MAT4") return 16;
            return 0;
        }

        struct BufferViewInfo {
            int buffer = -1;
            std::size_t byteOffset = 0;
            std::size_t byteLength = 0;
            std::size_t byteStride = 0;
        };

        struct AccessorInfo {
            int bufferView = -1;
            std::size_t byteOffset = 0;
            int componentType = 0;
            bool normalized = false;
            std::size_t count = 0;
            std::size_t numComponents = 0;
            bool hasSparse = false;
        };

        float readComponentAsFloat(const std::uint8_t* ptr, int componentType, bool normalized) {
            switch (componentType) {
            case kComponentFloat: {
                float value = 0.0f;
                std::memcpy(&value, ptr, sizeof(float));
                return value;
            }
            case kComponentByte: {
                std::int8_t value = 0;
                std::memcpy(&value, ptr, sizeof(std::int8_t));
                if (normalized) return std::max(static_cast<float>(value) / 127.0f, -1.0f);
                return static_cast<float>(value);
            }
            case kComponentUnsignedByte: {
                const std::uint8_t value = *ptr;
                if (normalized) return static_cast<float>(value) / 255.0f;
                return static_cast<float>(value);
            }
            case kComponentShort: {
                std::int16_t value = 0;
                std::memcpy(&value, ptr, sizeof(std::int16_t));
                if (normalized) return std::max(static_cast<float>(value) / 32767.0f, -1.0f);
                return static_cast<float>(value);
            }
            case kComponentUnsignedShort: {
                std::uint16_t value = 0;
                std::memcpy(&value, ptr, sizeof(std::uint16_t));
                if (normalized) return static_cast<float>(value) / 65535.0f;
                return static_cast<float>(value);
            }
            case kComponentUnsignedInt: {
                std::uint32_t value = 0;
                std::memcpy(&value, ptr, sizeof(std::uint32_t));
                return static_cast<float>(value);
            }
            default: return 0.0f;
            }
        }

        std::uint32_t readComponentAsUint32(const std::uint8_t* ptr, int componentType) {
            switch (componentType) {
            case kComponentUnsignedByte: return static_cast<std::uint32_t>(*ptr);
            case kComponentUnsignedShort: {
                std::uint16_t value = 0;
                std::memcpy(&value, ptr, sizeof(std::uint16_t));
                return static_cast<std::uint32_t>(value);
            }
            case kComponentUnsignedInt: {
                std::uint32_t value = 0;
                std::memcpy(&value, ptr, sizeof(std::uint32_t));
                return value;
            }
            default: return 0u;
            }
        }

        bool validateAccessorSlice(const AccessorInfo& acc, const std::vector<BufferViewInfo>& views,
                                   const std::vector<std::vector<std::uint8_t>>& buffers,
                                   const std::uint8_t*& basePtrOut, std::size_t& strideOut) {
            const std::size_t compSize = componentByteSize(acc.componentType);
            if (compSize == 0 || acc.numComponents == 0) return false;
            if (acc.count == 0) {
                basePtrOut = nullptr;
                strideOut = 0;
                return true;
            }
            if (acc.bufferView < 0 || static_cast<std::size_t>(acc.bufferView) >= views.size()) {
                return false;
            }
            const BufferViewInfo& view = views[static_cast<std::size_t>(acc.bufferView)];
            if (view.buffer < 0 || static_cast<std::size_t>(view.buffer) >= buffers.size()) return false;
            const std::vector<std::uint8_t>& buf = buffers[static_cast<std::size_t>(view.buffer)];

            const std::size_t elemSize = compSize * acc.numComponents;
            const std::size_t stride = view.byteStride > 0 ? view.byteStride : elemSize;
            if (stride < elemSize) return false;

            if (acc.byteOffset > view.byteLength) return false;
            const std::size_t needed = (acc.count - 1) * stride + elemSize;
            if (needed > view.byteLength - acc.byteOffset) return false;

            const std::size_t totalOffset = view.byteOffset + acc.byteOffset;
            if (totalOffset > buf.size() || needed > buf.size() - totalOffset) return false;

            basePtrOut = buf.data() + totalOffset;
            strideOut = stride;
            return true;
        }

        bool readAccessorFloats(const AccessorInfo& acc, const std::vector<BufferViewInfo>& views,
                                const std::vector<std::vector<std::uint8_t>>& buffers,
                                std::vector<float>& out) {
            if (acc.hasSparse) {
                MGE_LOG_WARN("Gltf", "Sparse accessors are not yet supported; using base data");
            }
            out.assign(acc.count * acc.numComponents, 0.0f);
            if (acc.count == 0 || acc.bufferView < 0) return true;

            const std::uint8_t* basePtr = nullptr;
            std::size_t stride = 0;
            if (!validateAccessorSlice(acc, views, buffers, basePtr, stride)) return false;

            const std::size_t compSize = componentByteSize(acc.componentType);
            for (std::size_t i = 0; i < acc.count; i++) {
                const std::uint8_t* elemPtr = basePtr + i * stride;
                for (std::size_t c = 0; c < acc.numComponents; c++) {
                    out[i * acc.numComponents + c] =
                        readComponentAsFloat(elemPtr + c * compSize, acc.componentType, acc.normalized);
                }
            }
            return true;
        }

        bool readAccessorIndices(const AccessorInfo& acc, const std::vector<BufferViewInfo>& views,
                                 const std::vector<std::vector<std::uint8_t>>& buffers,
                                 std::vector<std::uint32_t>& out) {
            if (acc.numComponents != 1) return false;
            if (acc.componentType != kComponentUnsignedByte && acc.componentType != kComponentUnsignedShort &&
                acc.componentType != kComponentUnsignedInt) {
                return false;
            }
            if (acc.hasSparse) {
                MGE_LOG_WARN("Gltf", "Sparse accessors are not yet supported; using base data");
            }
            out.assign(acc.count, 0u);
            if (acc.count == 0 || acc.bufferView < 0) return true;

            const std::uint8_t* basePtr = nullptr;
            std::size_t stride = 0;
            if (!validateAccessorSlice(acc, views, buffers, basePtr, stride)) return false;

            for (std::size_t i = 0; i < acc.count; i++) {
                out[i] = readComponentAsUint32(basePtr + i * stride, acc.componentType);
            }
            return true;
        }

        bool readAccessorJoints(const AccessorInfo& acc, const std::vector<BufferViewInfo>& views,
                                const std::vector<std::vector<std::uint8_t>>& buffers,
                                std::vector<glm::u16vec4>& out) {
            if (acc.numComponents != 4) return false;
            if (acc.componentType != kComponentUnsignedByte && acc.componentType != kComponentUnsignedShort) {
                return false;
            }
            out.assign(acc.count, glm::u16vec4(0));
            if (acc.count == 0 || acc.bufferView < 0) return true;

            const std::uint8_t* basePtr = nullptr;
            std::size_t stride = 0;
            if (!validateAccessorSlice(acc, views, buffers, basePtr, stride)) return false;

            const std::size_t compSize = componentByteSize(acc.componentType);
            for (std::size_t i = 0; i < acc.count; i++) {
                const std::uint8_t* elemPtr = basePtr + i * stride;
                glm::u16vec4 joints(0);
                for (int c = 0; c < 4; c++) {
                    const std::uint32_t val =
                        readComponentAsUint32(elemPtr + static_cast<std::size_t>(c) * compSize,
                                              acc.componentType);
                    joints[c] = static_cast<std::uint16_t>(val);
                }
                out[i] = joints;
            }
            return true;
        }

        Transform decomposeMatrix(const glm::mat4& matrix) {
            Transform result;
            result.position = glm::vec3(matrix[3]);

            const glm::vec3 col0(matrix[0]);
            const glm::vec3 col1(matrix[1]);
            const glm::vec3 col2(matrix[2]);

            float sx = glm::length(col0);
            const float sy = glm::length(col1);
            const float sz = glm::length(col2);

            if (glm::determinant(glm::mat3(matrix)) < 0.0f) {
                sx = -sx;
            }
            result.scale = glm::vec3(sx, sy, sz);

            glm::mat3 rotMat(1.0f);
            constexpr float kEps = 1e-7f;
            rotMat[0] = std::abs(sx) > kEps ? (col0 / sx) : glm::vec3(1.0f, 0.0f, 0.0f);
            rotMat[1] = std::abs(sy) > kEps ? (col1 / sy) : glm::vec3(0.0f, 1.0f, 0.0f);
            rotMat[2] = std::abs(sz) > kEps ? (col2 / sz) : glm::vec3(0.0f, 0.0f, 1.0f);
            result.rotation = glm::normalize(glm::quat_cast(rotMat));
            return result;
        }

        glm::vec3 readVec3(const GltfJsonValue* value, const glm::vec3& fallback) {
            if (!value || !value->isArray()) return fallback;
            const GltfJsonArray& arr = value->asArray();
            if (arr.size() < 3) return fallback;
            return glm::vec3(static_cast<float>(arr[0].asNumber(fallback.x)),
                             static_cast<float>(arr[1].asNumber(fallback.y)),
                             static_cast<float>(arr[2].asNumber(fallback.z)));
        }

        glm::vec4 readVec4(const GltfJsonValue* value, const glm::vec4& fallback) {
            if (!value || !value->isArray()) return fallback;
            const GltfJsonArray& arr = value->asArray();
            if (arr.size() < 4) return fallback;
            return glm::vec4(static_cast<float>(arr[0].asNumber(fallback.x)),
                             static_cast<float>(arr[1].asNumber(fallback.y)),
                             static_cast<float>(arr[2].asNumber(fallback.z)),
                             static_cast<float>(arr[3].asNumber(fallback.w)));
        }

        int readTextureIndex(const GltfJsonValue* textureInfo) {
            if (!textureInfo || !textureInfo->isObject()) return -1;
            const GltfJsonValue* idx = textureInfo->find("index");
            return (idx && idx->isNumber()) ? idx->asInt(-1) : -1;
        }
    }

    std::optional<GltfModel> loadGltfFromMemory(const std::uint8_t* data, std::size_t size,
                                                const std::filesystem::path& baseDir,
                                                const GltfLoadOptions& options) {
        if (!data || size == 0) {
            MGE_LOG_ERROR("Gltf", "Empty glTF input");
            return std::nullopt;
        }

        std::string_view jsonText;
        std::vector<std::uint8_t> glbBinChunk;
        bool hasGlbBin = false;

        // Detect GLB binary container via 4-byte magic "glTF"
        if (size >= 4 && readU32Le(data) == kGlbMagic) {
            if (size < kGlbHeaderSize) {
                MGE_LOG_ERROR("Gltf", "Truncated GLB header");
                return std::nullopt;
            }
            const std::uint32_t version = readU32Le(data + 4);
            const std::uint32_t totalLength = readU32Le(data + 8);
            if (version != kGlbVersion) {
                MGE_LOG_ERROR("Gltf", "Unsupported GLB version: ", version);
                return std::nullopt;
            }
            if (totalLength < kGlbHeaderSize || totalLength > size) {
                MGE_LOG_ERROR("Gltf", "Invalid GLB length: ", totalLength, " (buffer size: ", size, ")");
                return std::nullopt;
            }

            std::size_t offset = kGlbHeaderSize;
            bool foundJson = false;
            while (offset + kGlbChunkHeaderSize <= totalLength) {
                const std::uint32_t chunkLength = readU32Le(data + offset);
                const std::uint32_t chunkType = readU32Le(data + offset + 4);
                offset += kGlbChunkHeaderSize;
                if (chunkLength > totalLength - offset) {
                    MGE_LOG_ERROR("Gltf", "Truncated GLB chunk");
                    return std::nullopt;
                }
                if (chunkType == kChunkTypeJson && !foundJson) {
                    jsonText =
                        std::string_view(reinterpret_cast<const char*>(data + offset), chunkLength);
                    foundJson = true;
                } else if (chunkType == kChunkTypeBin && !hasGlbBin) {
                    glbBinChunk.assign(data + offset, data + offset + chunkLength);
                    hasGlbBin = true;
                }
                offset += chunkLength;
            }
            if (!foundJson) {
                MGE_LOG_ERROR("Gltf", "GLB file is missing a JSON chunk");
                return std::nullopt;
            }
        } else {
            jsonText = std::string_view(reinterpret_cast<const char*>(data), size);
        }

        std::string jsonError;
        const auto rootOpt = detail::parseGltfJson(jsonText, &jsonError);
        if (!rootOpt || !rootOpt->isObject()) {
            MGE_LOG_ERROR("Gltf", "Failed to parse glTF JSON: ", jsonError);
            return std::nullopt;
        }
        const GltfJsonValue& root = *rootOpt;

        // Validate asset.version starts with "2."
        const GltfJsonValue* assetVal = root.find("asset");
        if (!assetVal || !assetVal->isObject()) {
            MGE_LOG_ERROR("Gltf", "glTF JSON missing required 'asset' object");
            return std::nullopt;
        }
        const GltfJsonValue* versionVal = assetVal->find("version");
        if (!versionVal || !versionVal->isString() || versionVal->asString().rfind("2.", 0) != 0) {
            MGE_LOG_ERROR("Gltf", "Unsupported glTF asset version: ",
                          versionVal ? versionVal->asString() : "<missing>");
            return std::nullopt;
        }

        // --- Buffers ---
        std::vector<std::vector<std::uint8_t>> buffers;
        if (const GltfJsonValue* buffersVal = root.find("buffers"); buffersVal && buffersVal->isArray()) {
            const GltfJsonArray& arr = buffersVal->asArray();
            buffers.resize(arr.size());
            for (std::size_t i = 0; i < arr.size(); i++) {
                const GltfJsonValue& entry = arr[i];
                const GltfJsonValue* uriVal = entry.find("uri");
                if (!uriVal || !uriVal->isString() || uriVal->asString().empty()) {
                    if (i == 0 && hasGlbBin) {
                        buffers[i] = glbBinChunk;
                    } else {
                        MGE_LOG_ERROR("Gltf", "Buffer ", i, " has no URI and no GLB BIN chunk");
                        return std::nullopt;
                    }
                } else {
                    const std::string& uri = uriVal->asString();
                    if (isDataUri(uri)) {
                        if (!decodeDataUri(uri, buffers[i])) {
                            MGE_LOG_ERROR("Gltf", "Failed to decode base64 data URI for buffer ", i);
                            return std::nullopt;
                        }
                    } else {
                        if (baseDir.empty() || !isSafeRelativeUri(uri)) {
                            MGE_LOG_ERROR("Gltf", "Cannot load external buffer URI: ", uri);
                            return std::nullopt;
                        }
                        if (!readFileBytes(baseDir / uri, buffers[i])) {
                            MGE_LOG_ERROR("Gltf", "Failed to read external buffer: ",
                                          (baseDir / uri).string());
                            return std::nullopt;
                        }
                    }
                }
            }
        }

        // --- BufferViews ---
        std::vector<BufferViewInfo> bufferViews;
        if (const GltfJsonValue* viewsVal = root.find("bufferViews"); viewsVal && viewsVal->isArray()) {
            const GltfJsonArray& arr = viewsVal->asArray();
            bufferViews.reserve(arr.size());
            for (const GltfJsonValue& entry : arr) {
                BufferViewInfo info;
                if (const GltfJsonValue* v = entry.find("buffer")) info.buffer = v->asInt(-1);
                if (const GltfJsonValue* v = entry.find("byteOffset")) info.byteOffset = v->asSize(0);
                if (const GltfJsonValue* v = entry.find("byteLength")) info.byteLength = v->asSize(0);
                if (const GltfJsonValue* v = entry.find("byteStride")) info.byteStride = v->asSize(0);
                if (info.buffer < 0 || static_cast<std::size_t>(info.buffer) >= buffers.size()) {
                    MGE_LOG_ERROR("Gltf", "BufferView references invalid buffer index ", info.buffer);
                    return std::nullopt;
                }
                const std::size_t bufSize = buffers[static_cast<std::size_t>(info.buffer)].size();
                if (info.byteOffset > bufSize || info.byteLength > bufSize - info.byteOffset) {
                    MGE_LOG_ERROR("Gltf", "BufferView out of bounds");
                    return std::nullopt;
                }
                bufferViews.push_back(info);
            }
        }

        // --- Accessors ---
        std::vector<AccessorInfo> accessors;
        if (const GltfJsonValue* accVal = root.find("accessors"); accVal && accVal->isArray()) {
            const GltfJsonArray& arr = accVal->asArray();
            accessors.reserve(arr.size());
            for (const GltfJsonValue& entry : arr) {
                AccessorInfo info;
                if (const GltfJsonValue* v = entry.find("bufferView")) info.bufferView = v->asInt(-1);
                if (const GltfJsonValue* v = entry.find("byteOffset")) info.byteOffset = v->asSize(0);
                if (const GltfJsonValue* v = entry.find("componentType")) info.componentType = v->asInt(0);
                if (const GltfJsonValue* v = entry.find("normalized")) info.normalized = v->asBool(false);
                if (const GltfJsonValue* v = entry.find("count")) info.count = v->asSize(0);
                if (const GltfJsonValue* v = entry.find("type")) {
                    info.numComponents = accessorTypeComponentCount(v->asString());
                }
                info.hasSparse = entry.find("sparse") != nullptr;
                accessors.push_back(info);
            }
        }

        GltfModel model;

        // --- Images ---
        if (const GltfJsonValue* imagesVal = root.find("images"); imagesVal && imagesVal->isArray()) {
            for (const GltfJsonValue& entry : imagesVal->asArray()) {
                GltfImage img;
                if (const GltfJsonValue* v = entry.find("name")) img.name = v->asString();
                if (const GltfJsonValue* v = entry.find("mimeType")) img.mimeType = v->asString();
                if (const GltfJsonValue* bv = entry.find("bufferView"); bv && bv->isNumber()) {
                    const int bvIdx = bv->asInt(-1);
                    if (bvIdx >= 0 && static_cast<std::size_t>(bvIdx) < bufferViews.size()) {
                        const BufferViewInfo& view = bufferViews[static_cast<std::size_t>(bvIdx)];
                        const auto& buf = buffers[static_cast<std::size_t>(view.buffer)];
                        img.encodedBytes.assign(buf.begin() + static_cast<std::ptrdiff_t>(view.byteOffset),
                                                buf.begin() + static_cast<std::ptrdiff_t>(view.byteOffset +
                                                                                          view.byteLength));
                    }
                } else if (const GltfJsonValue* uriVal = entry.find("uri"); uriVal && uriVal->isString()) {
                    const std::string& uri = uriVal->asString();
                    if (isDataUri(uri)) {
                        std::string decodedMime;
                        if (decodeDataUri(uri, img.encodedBytes, &decodedMime)) {
                            if (img.mimeType.empty()) img.mimeType = decodedMime;
                        }
                    } else {
                        img.uri = uri;
                    }
                }
                model.images.push_back(std::move(img));
            }
        }

        // --- Samplers ---
        if (const GltfJsonValue* samplersVal = root.find("samplers"); samplersVal && samplersVal->isArray()) {
            for (const GltfJsonValue& entry : samplersVal->asArray()) {
                GltfSampler sampler;
                if (const GltfJsonValue* v = entry.find("magFilter"); v && v->isNumber()) {
                    sampler.magFilter = static_cast<GltfFilterMode>(v->asInt(0));
                }
                if (const GltfJsonValue* v = entry.find("minFilter"); v && v->isNumber()) {
                    sampler.minFilter = static_cast<GltfFilterMode>(v->asInt(0));
                }
                if (const GltfJsonValue* v = entry.find("wrapS"); v && v->isNumber()) {
                    sampler.wrapS = static_cast<GltfWrapMode>(v->asInt(10497));
                }
                if (const GltfJsonValue* v = entry.find("wrapT"); v && v->isNumber()) {
                    sampler.wrapT = static_cast<GltfWrapMode>(v->asInt(10497));
                }
                model.samplers.push_back(sampler);
            }
        }

        // --- Textures ---
        if (const GltfJsonValue* texturesVal = root.find("textures"); texturesVal && texturesVal->isArray()) {
            for (const GltfJsonValue& entry : texturesVal->asArray()) {
                GltfTexture tex;
                if (const GltfJsonValue* v = entry.find("name")) tex.name = v->asString();
                if (const GltfJsonValue* v = entry.find("source")) tex.imageIndex = v->asInt(-1);
                if (const GltfJsonValue* v = entry.find("sampler")) tex.samplerIndex = v->asInt(-1);
                model.textures.push_back(std::move(tex));
            }
        }

        // --- Materials ---
        if (const GltfJsonValue* matsVal = root.find("materials"); matsVal && matsVal->isArray()) {
            for (const GltfJsonValue& entry : matsVal->asArray()) {
                GltfMaterial mat;
                if (const GltfJsonValue* v = entry.find("name")) mat.name = v->asString();
                if (const GltfJsonValue* pbr = entry.find("pbrMetallicRoughness"); pbr && pbr->isObject()) {
                    mat.baseColorFactor = readVec4(pbr->find("baseColorFactor"), mat.baseColorFactor);
                    mat.baseColorTexture = readTextureIndex(pbr->find("baseColorTexture"));
                    if (const GltfJsonValue* v = pbr->find("metallicFactor")) {
                        mat.metallicFactor = static_cast<float>(v->asNumber(1.0));
                    }
                    if (const GltfJsonValue* v = pbr->find("roughnessFactor")) {
                        mat.roughnessFactor = static_cast<float>(v->asNumber(1.0));
                    }
                    mat.metallicRoughnessTexture = readTextureIndex(pbr->find("metallicRoughnessTexture"));
                }
                if (const GltfJsonValue* normTex = entry.find("normalTexture");
                    normTex && normTex->isObject()) {
                    mat.normalTexture = readTextureIndex(normTex);
                    if (const GltfJsonValue* v = normTex->find("scale")) {
                        mat.normalScale = static_cast<float>(v->asNumber(1.0));
                    }
                }
                mat.emissiveFactor = readVec3(entry.find("emissiveFactor"), mat.emissiveFactor);
                mat.emissiveTexture = readTextureIndex(entry.find("emissiveTexture"));
                if (const GltfJsonValue* modeVal = entry.find("alphaMode"); modeVal && modeVal->isString()) {
                    const std::string& modeStr = modeVal->asString();
                    if (modeStr == "MASK") {
                        mat.alphaMode = GltfAlphaMode::Mask;
                    } else if (modeStr == "BLEND") {
                        mat.alphaMode = GltfAlphaMode::Blend;
                    } else {
                        mat.alphaMode = GltfAlphaMode::Opaque;
                    }
                }
                if (const GltfJsonValue* v = entry.find("alphaCutoff")) {
                    mat.alphaCutoff = static_cast<float>(v->asNumber(0.5));
                }
                if (const GltfJsonValue* v = entry.find("doubleSided")) {
                    mat.doubleSided = v->asBool(false);
                }
                model.materials.push_back(std::move(mat));
            }
        }

        // --- Meshes ---
        if (const GltfJsonValue* meshesVal = root.find("meshes"); meshesVal && meshesVal->isArray()) {
            for (const GltfJsonValue& meshEntry : meshesVal->asArray()) {
                GltfMesh mesh;
                if (const GltfJsonValue* v = meshEntry.find("name")) mesh.name = v->asString();
                const GltfJsonValue* primsVal = meshEntry.find("primitives");
                if (primsVal && primsVal->isArray()) {
                    for (const GltfJsonValue& primEntry : primsVal->asArray()) {
                        const int mode =
                            primEntry.find("mode") ? primEntry.find("mode")->asInt(kModeTriangles)
                                                   : kModeTriangles;
                        if (mode != kModeTriangles) {
                            MGE_LOG_WARN("Gltf", "Skipping non-triangle primitive (mode=", mode, ")");
                            continue;
                        }
                        const GltfJsonValue* attrs = primEntry.find("attributes");
                        if (!attrs || !attrs->isObject()) {
                            MGE_LOG_ERROR("Gltf", "Primitive missing attributes");
                            return std::nullopt;
                        }
                        const GltfJsonValue* posAccVal = attrs->find("POSITION");
                        if (!posAccVal || !posAccVal->isNumber()) {
                            MGE_LOG_ERROR("Gltf", "Primitive missing POSITION attribute");
                            return std::nullopt;
                        }
                        const int posAccIdx = posAccVal->asInt(-1);
                        if (posAccIdx < 0 || static_cast<std::size_t>(posAccIdx) >= accessors.size() ||
                            accessors[static_cast<std::size_t>(posAccIdx)].numComponents != 3) {
                            MGE_LOG_ERROR("Gltf", "Invalid POSITION accessor");
                            return std::nullopt;
                        }

                        std::vector<float> posFloats;
                        if (!readAccessorFloats(accessors[static_cast<std::size_t>(posAccIdx)], bufferViews,
                                                buffers, posFloats)) {
                            MGE_LOG_ERROR("Gltf", "Failed to read POSITION accessor");
                            return std::nullopt;
                        }
                        const std::size_t vertCount = accessors[static_cast<std::size_t>(posAccIdx)].count;

                        GltfPrimitive prim;
                        if (const GltfJsonValue* matIdx = primEntry.find("material")) {
                            prim.materialIndex = matIdx->asInt(-1);
                        }
                        prim.mesh.positions.resize(vertCount);
                        for (std::size_t i = 0; i < vertCount; i++) {
                            prim.mesh.positions[i] =
                                glm::vec3(posFloats[i * 3], posFloats[i * 3 + 1], posFloats[i * 3 + 2]);
                        }

                        // NORMAL (optional)
                        bool hasNormals = false;
                        if (const GltfJsonValue* normAccVal = attrs->find("NORMAL");
                            normAccVal && normAccVal->isNumber()) {
                            const int idx = normAccVal->asInt(-1);
                            if (idx >= 0 && static_cast<std::size_t>(idx) < accessors.size() &&
                                accessors[static_cast<std::size_t>(idx)].numComponents == 3 &&
                                accessors[static_cast<std::size_t>(idx)].count == vertCount) {
                                std::vector<float> normFloats;
                                if (readAccessorFloats(accessors[static_cast<std::size_t>(idx)], bufferViews,
                                                       buffers, normFloats)) {
                                    prim.mesh.normals.resize(vertCount);
                                    for (std::size_t i = 0; i < vertCount; i++) {
                                        prim.mesh.normals[i] = glm::vec3(
                                            normFloats[i * 3], normFloats[i * 3 + 1], normFloats[i * 3 + 2]);
                                    }
                                    hasNormals = true;
                                }
                            }
                        }
                        if (!hasNormals) {
                            prim.mesh.normals.assign(vertCount, glm::vec3(0.0f, 1.0f, 0.0f));
                        }

                        // TEXCOORD_0 (optional)
                        prim.mesh.uvs.assign(vertCount, glm::vec2(0.0f));
                        if (const GltfJsonValue* uvAccVal = attrs->find("TEXCOORD_0");
                            uvAccVal && uvAccVal->isNumber()) {
                            const int idx = uvAccVal->asInt(-1);
                            if (idx >= 0 && static_cast<std::size_t>(idx) < accessors.size() &&
                                accessors[static_cast<std::size_t>(idx)].numComponents == 2 &&
                                accessors[static_cast<std::size_t>(idx)].count == vertCount) {
                                std::vector<float> uvFloats;
                                if (readAccessorFloats(accessors[static_cast<std::size_t>(idx)], bufferViews,
                                                       buffers, uvFloats)) {
                                    for (std::size_t i = 0; i < vertCount; i++) {
                                        const float u = uvFloats[i * 2];
                                        const float v =
                                            options.flipUvY ? (1.0f - uvFloats[i * 2 + 1])
                                                            : uvFloats[i * 2 + 1];
                                        prim.mesh.uvs[i] = glm::vec2(u, v);
                                    }
                                }
                            }
                        }

                        // COLOR_0 (optional; VEC3 or VEC4)
                        prim.mesh.colors.assign(vertCount, glm::vec4(1.0f));
                        if (const GltfJsonValue* colAccVal = attrs->find("COLOR_0");
                            colAccVal && colAccVal->isNumber()) {
                            const int idx = colAccVal->asInt(-1);
                            if (idx >= 0 && static_cast<std::size_t>(idx) < accessors.size()) {
                                const AccessorInfo& colAcc = accessors[static_cast<std::size_t>(idx)];
                                if ((colAcc.numComponents == 3 || colAcc.numComponents == 4) &&
                                    colAcc.count == vertCount) {
                                    std::vector<float> colFloats;
                                    if (readAccessorFloats(colAcc, bufferViews, buffers, colFloats)) {
                                        const std::size_t stride = colAcc.numComponents;
                                        for (std::size_t i = 0; i < vertCount; i++) {
                                            const float a = stride == 4 ? colFloats[i * stride + 3] : 1.0f;
                                            prim.mesh.colors[i] =
                                                glm::vec4(colFloats[i * stride], colFloats[i * stride + 1],
                                                          colFloats[i * stride + 2], a);
                                        }
                                    }
                                }
                            }
                        }

                        // TANGENT (optional; VEC4)
                        if (const GltfJsonValue* tanAccVal = attrs->find("TANGENT");
                            tanAccVal && tanAccVal->isNumber()) {
                            const int idx = tanAccVal->asInt(-1);
                            if (idx >= 0 && static_cast<std::size_t>(idx) < accessors.size() &&
                                accessors[static_cast<std::size_t>(idx)].numComponents == 4 &&
                                accessors[static_cast<std::size_t>(idx)].count == vertCount) {
                                std::vector<float> tanFloats;
                                if (readAccessorFloats(accessors[static_cast<std::size_t>(idx)], bufferViews,
                                                       buffers, tanFloats)) {
                                    prim.tangents.resize(vertCount);
                                    for (std::size_t i = 0; i < vertCount; i++) {
                                        prim.tangents[i] =
                                            glm::vec4(tanFloats[i * 4], tanFloats[i * 4 + 1],
                                                      tanFloats[i * 4 + 2], tanFloats[i * 4 + 3]);
                                    }
                                }
                            }
                        }

                        // JOINTS_0 (optional; VEC4 u8/u16)
                        if (const GltfJsonValue* jointsAccVal = attrs->find("JOINTS_0");
                            jointsAccVal && jointsAccVal->isNumber()) {
                            const int idx = jointsAccVal->asInt(-1);
                            if (idx >= 0 && static_cast<std::size_t>(idx) < accessors.size() &&
                                accessors[static_cast<std::size_t>(idx)].count == vertCount) {
                                readAccessorJoints(accessors[static_cast<std::size_t>(idx)], bufferViews,
                                                   buffers, prim.joints);
                            }
                        }

                        // WEIGHTS_0 (optional; VEC4)
                        if (const GltfJsonValue* weightsAccVal = attrs->find("WEIGHTS_0");
                            weightsAccVal && weightsAccVal->isNumber()) {
                            const int idx = weightsAccVal->asInt(-1);
                            if (idx >= 0 && static_cast<std::size_t>(idx) < accessors.size() &&
                                accessors[static_cast<std::size_t>(idx)].numComponents == 4 &&
                                accessors[static_cast<std::size_t>(idx)].count == vertCount) {
                                std::vector<float> weightFloats;
                                if (readAccessorFloats(accessors[static_cast<std::size_t>(idx)], bufferViews,
                                                       buffers, weightFloats)) {
                                    prim.weights.resize(vertCount);
                                    for (std::size_t i = 0; i < vertCount; i++) {
                                        prim.weights[i] =
                                            glm::vec4(weightFloats[i * 4], weightFloats[i * 4 + 1],
                                                      weightFloats[i * 4 + 2], weightFloats[i * 4 + 3]);
                                    }
                                }
                            }
                        }

                        // Indices (optional; generate sequential if omitted)
                        if (const GltfJsonValue* idxAccVal = primEntry.find("indices");
                            idxAccVal && idxAccVal->isNumber()) {
                            const int idx = idxAccVal->asInt(-1);
                            if (idx < 0 || static_cast<std::size_t>(idx) >= accessors.size()) {
                                MGE_LOG_ERROR("Gltf", "Invalid indices accessor");
                                return std::nullopt;
                            }
                            if (!readAccessorIndices(accessors[static_cast<std::size_t>(idx)], bufferViews,
                                                     buffers, prim.mesh.indices)) {
                                MGE_LOG_ERROR("Gltf", "Failed to read indices accessor");
                                return std::nullopt;
                            }
                        } else {
                            const std::size_t usable = (vertCount / 3) * 3;
                            prim.mesh.indices.resize(usable);
                            for (std::size_t i = 0; i < usable; i++) {
                                prim.mesh.indices[i] = static_cast<std::uint32_t>(i);
                            }
                        }

                        const std::string validationError = prim.mesh.validate();
                        if (!validationError.empty()) {
                            MGE_LOG_ERROR("Gltf", "Invalid primitive mesh: ", validationError);
                            return std::nullopt;
                        }

                        if (!hasNormals && options.generateMissingNormals && !prim.mesh.indices.empty()) {
                            recomputeNormals(prim.mesh, NormalMode::Smooth);
                        }

                        mesh.primitives.push_back(std::move(prim));
                    }
                }
                model.meshes.push_back(std::move(mesh));
            }
        }

        // --- Nodes ---
        if (const GltfJsonValue* nodesVal = root.find("nodes"); nodesVal && nodesVal->isArray()) {
            for (const GltfJsonValue& entry : nodesVal->asArray()) {
                GltfNode node;
                if (const GltfJsonValue* v = entry.find("name")) node.name = v->asString();
                if (const GltfJsonValue* v = entry.find("mesh")) node.meshIndex = v->asInt(-1);
                if (const GltfJsonValue* v = entry.find("skin")) node.skinIndex = v->asInt(-1);
                if (const GltfJsonValue* matVal = entry.find("matrix");
                    matVal && matVal->isArray() && matVal->asArray().size() == 16) {
                    const GltfJsonArray& arr = matVal->asArray();
                    glm::mat4 mat(1.0f);
                    for (int col = 0; col < 4; col++) {
                        for (int row = 0; row < 4; row++) {
                            const std::size_t elemIdx = static_cast<std::size_t>(col * 4 + row);
                            mat[col][row] = static_cast<float>(arr[elemIdx].asNumber(0.0));
                        }
                    }
                    node.transform = decomposeMatrix(mat);
                } else {
                    node.transform.position = readVec3(entry.find("translation"), glm::vec3(0.0f));
                    const glm::vec4 rot =
                        readVec4(entry.find("rotation"), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                    // glTF stores quaternion as [x, y, z, w]; glm::quat constructor is (w, x, y, z)
                    node.transform.rotation = glm::normalize(glm::quat(rot.w, rot.x, rot.y, rot.z));
                    node.transform.scale = readVec3(entry.find("scale"), glm::vec3(1.0f));
                }
                if (const GltfJsonValue* childrenVal = entry.find("children");
                    childrenVal && childrenVal->isArray()) {
                    for (const GltfJsonValue& childVal : childrenVal->asArray()) {
                        if (childVal.isNumber()) node.children.push_back(childVal.asInt(-1));
                    }
                }
                model.nodes.push_back(std::move(node));
            }
        }

        // --- Scenes ---
        if (const GltfJsonValue* scenesVal = root.find("scenes"); scenesVal && scenesVal->isArray()) {
            for (const GltfJsonValue& entry : scenesVal->asArray()) {
                GltfScene scene;
                if (const GltfJsonValue* v = entry.find("name")) scene.name = v->asString();
                if (const GltfJsonValue* nodesVal = entry.find("nodes"); nodesVal && nodesVal->isArray()) {
                    for (const GltfJsonValue& n : nodesVal->asArray()) {
                        if (n.isNumber()) scene.rootNodes.push_back(n.asInt(-1));
                    }
                }
                model.scenes.push_back(std::move(scene));
            }
        }
        if (const GltfJsonValue* defaultSceneVal = root.find("scene");
            defaultSceneVal && defaultSceneVal->isNumber()) {
            model.defaultScene = defaultSceneVal->asInt(-1);
        }

        return model;
    }

    std::optional<GltfModel> loadGltfFile(const std::filesystem::path& path,
                                          const GltfLoadOptions& options) {
        const std::filesystem::path full = path.is_absolute() ? path : assetPath(path.generic_string());
        std::vector<std::uint8_t> bytes;
        if (!readFileBytes(full, bytes)) {
            MGE_LOG_ERROR("Gltf", "Failed to open ", full.string());
            return std::nullopt;
        }
        auto model = loadGltfFromMemory(bytes.data(), bytes.size(), full.parent_path(), options);
        if (model) {
            MGE_LOG_INFO("Gltf", "Loaded ", full.filename().string(), " (", model->meshes.size(),
                         " meshes, ", model->nodes.size(), " nodes, ", model->materials.size(),
                         " materials)");
        }
        return model;
    }
}

#include <doctest/doctest.h>
#include <mge/gltf/GltfLoader.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace mge;
namespace fs = std::filesystem;

namespace {
    void appendU32Le(std::vector<std::uint8_t>& out, std::uint32_t value) {
        out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
    }

    template <typename T> void appendBytes(std::vector<std::uint8_t>& out, const T& value) {
        const std::size_t oldSize = out.size();
        out.resize(oldSize + sizeof(T));
        std::memcpy(out.data() + oldSize, &value, sizeof(T));
    }

    std::vector<std::uint8_t> buildGlb(const std::string& json, const std::vector<std::uint8_t>& bin) {
        std::vector<std::uint8_t> jsonBytes(json.begin(), json.end());
        while (jsonBytes.size() % 4 != 0) jsonBytes.push_back(static_cast<std::uint8_t>(' '));

        std::vector<std::uint8_t> binBytes = bin;
        while (binBytes.size() % 4 != 0) binBytes.push_back(0u);

        std::uint32_t totalLength = 12u + 8u + static_cast<std::uint32_t>(jsonBytes.size());
        if (!binBytes.empty()) {
            totalLength += 8u + static_cast<std::uint32_t>(binBytes.size());
        }

        std::vector<std::uint8_t> glb;
        glb.reserve(totalLength);
        appendU32Le(glb, 0x46546C67u); // "glTF"
        appendU32Le(glb, 2u);          // version 2
        appendU32Le(glb, totalLength);

        appendU32Le(glb, static_cast<std::uint32_t>(jsonBytes.size()));
        appendU32Le(glb, 0x4E4F534Au); // "JSON"
        glb.insert(glb.end(), jsonBytes.begin(), jsonBytes.end());

        if (!binBytes.empty()) {
            appendU32Le(glb, static_cast<std::uint32_t>(binBytes.size()));
            appendU32Le(glb, 0x004E4942u); // "BIN\0"
            glb.insert(glb.end(), binBytes.begin(), binBytes.end());
        }
        return glb;
    }

    std::string encodeBase64(const std::vector<std::uint8_t>& data) {
        static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((data.size() + 2) / 3) * 4);
        for (std::size_t i = 0; i < data.size(); i += 3) {
            const std::uint32_t b0 = data[i];
            const std::uint32_t b1 = (i + 1 < data.size()) ? data[i + 1] : 0u;
            const std::uint32_t b2 = (i + 2 < data.size()) ? data[i + 2] : 0u;
            const std::uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
            out.push_back(kTable[(triple >> 18) & 0x3Fu]);
            out.push_back(kTable[(triple >> 12) & 0x3Fu]);
            out.push_back((i + 1 < data.size()) ? kTable[(triple >> 6) & 0x3Fu] : '=');
            out.push_back((i + 2 < data.size()) ? kTable[triple & 0x3Fu] : '=');
        }
        return out;
    }
}

TEST_CASE("loadGltfFromMemory rejects malformed JSON, truncated GLB prefixes, and adversarial buffers") {
    const std::string badJson1 = "{unquoted: 1}";
    CHECK_FALSE(loadGltfFromMemory(reinterpret_cast<const std::uint8_t*>(badJson1.data()), badJson1.size())
                    .has_value());
    const std::string badJson2 = "[1, 2,]";
    CHECK_FALSE(loadGltfFromMemory(reinterpret_cast<const std::uint8_t*>(badJson2.data()), badJson2.size())
                    .has_value());
    const std::string garbage = "\xFF\xFE\x00\x01garbage_bytes_not_gltf";
    CHECK_FALSE(loadGltfFromMemory(reinterpret_cast<const std::uint8_t*>(garbage.data()), garbage.size())
                    .has_value());

    // Valid GLB truncated at every possible prefix length must fail cleanly without UB/ASan errors
    std::vector<std::uint8_t> bin;
    const float verts[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    for (float v : verts) appendBytes(bin, v);
    const std::string validJson = R"({
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": 36}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}]
    })";
    const std::vector<std::uint8_t> validGlb = buildGlb(validJson, bin);
    REQUIRE(loadGltfFromMemory(validGlb.data(), validGlb.size()).has_value());

    for (std::size_t cut = 0; cut < validGlb.size(); ++cut) {
        CHECK_FALSE(loadGltfFromMemory(validGlb.data(), cut).has_value());
    }

    // Adversarial chunk length claiming 4 GiB inside a 24-byte buffer
    std::vector<std::uint8_t> hugeChunk;
    appendU32Le(hugeChunk, 0x46546C67u);
    appendU32Le(hugeChunk, 2u);
    appendU32Le(hugeChunk, 24u);
    appendU32Le(hugeChunk, 0xFFFFFFF0u);
    appendU32Le(hugeChunk, 0x4E4F534Au);
    appendU32Le(hugeChunk, 0x20202020u);
    CHECK_FALSE(loadGltfFromMemory(hugeChunk.data(), hugeChunk.size()).has_value());
}

TEST_CASE("loadGltfFromMemory loads a JSON glTF triangle with base64 buffer and UV V-flip") {
    std::vector<std::uint8_t> bin;
    // 3 positions (36 bytes)
    const float positions[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    for (float v : positions) appendBytes(bin, v);
    // 3 UVs (24 bytes)
    const float uvs[6] = {0.0f, 0.0f, 1.0f, 0.25f, 0.5f, 1.0f};
    for (float v : uvs) appendBytes(bin, v);
    // 3 uint16 indices (6 bytes)
    const std::uint16_t indices[3] = {0, 1, 2};
    for (std::uint16_t idx : indices) appendBytes(bin, idx);

    const std::string uri = "data:application/octet-stream;base64," + encodeBase64(bin);
    const std::string json = R"({
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": )" +
                             std::to_string(bin.size()) + R"(, "uri": ")" + uri + R"("}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 36},
            {"buffer": 0, "byteOffset": 36, "byteLength": 24},
            {"buffer": 0, "byteOffset": 60, "byteLength": 6}
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2"},
            {"bufferView": 2, "componentType": 5123, "count": 3, "type": "SCALAR"}
        ],
        "meshes": [{
            "name": "Triangle",
            "primitives": [{
                "attributes": {"POSITION": 0, "TEXCOORD_0": 1},
                "indices": 2
            }]
        }],
        "nodes": [{"name": "TriNode", "mesh": 0}],
        "scenes": [{"nodes": [0]}],
        "scene": 0
    })";

    const auto model = loadGltfFromMemory(reinterpret_cast<const std::uint8_t*>(json.data()), json.size());
    REQUIRE(model.has_value());
    CHECK(model->valid());
    REQUIRE(model->meshes.size() == 1);
    CHECK(model->meshes[0].name == "Triangle");
    REQUIRE(model->meshes[0].primitives.size() == 1);

    const MeshData& mesh = model->meshes[0].primitives[0].mesh;
    CHECK(mesh.validate().empty());
    CHECK(mesh.vertexCount() == 3);
    CHECK(mesh.triangleCount() == 1);
    CHECK(mesh.positions[1] == glm::vec3(1.0f, 0.0f, 0.0f));
    // Default flipUvY = true -> v becomes 1 - v
    CHECK(mesh.uvs[0].y == doctest::Approx(1.0f));
    CHECK(mesh.uvs[1].y == doctest::Approx(0.75f));
    CHECK(mesh.uvs[2].y == doctest::Approx(0.0f));
    // Missing normals were auto-recomputed to +Z for CCW XY triangle
    CHECK(mesh.normals[0].z == doctest::Approx(1.0f));

    // With flipUvY = false, UVs stay untouched
    GltfLoadOptions noFlip;
    noFlip.flipUvY = false;
    const auto rawModel =
        loadGltfFromMemory(reinterpret_cast<const std::uint8_t*>(json.data()), json.size(), {}, noFlip);
    REQUIRE(rawModel.has_value());
    CHECK(rawModel->meshes[0].primitives[0].mesh.uvs[1].y == doctest::Approx(0.25f));
}

TEST_CASE("loadGltfFromMemory loads a binary GLB with interleaved stride, normalized colors, and skinning") {
    // Interleaved vertex: vec3 pos (12) + u8x4 color (4) + u8x4 joints (4) + vec4 weights (16) = 36 bytes
    std::vector<std::uint8_t> bin;
    const glm::vec3 verts[3] = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}};
    for (int i = 0; i < 3; i++) {
        appendBytes(bin, verts[i].x);
        appendBytes(bin, verts[i].y);
        appendBytes(bin, verts[i].z);
        // Normalized RGBA color: (255, 0, 128, 255)
        bin.push_back(255u);
        bin.push_back(0u);
        bin.push_back(128u);
        bin.push_back(255u);
        // Joints: (1, 2, 0, 0)
        bin.push_back(1u);
        bin.push_back(2u);
        bin.push_back(0u);
        bin.push_back(0u);
        // Weights: (0.75, 0.25, 0.0, 0.0)
        const float w[4] = {0.75f, 0.25f, 0.0f, 0.0f};
        for (float f : w) appendBytes(bin, f);
    }

    const std::string json = R"({
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": 108}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 108, "byteStride": 36}],
        "accessors": [
            {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 0, "byteOffset": 12, "componentType": 5121, "normalized": true, "count": 3, "type": "VEC4"},
            {"bufferView": 0, "byteOffset": 16, "componentType": 5121, "count": 3, "type": "VEC4"},
            {"bufferView": 0, "byteOffset": 20, "componentType": 5126, "count": 3, "type": "VEC4"}
        ],
        "meshes": [{
            "primitives": [{
                "attributes": {
                    "POSITION": 0,
                    "COLOR_0": 1,
                    "JOINTS_0": 2,
                    "WEIGHTS_0": 3
                }
            }]
        }]
    })";

    const std::vector<std::uint8_t> glb = buildGlb(json, bin);
    const auto model = loadGltfFromMemory(glb.data(), glb.size());
    REQUIRE(model.has_value());
    REQUIRE(model->meshes.size() == 1);
    REQUIRE(model->meshes[0].primitives.size() == 1);

    const GltfPrimitive& prim = model->meshes[0].primitives[0];
    CHECK(prim.mesh.validate().empty());
    CHECK(prim.mesh.vertexCount() == 3);
    CHECK(prim.mesh.triangleCount() == 1); // Non-indexed generated 0, 1, 2
    CHECK(prim.mesh.positions[1] == glm::vec3(2.0f, 0.0f, 0.0f));
    CHECK(prim.mesh.colors[0].r == doctest::Approx(1.0f));
    CHECK(prim.mesh.colors[0].g == doctest::Approx(0.0f));
    CHECK(prim.mesh.colors[0].b == doctest::Approx(128.0f / 255.0f));
    CHECK(prim.mesh.colors[0].a == doctest::Approx(1.0f));
    REQUIRE(prim.joints.size() == 3);
    CHECK(prim.joints[0] == glm::u16vec4(1, 2, 0, 0));
    REQUIRE(prim.weights.size() == 3);
    CHECK(prim.weights[0].x == doctest::Approx(0.75f));
    CHECK(prim.weights[0].y == doctest::Approx(0.25f));
}

TEST_CASE("loadGltfFromMemory parses materials, textures, samplers, and embedded images") {
    const std::vector<std::uint8_t> fakePng = {0x89u, 0x50u, 0x4Eu, 0x47u};
    const std::string json = R"({
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": 4}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 4}],
        "images": [{"name": "Albedo", "mimeType": "image/png", "bufferView": 0}],
        "samplers": [{"magFilter": 9728, "minFilter": 9729, "wrapS": 33071, "wrapT": 33648}],
        "textures": [{"name": "Tex0", "source": 0, "sampler": 0}],
        "materials": [{
            "name": "Leaf",
            "pbrMetallicRoughness": {
                "baseColorFactor": [0.2, 0.8, 0.3, 1.0],
                "baseColorTexture": {"index": 0},
                "metallicFactor": 0.1,
                "roughnessFactor": 0.9
            },
            "alphaMode": "MASK",
            "alphaCutoff": 0.4,
            "doubleSided": true
        }]
    })";

    const std::vector<std::uint8_t> glb = buildGlb(json, fakePng);
    const auto model = loadGltfFromMemory(glb.data(), glb.size());
    REQUIRE(model.has_value());
    REQUIRE(model->images.size() == 1);
    CHECK(model->images[0].mimeType == "image/png");
    CHECK(model->images[0].encodedBytes == fakePng);
    REQUIRE(model->samplers.size() == 1);
    CHECK(model->samplers[0].magFilter == GltfFilterMode::Nearest);
    CHECK(model->samplers[0].wrapS == GltfWrapMode::ClampToEdge);
    CHECK(model->samplers[0].wrapT == GltfWrapMode::MirroredRepeat);
    REQUIRE(model->textures.size() == 1);
    CHECK(model->textures[0].imageIndex == 0);
    CHECK(model->textures[0].samplerIndex == 0);
    REQUIRE(model->materials.size() == 1);
    const GltfMaterial& mat = model->materials[0];
    CHECK(mat.name == "Leaf");
    CHECK(mat.baseColorFactor.g == doctest::Approx(0.8f));
    CHECK(mat.baseColorTexture == 0);
    CHECK(mat.metallicFactor == doctest::Approx(0.1f));
    CHECK(mat.roughnessFactor == doctest::Approx(0.9f));
    CHECK(mat.alphaMode == GltfAlphaMode::Mask);
    CHECK(mat.alphaCutoff == doctest::Approx(0.4f));
    CHECK(mat.doubleSided);
}

TEST_CASE("loadGltfFromMemory parses node hierarchy, TRS and matrix transforms, bounds, and cycles") {
    std::vector<std::uint8_t> bin;
    const float verts[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    for (float v : verts) appendBytes(bin, v);

    // Node 0: translation (10, 0, 0), child Node 1
    // Node 1: matrix = translate(0, 5, 0) * scale(2, 2, 2), mesh 0
    const std::string json = R"({
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": 36}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
        "nodes": [
            {"name": "Parent", "translation": [10, 0, 0], "children": [1]},
            {"name": "Child", "matrix": [2,0,0,0, 0,2,0,0, 0,0,2,0, 0,5,0,1], "mesh": 0}
        ],
        "scenes": [{"name": "Main", "nodes": [0]}],
        "scene": 0
    })";

    const std::vector<std::uint8_t> glb = buildGlb(json, bin);
    const auto model = loadGltfFromMemory(glb.data(), glb.size());
    REQUIRE(model.has_value());
    REQUIRE(model->nodes.size() == 2);

    const std::vector<glm::mat4> world = model->computeWorldTransforms();
    REQUIRE(world.size() == 2);
    const glm::vec3 childOrigin = glm::vec3(world[1] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(childOrigin.x == doctest::Approx(10.0f));
    CHECK(childOrigin.y == doctest::Approx(5.0f));
    CHECK(childOrigin.z == doctest::Approx(0.0f));

    const Aabb box = model->bounds();
    CHECK(box.min.x == doctest::Approx(10.0f));
    CHECK(box.min.y == doctest::Approx(5.0f));
    CHECK(box.max.x == doctest::Approx(12.0f));
    CHECK(box.max.y == doctest::Approx(7.0f));

    // Cycle safety: self-referencing / mutually-referencing nodes must terminate cleanly
    GltfModel cyclic;
    cyclic.nodes.resize(2);
    cyclic.nodes[0].children = {1};
    cyclic.nodes[1].children = {0};
    const std::vector<glm::mat4> cyclicWorld = cyclic.computeWorldTransforms();
    CHECK(cyclicWorld.size() == 2);
}

TEST_CASE("loadGltfFromMemory rejects invalid GLB headers, truncated chunks, and bad JSON") {
    CHECK_FALSE(loadGltfFromMemory(nullptr, 0).has_value());

    // Bad GLB version
    std::vector<std::uint8_t> badVersion;
    appendU32Le(badVersion, 0x46546C67u);
    appendU32Le(badVersion, 1u);
    appendU32Le(badVersion, 12u);
    CHECK_FALSE(loadGltfFromMemory(badVersion.data(), badVersion.size()).has_value());

    // Truncated GLB length > buffer size
    std::vector<std::uint8_t> truncated;
    appendU32Le(truncated, 0x46546C67u);
    appendU32Le(truncated, 2u);
    appendU32Le(truncated, 999u);
    CHECK_FALSE(loadGltfFromMemory(truncated.data(), truncated.size()).has_value());

    // Unsupported glTF asset version
    const std::string v1Json = R"({"asset": {"version": "1.0"}})";
    CHECK_FALSE(
        loadGltfFromMemory(reinterpret_cast<const std::uint8_t*>(v1Json.data()), v1Json.size()).has_value());

    // Out-of-bounds bufferView
    const std::string oobJson = R"({
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": 4, "uri": "data:application/octet-stream;base64,AAAAAA=="}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 64}]
    })";
    CHECK_FALSE(loadGltfFromMemory(reinterpret_cast<const std::uint8_t*>(oobJson.data()), oobJson.size())
                    .has_value());
}

TEST_CASE("loadGltfFile loads a GLB from disk and fails gracefully on missing files") {
    CHECK_FALSE(loadGltfFile("/nonexistent/path/model.glb").has_value());

    std::vector<std::uint8_t> bin;
    const float verts[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    for (float v : verts) appendBytes(bin, v);
    const std::string json = R"({
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": 36}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
        "meshes": [{"name": "DiskTri", "primitives": [{"attributes": {"POSITION": 0}}]}]
    })";

    const fs::path tempPath = fs::temp_directory_path() / "mge_test_model.glb";
    const std::vector<std::uint8_t> glb = buildGlb(json, bin);
    {
        std::ofstream out(tempPath, std::ios::binary);
        REQUIRE(out.good());
        out.write(reinterpret_cast<const char*>(glb.data()), static_cast<std::streamsize>(glb.size()));
    }

    const auto loaded = loadGltfFile(tempPath);
    fs::remove(tempPath);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->meshes.size() == 1);
    CHECK(loaded->meshes[0].name == "DiskTri");
    CHECK(loaded->meshes[0].primitives[0].mesh.triangleCount() == 1);
}

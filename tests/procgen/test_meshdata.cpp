#include <doctest/doctest.h>
#include <mge/procgen/MeshData.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace mge;
namespace fs = std::filesystem;

namespace {
    MeshData unitQuad() {
        MeshData mesh;
        auto a = mesh.addVertex({0, 0, 0}, {0, 0, 1}, {1, 0, 0, 1}, {0, 0});
        auto b = mesh.addVertex({1, 0, 0}, {0, 0, 1}, {0, 1, 0, 1}, {1, 0});
        auto c = mesh.addVertex({1, 1, 0}, {0, 0, 1}, {0, 0, 1, 1}, {1, 1});
        auto d = mesh.addVertex({0, 1, 0}, {0, 0, 1}, {1, 1, 1, 1}, {0, 1});
        mesh.addQuad(a, b, c, d);
        return mesh;
    }
}

TEST_CASE("addVertex/addQuad: conteos, índices y atributos alineados") {
    MeshData mesh = unitQuad();
    CHECK(mesh.vertexCount() == 4);
    CHECK(mesh.triangleCount() == 2);
    CHECK(mesh.indices == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3});
    CHECK(mesh.normals.size() == 4);
    CHECK(mesh.colors.size() == 4);
    CHECK(mesh.uvs.size() == 4);
    CHECK(mesh.validate().empty());
}

TEST_CASE("addVertex con valores por defecto") {
    MeshData mesh;
    mesh.addVertex({1, 2, 3});
    CHECK(mesh.colors[0] == glm::vec4(1.0f));
    CHECK(mesh.normals[0] == glm::vec3(0.0f));
}

TEST_CASE("validate detecta índices fuera de rango, largos distintos y triángulos incompletos") {
    MeshData bad = unitQuad();
    bad.indices.push_back(7);
    bad.indices.push_back(0);
    bad.indices.push_back(1);
    CHECK(bad.validate().find("fuera de rango") != std::string::npos);

    MeshData mismatched = unitQuad();
    mismatched.uvs.pop_back();
    CHECK(mismatched.validate().find("distinto largo") != std::string::npos);

    MeshData partial = unitQuad();
    partial.indices.pop_back();
    CHECK(partial.validate().find("múltiplo de 3") != std::string::npos);

    CHECK(MeshData{}.validate().empty()); // Una malla vacía es válida
}

TEST_CASE("bounds, clear y reserve") {
    MeshData mesh = unitQuad();
    Aabb box = mesh.bounds();
    CHECK(box.min == glm::vec3(0, 0, 0));
    CHECK(box.max == glm::vec3(1, 1, 0));
    mesh.clear();
    CHECK(mesh.empty());
    CHECK(mesh.bounds().isEmpty());
    mesh.reserve(100, 50);
    CHECK(mesh.positions.capacity() >= 100);
}

TEST_CASE("writeObj escribe v/vt/vn/f con color y caras que empiezan en 1") {
    fs::path path = fs::temp_directory_path() / "mge_test_quad.obj";
    REQUIRE(writeObj(unitQuad(), path));

    std::ifstream file(path);
    int v = 0, vt = 0, vn = 0, f = 0;
    std::string line, firstVertex, firstFace;
    while (std::getline(file, line)) {
        if (line.rfind("v ", 0) == 0) {
            if (v++ == 0) firstVertex = line;
        } else if (line.rfind("vt ", 0) == 0) {
            vt++;
        } else if (line.rfind("vn ", 0) == 0) {
            vn++;
        } else if (line.rfind("f ", 0) == 0) {
            if (f++ == 0) firstFace = line;
        }
    }
    CHECK(v == 4);
    CHECK(vt == 4);
    CHECK(vn == 4);
    CHECK(f == 2);
    CHECK(firstVertex == "v 0 0 0 1 0 0");     // posición + color rojo
    CHECK(firstFace == "f 1/1/1 2/2/2 3/3/3"); // índices desde 1
    fs::remove(path);

    CHECK_FALSE(writeObj(unitQuad(), "/no/existe/dir/x.obj"));
}

#pragma once

#include <mge/gpu/GpuMesh.h>

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

namespace mge {

    class Texture2D;

    // Cómo se dibuja una malla (el orden del enum es el orden de dibujo)
    enum class MeshPass {
        Opaque,      // Prueba y escritura de profundidad, sin mezcla
        Cutout,      // Como Opaque, pero descarta los píxeles con alfa < 0.5 (hojas, plantas)
        Transparent, // Mezcla alfa, prueba de profundidad SIN escritura (después de lo opaco)
    };
    inline constexpr std::size_t kMeshPassCount = 3;

    // Material procedural por malla (#34): ruido de brillo en el espacio del mundo, para mallas de solo color
    // (bloques sin textura) que con greedy son caras grandes y planas
    struct MeshMaterial {
        float detailScale = 4.0f;    // Celdas del ruido por unidad del mundo
        float detailStrength = 0.0f; // 0 = sin detalle; 0.1–0.3 = sutil
    };

    // Una malla a dibujar en el frame
    struct DrawItem {
        const GpuMesh* mesh = nullptr;
        glm::mat4 model{1.0f};
        const Texture2D* texture = nullptr; // nullptr = blanca
        MeshPass pass = MeshPass::Opaque;
        glm::vec3 center{0.0f}; // Centro en el mundo (para ordenar)
        MeshMaterial material;
    };

    // Cola de dibujo del frame. sort() la ordena para dibujar bien y rápido:
    //   1. por pasada (opaco, cutout, transparente);
    //   2. opaco y cutout de adelante hacia atrás (la profundidad descarta lo tapado antes de sombrear);
    //   3. transparente de atrás hacia adelante (la mezcla alfa necesita ese orden).
    // El orden es por centro de cada item (ej: un chunk); dentro de una malla no se ordena.
    class RenderQueue {
    public:
        void add(const DrawItem& item);
        void clear() { m_items.clear(); }
        // depth = distancia a lo largo de la vista: dot(center - cameraPosition, cameraForward)
        void sort(const glm::vec3& cameraPosition, const glm::vec3& cameraForward);

        const std::vector<DrawItem>& items() const { return m_items; }
        std::size_t size() const { return m_items.size(); }

    private:
        std::vector<DrawItem> m_items;
    };
}

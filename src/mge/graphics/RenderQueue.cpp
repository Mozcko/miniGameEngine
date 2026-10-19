#include <mge/graphics/RenderQueue.h>

#include <algorithm>

namespace mge {

    void RenderQueue::add(const DrawItem& item) {
        if (item.mesh && !item.mesh->empty()) m_items.push_back(item);
    }

    void RenderQueue::sort(const glm::vec3& cameraPosition, const glm::vec3& cameraForward) {
        auto depth = [&](const DrawItem& item) {
            return glm::dot(item.center - cameraPosition, cameraForward);
        };
        std::stable_sort(m_items.begin(), m_items.end(), [&](const DrawItem& a, const DrawItem& b) {
            if (a.pass != b.pass) return a.pass < b.pass;
            if (a.pass == MeshPass::Transparent) return depth(a) > depth(b); // Atrás → adelante
            return depth(a) < depth(b);                                      // Adelante → atrás
        });
    }
}

#pragma once

#include <mge/core/Log.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace mge {

    using GpuResourceId = std::uint64_t;

    // Registra recursos de GPU creados a través de bindings para asegurar que se liberen
    // antes de destruir el SDL_GPUDevice, evitando cuelgues en el shutdown.
    class GpuResourceTracker {
    public:
        struct Entry {
            GpuResourceId id = 0;
            std::string kind;
            std::function<void()> releaseFn;
        };

        GpuResourceId registerResource(std::string kind, std::function<void()> releaseFn) {
            const GpuResourceId id = ++m_nextId;
            m_entries.push_back(Entry{id, std::move(kind), std::move(releaseFn)});
            return id;
        }

        void unregisterResource(GpuResourceId id) {
            auto it =
                std::find_if(m_entries.begin(), m_entries.end(), [id](const Entry& e) { return e.id == id; });
            if (it != m_entries.end()) {
                m_entries.erase(it);
            }
        }

        void releaseAll() {
            if (m_entries.empty()) return;

            // Contar tipos para el warning
            std::map<std::string, std::size_t> counts;
            for (const auto& entry : m_entries) {
                counts[entry.kind]++;
            }
            for (const auto& pair : counts) {
                MGE_LOG_WARN("Gpu", pair.second, " ", pair.first,
                             " released at shutdown – release them in onShutdown");
            }

            auto entries = std::move(m_entries);
            m_entries.clear();
            for (auto& entry : entries) {
                if (entry.releaseFn) {
                    entry.releaseFn();
                }
            }
        }

        std::size_t trackedCount() const { return m_entries.size(); }

    private:
        std::uint64_t m_nextId = 0;
        std::vector<Entry> m_entries;
    };

} // namespace mge

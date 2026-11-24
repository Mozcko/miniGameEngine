#pragma once

#include <cstdint>

namespace mge {

    // Entity identifier: index (reusable slot) + generation (incremented whenever the
    // index is recycled). A stale handle to a destroyed entity is no longer valid.
    struct Entity {
        std::uint32_t index = kInvalidIndex;
        std::uint32_t generation = 0;

        static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFu;

        bool isNull() const { return index == kInvalidIndex; }
        friend bool operator==(Entity a, Entity b) {
            return a.index == b.index && a.generation == b.generation;
        }
        friend bool operator!=(Entity a, Entity b) { return !(a == b); }
    };

    // Null entity (default value)
    inline constexpr Entity kNullEntity{};
}

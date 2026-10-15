#pragma once

#include <cstdint>

namespace mge {

    // Identificador de entidad: índice (posición reutilizable) + generación (cambia cada vez que el
    // índice se recicla). Un handle viejo de una entidad destruida deja de ser válido.
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

    // Entidad nula (valor por defecto)
    inline constexpr Entity kNullEntity{};
}

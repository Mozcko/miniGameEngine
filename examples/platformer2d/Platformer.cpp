#include <examples/platformer2d/Platformer.h>

#include <algorithm>
#include <cmath>

namespace platformer {

    namespace {
        constexpr float kEpsilon = 1e-4f;

        bool overlaps(const glm::vec2& aPos, const glm::vec2& aSize, const glm::vec2& bPos,
                      const glm::vec2& bSize) {
            return aPos.x < bPos.x + bSize.x && bPos.x < aPos.x + aSize.x && aPos.y < bPos.y + bSize.y &&
                   bPos.y < aPos.y + aSize.y;
        }
    }

    Platformer::Platformer(Level level)
        : m_level(std::move(level)), m_collected(m_level.items.size(), false) {
        // Los enemigos caen hasta el piso o el agua (en el archivo están en la fila de los objetos)
        for (glm::ivec2& enemy : m_level.enemies)
            while (enemy.y + 1 < m_level.height && m_level.at(enemy.x, enemy.y + 1) == Tile::Empty) enemy.y++;
        respawn();
    }

    void Platformer::respawn() {
        // Parado en el fondo de la casilla de inicio, centrado
        const glm::vec2 cell(m_level.spawn);
        m_position = {cell.x + (1.0f - kPlayerSize.x) * 0.5f, cell.y + 1.0f - kPlayerSize.y};
        m_velocity = glm::vec2(0.0f);
        m_grounded = false;
    }

    bool Platformer::overlapsSolid(const glm::vec2& pos) const {
        const glm::ivec2 lo(glm::floor(pos));
        const glm::ivec2 hi(glm::floor(pos + kPlayerSize - kEpsilon));
        for (int y = lo.y; y <= hi.y; y++)
            for (int x = lo.x; x <= hi.x; x++)
                if (m_level.solid(x, y)) return true;
        return false;
    }

    bool Platformer::moveAxis(int axis, float delta) {
        if (delta == 0.0f) return false;
        glm::vec2 next = m_position;
        next[axis] += delta;
        if (!overlapsSolid(next)) {
            m_position = next;
            return false;
        }
        // Pegarse al borde de la casilla con la que choca
        if (delta > 0.0f)
            next[axis] = std::floor(next[axis] + kPlayerSize[axis] - kEpsilon) - kPlayerSize[axis];
        else
            next[axis] = std::floor(next[axis]) + 1.0f;
        if (!overlapsSolid(next)) m_position = next;
        return true;
    }

    StepEvents Platformer::step(const PlayerInput& input, float dt) {
        StepEvents events;
        m_velocity.x = std::clamp(input.move, -1.0f, 1.0f) * kRunSpeed;
        if (input.jump && m_grounded) {
            m_velocity.y = -kJumpSpeed;
            events.jumped = true;
        }
        m_velocity.y = std::min(m_velocity.y + kGravity * dt, kMaxFallSpeed);

        moveAxis(0, m_velocity.x * dt);
        m_position.x = std::max(m_position.x, 0.0f); // Pared invisible a la izquierda
        const bool hitY = moveAxis(1, m_velocity.y * dt);
        m_grounded = hitY && m_velocity.y > 0.0f;
        if (hitY) m_velocity.y = 0.0f;

        for (std::size_t i = 0; i < m_level.items.size(); i++) {
            if (m_collected[i]) continue;
            if (overlaps(m_position, kPlayerSize, glm::vec2(m_level.items[i].cell), glm::vec2(1.0f))) {
                m_collected[i] = true;
                m_coins++;
                events.coins++;
            }
        }
        // ¿Los pies en el agua?
        const glm::vec2 feet = m_position + glm::vec2(kPlayerSize.x * 0.5f, kPlayerSize.y - 0.1f);
        const bool inWater = m_level.at(int(std::floor(feet.x)), int(std::floor(feet.y))) == Tile::Water;
        bool hitEnemy = false;
        for (const glm::ivec2& enemy : m_level.enemies)
            hitEnemy = hitEnemy || overlaps(m_position, kPlayerSize,
                                            glm::vec2(enemy) + glm::vec2(0.15f, 0.3f), glm::vec2(0.7f, 0.7f));
        if (hitEnemy || inWater || m_position.y > float(m_level.height) + 2.0f) {
            events.died = true;
            m_deaths++;
            respawn();
        }
        if (m_position.x + kPlayerSize.x >= float(m_level.width)) events.finished = true;
        return events;
    }
}

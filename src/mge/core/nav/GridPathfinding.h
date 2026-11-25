#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <functional>
#include <vector>

namespace mge {

    // Callback returning the traversal cost of entering a grid cell.
    // Values <= 0.0f (or non-finite) indicate an impassable / blocked cell.
    using GridCostQuery = std::function<float(const glm::ivec2&)>;

    // Dense 2D navigation grid storing per-cell traversal costs.
    // Cost > 0.0f means walkable (default 1.0f); cost <= 0.0f means blocked.
    class NavGrid {
    public:
        NavGrid() = default;
        NavGrid(int width, int height, float defaultCost = 1.0f);

        int width() const { return m_width; }
        int height() const { return m_height; }

        bool inBounds(int x, int y) const;
        bool inBounds(const glm::ivec2& cell) const { return inBounds(cell.x, cell.y); }

        bool isWalkable(int x, int y) const;
        bool isWalkable(const glm::ivec2& cell) const { return isWalkable(cell.x, cell.y); }

        // Returns 0.0f for blocked or out-of-bounds cells.
        float cost(int x, int y) const;
        float cost(const glm::ivec2& cell) const { return cost(cell.x, cell.y); }

        // Sets a cell to walkable (cost 1.0f) or blocked (cost 0.0f). Out-of-bounds calls are ignored.
        void setWalkable(int x, int y, bool walkable);
        void setWalkable(const glm::ivec2& cell, bool walkable) { setWalkable(cell.x, cell.y, walkable); }

        // Sets a cell's traversal cost (<= 0.0f or non-finite marks it blocked). Out-of-bounds calls are
        // ignored.
        void setCost(int x, int y, float cellCost);
        void setCost(const glm::ivec2& cell, float cellCost) { setCost(cell.x, cell.y, cellCost); }

        void fill(float cellCost);
        void clearObstacles();

    private:
        std::size_t index(int x, int y) const;

        int m_width = 0;
        int m_height = 0;
        std::vector<float> m_costs;
    };

    struct GridPathOptions {
        // False = 4-way cardinal movement; true = 8-way (cardinal + diagonal, diagonal step * sqrt(2)).
        bool allowDiagonal = false;
        // When allowDiagonal is true, require both adjacent cardinal cells to be walkable before moving
        // diagonally between them.
        bool preventCornerCutting = true;
        // Maximum number of nodes expanded before aborting (0 = unlimited).
        std::size_t maxExpandedNodes = 0;
        // Multiplier applied to the heuristic (1.0f = optimal A* when all walkable costs >= 1.0f;
        // 0.0f = Dijkstra; > 1.0f = faster weighted A*). Negative values are clamped to 0.0f.
        float heuristicWeight = 1.0f;
    };

    struct GridPathResult {
        bool found = false;
        // Sequence of cells from start to goal (inclusive). Empty when found == false.
        std::vector<glm::ivec2> path;
        // Total traversal cost along the path (0.0f when not found or start == goal).
        float totalCost = 0.0f;
        // Number of nodes expanded from the open set during the search.
        std::size_t nodesExpanded = 0;
    };

    // Runs A* pathfinding on a NavGrid from start to goal.
    GridPathResult findGridPath(const NavGrid& grid, const glm::ivec2& start, const glm::ivec2& goal,
                                const GridPathOptions& options = {});

    // Runs A* pathfinding over a bounded [0, width) x [0, height) grid using a custom cost callback.
    GridPathResult findGridPath(int width, int height, const glm::ivec2& start, const glm::ivec2& goal,
                                const GridCostQuery& costQuery, const GridPathOptions& options = {});

    // Removes redundant collinear intermediate cells from a grid path while preserving start and goal.
    std::vector<glm::ivec2> simplifyGridPath(const std::vector<glm::ivec2>& path);

} // namespace mge

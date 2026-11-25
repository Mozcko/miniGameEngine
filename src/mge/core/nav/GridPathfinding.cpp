#include <mge/core/nav/GridPathfinding.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>

namespace mge {

    namespace {

        constexpr float kDiagonalCost = 1.4142135623730950488f;
        constexpr float kOctileDiagDelta = kDiagonalCost - 1.0f;
        constexpr std::size_t kNoParent = std::numeric_limits<std::size_t>::max();

        struct Step {
            int dx = 0;
            int dy = 0;
            float distance = 1.0f;
        };

        constexpr std::array<Step, 4> kCardinalSteps = {{
            {1, 0, 1.0f},
            {-1, 0, 1.0f},
            {0, 1, 1.0f},
            {0, -1, 1.0f},
        }};

        constexpr std::array<Step, 8> kAllSteps = {{
            {1, 0, 1.0f},
            {-1, 0, 1.0f},
            {0, 1, 1.0f},
            {0, -1, 1.0f},
            {1, 1, kDiagonalCost},
            {1, -1, kDiagonalCost},
            {-1, 1, kDiagonalCost},
            {-1, -1, kDiagonalCost},
        }};

        struct OpenNode {
            float fScore = 0.0f;
            float hScore = 0.0f;
            std::size_t index = 0;
        };

        // Min-heap comparator: smallest fScore, then smallest hScore, then smallest cell index for
        // deterministic tie-breaking.
        struct OpenNodeGreater {
            bool operator()(const OpenNode& a, const OpenNode& b) const {
                if (a.fScore != b.fScore) {
                    return a.fScore > b.fScore;
                }
                if (a.hScore != b.hScore) {
                    return a.hScore > b.hScore;
                }
                return a.index > b.index;
            }
        };

        float sanitizeCost(float cellCost) {
            if (!std::isfinite(cellCost) || cellCost <= 0.0f) {
                return 0.0f;
            }
            return cellCost;
        }

        std::size_t toIndex(int x, int y, int width) {
            return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                   static_cast<std::size_t>(x);
        }

        glm::ivec2 fromIndex(std::size_t idx, int width) {
            const std::size_t w = static_cast<std::size_t>(width);
            return glm::ivec2(static_cast<int>(idx % w), static_cast<int>(idx / w));
        }

        bool isInBounds(const glm::ivec2& cell, int width, int height) {
            return cell.x >= 0 && cell.x < width && cell.y >= 0 && cell.y < height;
        }

        float computeHeuristic(const glm::ivec2& from, const glm::ivec2& to, bool allowDiagonal) {
            const float dx = static_cast<float>(std::abs(to.x - from.x));
            const float dy = static_cast<float>(std::abs(to.y - from.y));
            if (allowDiagonal) {
                const float minDelta = std::min(dx, dy);
                const float maxDelta = std::max(dx, dy);
                return maxDelta + kOctileDiagDelta * minDelta;
            }
            return dx + dy;
        }

        template <typename CostFn>
        GridPathResult runAStar(int width, int height, const glm::ivec2& start, const glm::ivec2& goal,
                                const CostFn& getCost, const GridPathOptions& options) {
            GridPathResult result;
            if (width <= 0 || height <= 0) {
                return result;
            }
            if (!isInBounds(start, width, height) || !isInBounds(goal, width, height)) {
                return result;
            }

            if (sanitizeCost(getCost(start)) <= 0.0f || sanitizeCost(getCost(goal)) <= 0.0f) {
                return result;
            }

            if (start == goal) {
                result.found = true;
                result.path.push_back(start);
                result.totalCost = 0.0f;
                result.nodesExpanded = 0;
                return result;
            }

            const std::size_t cellCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            const std::size_t startIdx = toIndex(start.x, start.y, width);
            const std::size_t goalIdx = toIndex(goal.x, goal.y, width);

            const float weight = (std::isfinite(options.heuristicWeight) && options.heuristicWeight > 0.0f)
                                     ? options.heuristicWeight
                                     : 0.0f;

            constexpr float kInfinity = std::numeric_limits<float>::infinity();
            std::vector<float> gScore(cellCount, kInfinity);
            std::vector<std::size_t> cameFrom(cellCount, kNoParent);
            std::vector<std::uint8_t> closed(cellCount, 0);

            std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeGreater> openSet;

            const float startH = computeHeuristic(start, goal, options.allowDiagonal) * weight;
            gScore[startIdx] = 0.0f;
            openSet.push(OpenNode{startH, startH, startIdx});

            const Step* steps = options.allowDiagonal ? kAllSteps.data() : kCardinalSteps.data();
            const std::size_t stepCount = options.allowDiagonal ? kAllSteps.size() : kCardinalSteps.size();

            while (!openSet.empty()) {
                const OpenNode current = openSet.top();
                openSet.pop();

                if (closed[current.index] != 0) {
                    continue;
                }
                closed[current.index] = 1;

                if (current.index == goalIdx) {
                    result.found = true;
                    result.totalCost = gScore[goalIdx];

                    std::size_t trace = goalIdx;
                    while (trace != kNoParent) {
                        result.path.push_back(fromIndex(trace, width));
                        trace = cameFrom[trace];
                    }
                    std::reverse(result.path.begin(), result.path.end());
                    return result;
                }

                if (options.maxExpandedNodes > 0 && result.nodesExpanded >= options.maxExpandedNodes) {
                    return result;
                }
                ++result.nodesExpanded;

                const glm::ivec2 currentCell = fromIndex(current.index, width);
                const float currentG = gScore[current.index];

                for (std::size_t i = 0; i < stepCount; ++i) {
                    const Step& step = steps[i];
                    const glm::ivec2 neighbor(currentCell.x + step.dx, currentCell.y + step.dy);
                    if (!isInBounds(neighbor, width, height)) {
                        continue;
                    }

                    const std::size_t neighborIdx = toIndex(neighbor.x, neighbor.y, width);
                    if (closed[neighborIdx] != 0) {
                        continue;
                    }

                    const float neighborCost = sanitizeCost(getCost(neighbor));
                    if (neighborCost <= 0.0f) {
                        continue;
                    }

                    // Prevent cutting across blocked corners on diagonal moves
                    if (step.dx != 0 && step.dy != 0 && options.preventCornerCutting) {
                        const glm::ivec2 adjX(currentCell.x + step.dx, currentCell.y);
                        const glm::ivec2 adjY(currentCell.x, currentCell.y + step.dy);
                        if (sanitizeCost(getCost(adjX)) <= 0.0f || sanitizeCost(getCost(adjY)) <= 0.0f) {
                            continue;
                        }
                    }

                    const float tentativeG = currentG + step.distance * neighborCost;
                    if (tentativeG < gScore[neighborIdx]) {
                        cameFrom[neighborIdx] = current.index;
                        gScore[neighborIdx] = tentativeG;
                        const float h = computeHeuristic(neighbor, goal, options.allowDiagonal) * weight;
                        openSet.push(OpenNode{tentativeG + h, h, neighborIdx});
                    }
                }
            }

            return result;
        }

    } // namespace

    // --- NavGrid ---

    NavGrid::NavGrid(int width, int height, float defaultCost)
        : m_width(width > 0 ? width : 0), m_height(height > 0 ? height : 0) {
        const std::size_t count = static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
        m_costs.assign(count, sanitizeCost(defaultCost));
    }

    bool NavGrid::inBounds(int x, int y) const {
        return x >= 0 && x < m_width && y >= 0 && y < m_height;
    }

    bool NavGrid::isWalkable(int x, int y) const {
        return cost(x, y) > 0.0f;
    }

    float NavGrid::cost(int x, int y) const {
        if (!inBounds(x, y)) {
            return 0.0f;
        }
        return m_costs[index(x, y)];
    }

    void NavGrid::setWalkable(int x, int y, bool walkable) {
        setCost(x, y, walkable ? 1.0f : 0.0f);
    }

    void NavGrid::setCost(int x, int y, float cellCost) {
        if (!inBounds(x, y)) {
            return;
        }
        m_costs[index(x, y)] = sanitizeCost(cellCost);
    }

    void NavGrid::fill(float cellCost) {
        std::fill(m_costs.begin(), m_costs.end(), sanitizeCost(cellCost));
    }

    void NavGrid::clearObstacles() {
        fill(1.0f);
    }

    std::size_t NavGrid::index(int x, int y) const {
        return toIndex(x, y, m_width);
    }

    // --- Pathfinding ---

    GridPathResult findGridPath(const NavGrid& grid, const glm::ivec2& start, const glm::ivec2& goal,
                                const GridPathOptions& options) {
        auto costFn = [&grid](const glm::ivec2& cell) { return grid.cost(cell); };
        return runAStar(grid.width(), grid.height(), start, goal, costFn, options);
    }

    GridPathResult findGridPath(int width, int height, const glm::ivec2& start, const glm::ivec2& goal,
                                const GridCostQuery& costQuery, const GridPathOptions& options) {
        if (!costQuery) {
            return GridPathResult{};
        }
        return runAStar(width, height, start, goal, costQuery, options);
    }

    std::vector<glm::ivec2> simplifyGridPath(const std::vector<glm::ivec2>& path) {
        if (path.size() <= 2) {
            return path;
        }

        std::vector<glm::ivec2> simplified;
        simplified.reserve(path.size());
        simplified.push_back(path.front());

        for (std::size_t i = 1; i + 1 < path.size(); ++i) {
            const glm::ivec2 prevDir = path[i] - path[i - 1];
            const glm::ivec2 nextDir = path[i + 1] - path[i];
            if (prevDir != nextDir) {
                simplified.push_back(path[i]);
            }
        }

        simplified.push_back(path.back());
        return simplified;
    }

} // namespace mge

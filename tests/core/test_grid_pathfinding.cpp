#include <doctest/doctest.h>
#include <mge/core/nav/GridPathfinding.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace mge;

namespace {

    // Builds a NavGrid from ASCII rows: '#' = wall, '.' = floor (cost 1), '~' = mud (cost 4).
    NavGrid gridFromAscii(const std::vector<std::string>& rows) {
        if (rows.empty() || rows[0].empty()) {
            return NavGrid{};
        }
        const int height = static_cast<int>(rows.size());
        const int width = static_cast<int>(rows[0].size());
        NavGrid grid(width, height, 1.0f);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const char ch = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                if (ch == '#') {
                    grid.setWalkable(x, y, false);
                } else if (ch == '~') {
                    grid.setCost(x, y, 4.0f);
                }
            }
        }
        return grid;
    }

    // Verifies every cell in the path is in bounds, walkable, and adjacent to the previous cell.
    void checkPathContinuity(const NavGrid& grid, const std::vector<glm::ivec2>& path, bool allowDiagonal) {
        REQUIRE_FALSE(path.empty());
        for (const glm::ivec2& cell : path) {
            CHECK(grid.inBounds(cell));
            CHECK(grid.isWalkable(cell));
        }
        for (std::size_t i = 1; i < path.size(); ++i) {
            const int dx = std::abs(path[i].x - path[i - 1].x);
            const int dy = std::abs(path[i].y - path[i - 1].y);
            if (allowDiagonal) {
                CHECK(dx <= 1);
                CHECK(dy <= 1);
                CHECK((dx + dy) >= 1);
            } else {
                CHECK((dx + dy) == 1);
            }
        }
    }

} // namespace

TEST_CASE("NavGrid: bounds, costs, obstacles, and invalid inputs") {
    NavGrid empty;
    CHECK(empty.width() == 0);
    CHECK(empty.height() == 0);
    CHECK_FALSE(empty.inBounds(0, 0));
    CHECK_FALSE(empty.isWalkable(0, 0));
    CHECK(empty.cost(0, 0) == 0.0f);

    NavGrid negative(-3, -2);
    CHECK(negative.width() == 0);
    CHECK(negative.height() == 0);

    NavGrid grid(4, 3, 1.5f);
    CHECK(grid.width() == 4);
    CHECK(grid.height() == 3);
    CHECK(grid.inBounds(0, 0));
    CHECK(grid.inBounds(3, 2));
    CHECK_FALSE(grid.inBounds(-1, 0));
    CHECK_FALSE(grid.inBounds(4, 0));
    CHECK_FALSE(grid.inBounds(0, 3));
    CHECK(grid.cost(1, 1) == doctest::Approx(1.5f));
    CHECK(grid.isWalkable({1, 1}));

    grid.setWalkable({1, 1}, false);
    CHECK_FALSE(grid.isWalkable(1, 1));
    CHECK(grid.cost(1, 1) == 0.0f);

    // Non-positive and non-finite costs mark the cell as blocked
    grid.setCost(2, 1, -5.0f);
    CHECK_FALSE(grid.isWalkable(2, 1));
    grid.setCost(2, 1, std::numeric_limits<float>::quiet_NaN());
    CHECK_FALSE(grid.isWalkable(2, 1));
    grid.setCost(2, 1, std::numeric_limits<float>::infinity());
    CHECK_FALSE(grid.isWalkable(2, 1));

    // Out-of-bounds writes are safely ignored
    grid.setCost(-1, -1, 2.0f);
    grid.setWalkable(10, 10, false);

    grid.clearObstacles();
    CHECK(grid.isWalkable(1, 1));
    CHECK(grid.cost(2, 1) == doctest::Approx(1.0f));

    grid.fill(3.0f);
    CHECK(grid.cost(0, 0) == doctest::Approx(3.0f));
    CHECK(grid.cost(3, 2) == doctest::Approx(3.0f));
}

TEST_CASE("Grid A*: trivial, blocked, and out-of-bounds cases") {
    NavGrid grid(5, 5);
    grid.setWalkable(2, 2, false);

    // Start == goal on walkable cell
    GridPathResult same = findGridPath(grid, {1, 1}, {1, 1});
    CHECK(same.found);
    REQUIRE(same.path.size() == 1);
    CHECK(same.path[0] == glm::ivec2(1, 1));
    CHECK(same.totalCost == 0.0f);
    CHECK(same.nodesExpanded == 0);

    // Start == goal on blocked cell fails
    CHECK_FALSE(findGridPath(grid, {2, 2}, {2, 2}).found);

    // Blocked start or blocked goal fails
    CHECK_FALSE(findGridPath(grid, {2, 2}, {4, 4}).found);
    CHECK_FALSE(findGridPath(grid, {0, 0}, {2, 2}).found);

    // Out-of-bounds start or goal fails
    CHECK_FALSE(findGridPath(grid, {-1, 0}, {4, 4}).found);
    CHECK_FALSE(findGridPath(grid, {0, 0}, {5, 4}).found);

    // Empty grid and null callback fail
    CHECK_FALSE(findGridPath(NavGrid{}, {0, 0}, {0, 0}).found);
    CHECK_FALSE(findGridPath(5, 5, {0, 0}, {4, 4}, GridCostQuery{}).found);
}

TEST_CASE("Grid A*: 4-directional path around obstacles and unreachable goal") {
    NavGrid grid = gridFromAscii({
        ".....",
        ".###.",
        "...#.",
        ".###.",
        ".....",
    });

    GridPathResult result = findGridPath(grid, {2, 2}, {4, 2});
    REQUIRE(result.found);
    CHECK(result.path.front() == glm::ivec2(2, 2));
    CHECK(result.path.back() == glm::ivec2(4, 2));
    checkPathContinuity(grid, result.path, false);
    // Must exit left at (0,2), go around top or bottom to (4,2): length 10 steps
    CHECK(result.totalCost == doctest::Approx(10.0f));
    CHECK(result.path.size() == 11);

    // Enclose the goal completely -> unreachable
    NavGrid sealed = gridFromAscii({
        ".....",
        ".###.",
        ".#.#.",
        ".###.",
        ".....",
    });
    GridPathResult unreachable = findGridPath(sealed, {0, 0}, {2, 2});
    CHECK_FALSE(unreachable.found);
    CHECK(unreachable.path.empty());
    CHECK(unreachable.totalCost == 0.0f);
    CHECK(unreachable.nodesExpanded > 0);
}

TEST_CASE("Grid A*: 8-directional movement and corner-cutting prevention") {
    constexpr float kSqrt2 = 1.4142135623730950488f;

    // Open 5x5 grid: diagonal from (0,0) to (4,4) takes 4 diagonal steps
    NavGrid open(5, 5);
    GridPathOptions diagOptions;
    diagOptions.allowDiagonal = true;
    GridPathResult diag = findGridPath(open, {0, 0}, {4, 4}, diagOptions);
    REQUIRE(diag.found);
    CHECK(diag.path.size() == 5);
    CHECK(diag.totalCost == doctest::Approx(4.0f * kSqrt2));
    checkPathContinuity(open, diag.path, true);

    // Two walls touching diagonally at (1,0) and (0,1) block diagonal move from (0,0) to (1,1)
    NavGrid corner = gridFromAscii({
        ".#.",
        "#..",
        "...",
    });

    GridPathOptions noCut;
    noCut.allowDiagonal = true;
    noCut.preventCornerCutting = true;
    CHECK_FALSE(findGridPath(corner, {0, 0}, {1, 1}, noCut).found);

    GridPathOptions allowCut;
    allowCut.allowDiagonal = true;
    allowCut.preventCornerCutting = false;
    GridPathResult squeezed = findGridPath(corner, {0, 0}, {1, 1}, allowCut);
    REQUIRE(squeezed.found);
    CHECK(squeezed.path.size() == 2);
    CHECK(squeezed.totalCost == doctest::Approx(kSqrt2));
}

TEST_CASE("Grid A*: weighted terrain prefers cheaper detour") {
    // Direct path across row 1 crosses 3 mud cells (cost 4 each = 12 + 1 = 13).
    // Going around row 0 or row 2 costs 6 normal steps (cost 6).
    NavGrid grid = gridFromAscii({
        ".....",
        ".~~~.",
        ".....",
    });

    GridPathResult result = findGridPath(grid, {0, 1}, {4, 1});
    REQUIRE(result.found);
    checkPathContinuity(grid, result.path, false);
    CHECK(result.totalCost == doctest::Approx(6.0f));
    for (const glm::ivec2& cell : result.path) {
        CHECK(grid.cost(cell) == doctest::Approx(1.0f));
    }

    // If the wall forces crossing a single mud cell, cost reflects the mud penalty
    NavGrid narrow = gridFromAscii({
        ".#.",
        ".~.",
        ".#.",
    });
    GridPathResult forced = findGridPath(narrow, {0, 1}, {2, 1});
    REQUIRE(forced.found);
    CHECK(forced.totalCost == doctest::Approx(5.0f)); // 4.0 (mud) + 1.0 (goal)
}

TEST_CASE("Grid A*: heuristic expands fewer nodes than Dijkstra and respects node budget") {
    NavGrid open(25, 25);

    GridPathOptions astarOpts;
    astarOpts.heuristicWeight = 1.0f;
    GridPathResult astar = findGridPath(open, {2, 2}, {22, 20}, astarOpts);

    GridPathOptions dijkstraOpts;
    dijkstraOpts.heuristicWeight = 0.0f;
    GridPathResult dijkstra = findGridPath(open, {2, 2}, {22, 20}, dijkstraOpts);

    REQUIRE(astar.found);
    REQUIRE(dijkstra.found);
    CHECK(astar.totalCost == doctest::Approx(dijkstra.totalCost));
    CHECK(astar.nodesExpanded < dijkstra.nodesExpanded);

    // Search budget smaller than required expansions aborts cleanly
    GridPathOptions tightBudget;
    tightBudget.maxExpandedNodes = 5;
    GridPathResult aborted = findGridPath(open, {0, 0}, {24, 24}, tightBudget);
    CHECK_FALSE(aborted.found);
    CHECK(aborted.nodesExpanded == 5);
}

TEST_CASE("Grid A*: custom cost callback and determinism") {
    GridCostQuery query = [](const glm::ivec2& cell) -> float {
        if (cell.x == 3 && cell.y >= 0 && cell.y <= 3) {
            return 0.0f; // Vertical wall leaving gap at y = 4
        }
        return 1.0f;
    };

    GridPathResult first = findGridPath(6, 6, {1, 1}, {5, 1}, query);
    GridPathResult second = findGridPath(6, 6, {1, 1}, {5, 1}, query);

    REQUIRE(first.found);
    REQUIRE(second.found);
    CHECK(first.path == second.path);
    CHECK(first.totalCost == doctest::Approx(second.totalCost));
    CHECK(first.nodesExpanded == second.nodesExpanded);
    CHECK(first.path.front() == glm::ivec2(1, 1));
    CHECK(first.path.back() == glm::ivec2(5, 1));
}

TEST_CASE("simplifyGridPath: removes redundant collinear waypoints") {
    CHECK(simplifyGridPath({}).empty());

    const std::vector<glm::ivec2> single = {{2, 3}};
    CHECK(simplifyGridPath(single) == single);

    const std::vector<glm::ivec2> pair = {{0, 0}, {1, 0}};
    CHECK(simplifyGridPath(pair) == pair);

    // Straight horizontal then vertical then diagonal
    const std::vector<glm::ivec2> full = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {3, 1}, {3, 2}, {4, 3}, {5, 4}};
    const std::vector<glm::ivec2> expected = {{0, 0}, {3, 0}, {3, 2}, {5, 4}};
    CHECK(simplifyGridPath(full) == expected);
}

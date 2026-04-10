#include "AStarRouter.h"
#include <cmath>
#include <queue>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <limits>

// ─── Grid hash for unordered_map ─────────────────────────────────────────────

struct GridHash {
    size_t operator()(const GridCell& c) const {
        size_t h = static_cast<size_t>(c.x);
        h ^= static_cast<size_t>(c.y) * 2654435761u;
        h ^= static_cast<size_t>(c.z) * 805459861u;
        return h;
    }
};

// ─── A* node ─────────────────────────────────────────────────────────────────

struct Node {
    GridCell cell;
    double   g = 0.0;   // cost from start
    double   f = 0.0;   // g + heuristic
    bool operator>(const Node& o) const { return f > o.f; }
};

// ─── AStarRouter ─────────────────────────────────────────────────────────────

AStarRouter::AStarRouter(
    double workspaceW, double workspaceH, double workspaceZ,
    double gridRes, double softPenalty)
    : res_(gridRes), softPenalty_(softPenalty)
{
    gx_ = static_cast<int>(std::ceil(workspaceW / gridRes));
    gy_ = static_cast<int>(std::ceil(workspaceH / gridRes));
    gz_ = static_cast<int>(std::ceil(workspaceZ / gridRes));
}

void AStarRouter::blockMachine(const Machine& m) {
    // Cover the machine footprint + 1-cell margin (keeps pipes away from shells).
    int x0 = static_cast<int>((m.position.x - m.size.x/2.0 - res_) / res_);
    int x1 = static_cast<int>((m.position.x + m.size.x/2.0 + res_) / res_);
    int y0 = static_cast<int>((m.position.y - m.size.y/2.0 - res_) / res_);
    int y1 = static_cast<int>((m.position.y + m.size.y/2.0 + res_) / res_);
    int z0 = 0;
    int z1 = static_cast<int>(m.size.z / res_);

    for (int x = x0; x <= x1; ++x)
    for (int y = y0; y <= y1; ++y)
    for (int z = z0; z <= z1; ++z) {
        GridCell c{x, y, z};
        if (inBounds(c)) hardBlocks_.insert(c);
    }
}

std::vector<AcGePoint3d> AStarRouter::route(
    const AcGePoint3d& start, const AcGePoint3d& end)
{
    GridCell src = toCell(start);
    GridCell dst = toCell(end);

    if (src == dst) return { toWorld(src) };

    // Remove start/end from hard blocks so ports can be reached.
    hardBlocks_.erase(src);
    hardBlocks_.erase(dst);

    using Map = std::unordered_map<GridCell, GridCell,  GridHash>;
    using GMap= std::unordered_map<GridCell, double,    GridHash>;

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    Map   cameFrom;
    GMap  gScore;

    auto heuristic = [&](const GridCell& a, const GridCell& b) {
        return static_cast<double>(
            std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z));
    };

    gScore[src] = 0.0;
    open.push({ src, 0.0, heuristic(src, dst) });

    const int dx[] = {1,-1,0,0,0,0};
    const int dy[] = {0,0,1,-1,0,0};
    const int dz[] = {0,0,0,0,1,-1};

    while (!open.empty()) {
        Node cur = open.top(); open.pop();

        if (cur.cell == dst) {
            // Reconstruct path
            std::vector<GridCell> cells;
            GridCell c = dst;
            while (!(c == src)) {
                cells.push_back(c);
                c = cameFrom[c];
            }
            cells.push_back(src);
            std::reverse(cells.begin(), cells.end());

            std::vector<AcGePoint3d> path;
            path.reserve(cells.size());
            for (auto& gc : cells) path.push_back(toWorld(gc));
            return path;
        }

        double curG = gScore.count(cur.cell) ? gScore[cur.cell]
                                             : std::numeric_limits<double>::infinity();

        for (int d = 0; d < 6; ++d) {
            GridCell nb{ cur.cell.x + dx[d],
                         cur.cell.y + dy[d],
                         cur.cell.z + dz[d] };
            if (!inBounds(nb)) continue;
            if (hardBlocks_.count(nb)) continue;

            double step = res_;
            if (softBlocks_.count(nb)) step += softPenalty_;

            double newG = curG + step;
            double prevG = gScore.count(nb) ? gScore[nb]
                                            : std::numeric_limits<double>::infinity();
            if (newG < prevG) {
                gScore[nb]   = newG;
                cameFrom[nb] = cur.cell;
                open.push({ nb, newG, newG + heuristic(nb, dst) });
            }
        }
    }
    return {};  // no path
}

void AStarRouter::addPipeObstacle(const std::vector<AcGePoint3d>& path) {
    for (const auto& pt : path) {
        GridCell c = toCell(pt);
        if (inBounds(c)) softBlocks_.insert(c);
    }
}

GridCell AStarRouter::toCell(const AcGePoint3d& p) const {
    int cx = std::max(0, std::min(gx_-1, static_cast<int>(p.x / res_)));
    int cy = std::max(0, std::min(gy_-1, static_cast<int>(p.y / res_)));
    int cz = std::max(0, std::min(gz_-1, static_cast<int>(p.z / res_)));
    return { cx, cy, cz };
}

AcGePoint3d AStarRouter::toWorld(const GridCell& c) const {
    return AcGePoint3d(
        (c.x + 0.5) * res_,
        (c.y + 0.5) * res_,
        (c.z + 0.5) * res_);
}

bool AStarRouter::inBounds(const GridCell& c) const {
    return c.x >= 0 && c.x < gx_ &&
           c.y >= 0 && c.y < gy_ &&
           c.z >= 0 && c.z < gz_;
}
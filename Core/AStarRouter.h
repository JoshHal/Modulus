#pragma once
#include "Models.h"
#include <vector>
#include <set>

// ─── Manhattan A* Pipe Router ─────────────────────────────────────────────────
//
// Grid-based 3-D A* using Manhattan distance as heuristic.
// Moves: ±X, ±Y, ±Z (6 neighbours).
//
// Obstacle map
//   - Machine footprints are HARD blocks (cost = infinity, skip neighbour).
//   - Previously routed pipe cells are SOFT penalty (cost += softPenalty).
//
// Usage:
//   AStarRouter router(workspaceW, workspaceH, workspaceZ, gridRes);
//   router.blockMachine(m);       // call for every machine before routing
//   std::vector<AcGePoint3d> path = router.route(startPt, endPt);
//   router.addPipeObstacle(path); // mark the path as soft obstacle
// ─────────────────────────────────────────────────────────────────────────────

struct GridCell {
    int x = 0, y = 0, z = 0;
    bool operator==(const GridCell& o) const { return x==o.x && y==o.y && z==o.z; }
    bool operator<(const GridCell& o)  const {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return z < o.z;
    }
};

class AStarRouter {
public:
    AStarRouter(
        double workspaceW,
        double workspaceH,
        double workspaceZ   = 6.0,
        double gridRes      = Limits::GridRes,
        double softPenalty  = 8.0);

    // Mark all cells covered by a machine as hard obstacles.
    void blockMachine(const Machine& m);

    // Find shortest Manhattan path from start to end.
    // Returns empty vector if no path found.
    std::vector<AcGePoint3d> route(
        const AcGePoint3d& start,
        const AcGePoint3d& end);

    // After routing, mark the path cells as soft obstacles for future pipes.
    void addPipeObstacle(const std::vector<AcGePoint3d>& path);

private:
    // Convert world point → grid cell (clamps to grid bounds).
    GridCell toCell(const AcGePoint3d& p) const;
    // Convert grid cell → world centre point.
    AcGePoint3d toWorld(const GridCell& c) const;

    // Cell validity check.
    bool inBounds(const GridCell& c) const;

    int   gx_, gy_, gz_;          // grid dimensions
    double res_, softPenalty_;

    std::set<GridCell> hardBlocks_;
    std::set<GridCell> softBlocks_;
};
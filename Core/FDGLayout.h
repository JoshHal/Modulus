#pragma once
#include "Models.h"
#include <vector>

// ─── Force-Directed Graph Layout ─────────────────────────────────────────────
//
// Fruchterman-Reingold variant adapted for a bounded 2-D workspace.
// Only X and Y are moved; Z stays at 0 for all machines.
//
// Repulsion  : F_r = k² / d   (all pairs)
// Attraction : F_a = d² / k   (connected pairs)
//
// After each iteration positions are clamped to the workspace rectangle,
// keeping a half-machine-size margin so machines don't straddle the edge.
//
// The connectivity list is built from the Pipe vector: each pipe adds an
// attractive edge between fromMachineId and toMachineId.
// ─────────────────────────────────────────────────────────────────────────────

class FDGLayout {
public:
    // Runs the full layout pipeline in-place on the machines vector.
    // machines     : list to reposition (positions are modified)
    // pipes        : connectivity for attraction forces
    // workspaceW/H : bounding box in metres (origin at 0,0)
    // iterations   : number of FDG steps (default Limits::FDGIterations)
    static void run(
        std::vector<Machine>& machines,
        const std::vector<Pipe>& pipes,
        double workspaceW   = Limits::WorkspaceW,
        double workspaceH   = Limits::WorkspaceH,
        int    iterations   = Limits::FDGIterations);

private:
    // Spread machines in a grid before iterating, giving FDG a clean start.
    static void initialGridPlacement(
        std::vector<Machine>& machines,
        double workspaceW, double workspaceH);

    // Clamp a single machine so it stays inside the workspace boundary.
    static void clamp(Machine& m, double workspaceW, double workspaceH);
};
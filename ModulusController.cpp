#pragma once
#include "../Core/Models.h"
#include "../Core/FDGLayout.h"
#include "../Core/AStarRouter.h"
#include "../Core/PipeProperties.h"
#include "../CADAdapter/CADAdapter.h"
#include <vector>
#include <stdexcept>

// ─── ModulusController ───────────────────────────────────────────────────────
//
// Owns all machine and pipe data.  Provides two high-level operations:
//
//   generateLayout()  — runs FDG, positions machines
//   routeAndDraw()    — runs A* per pipe, computes properties, writes DWG
//
// The controller is populated by the UI layer before either call is made.
// Sample data (buildSampleScene) is provided for quick testing.
// ─────────────────────────────────────────────────────────────────────────────

class ModulusController {
public:
    ModulusController()  = default;
    ~ModulusController() = default;

    // ── Scene data ────────────────────────────────────────────────────────

    std::vector<Machine> machines;
    std::vector<Pipe>    pipes;

    // Replace current data with a hard-coded 4-machine / 5-pipe demo scene.
    void buildSampleScene();

    // ── Pipeline steps ────────────────────────────────────────────────────

    // Step 1: Run FDG layout on machines.
    //         Updates machine.position in-place.
    //         No DWG writes occur here.
    void generateLayout();

    // Step 2: Route every pipe, compute properties, then write all entities.
    //         Must be called AFTER generateLayout().
    //         All AcDb writes happen on the calling (main) thread.
    void routeAndDraw();

    // ── Accessors (for Qt UI) ─────────────────────────────────────────────

    const std::vector<Machine>& getMachines() const { return machines; }
    const std::vector<Pipe>&    getPipes()    const { return pipes; }

private:
    bool layoutDone_ = false;
};
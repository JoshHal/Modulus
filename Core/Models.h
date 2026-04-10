#pragma once
#include <string>
#include <vector>
#include <acgepoint3d.h>
#include <acgevector3d.h>

// ─── Enumerations ────────────────────────────────────────────────────────────

enum class MaterialType { Liquid, Gas, Solid, Sludge };
enum class ServiceType  { Water, Air, Slurry, Chemical };

// ─── Machine ─────────────────────────────────────────────────────────────────

struct Machine {
    int             id         = 0;
    std::string     name;
    AcGePoint3d     position;
    AcGeVector3d    size       = AcGeVector3d(4.0, 4.0, 4.0);   // metres
    std::vector<AcGePoint3d> ports;   // connection points in world space
    std::string     modelPath;         // DWG block name
};

// ─── Pipe ────────────────────────────────────────────────────────────────────

struct Pipe {
    int              id            = 0;
    int              fromMachineId = -1;
    int              toMachineId   = -1;
    int              fromPortIndex = 0;
    int              toPortIndex   = 0;

    std::vector<AcGePoint3d> path;   // A* waypoints

    double           diameter      = 0.10;
    MaterialType     material      = MaterialType::Liquid;
    ServiceType      service       = ServiceType::Water;

    double           length        = 0.0;
    double           cost          = 0.0;

    int              lineNumber    = 0;
    std::string      specCode;
    std::string      tag;
};

// ─── Constants ───────────────────────────────────────────────────────────────

namespace Limits {
    constexpr int    MaxMachines   = 5;
    constexpr int    MaxPipes      = 6;
    constexpr double WorkspaceW    = 50.0;   // metres
    constexpr double WorkspaceH    = 50.0;
    constexpr double GridRes       = 1.0;    // A* grid cell size (metres)
    constexpr double ArrowInterval = 2.0;    // metres between flow arrows
    constexpr int    FDGIterations = 100;
}
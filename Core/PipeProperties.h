#pragma once
#include "Models.h"
#include <string>
#include <cmath>
#include <sstream>
#include <iomanip>

// ─── PipeProperties ──────────────────────────────────────────────────────────
//
// All functions are static and pure: same inputs always produce same outputs.
// ─────────────────────────────────────────────────────────────────────────────

class PipeProperties {
public:

    // ── Diameter (metres) ──────────────────────────────────────────────────

    static double diameter(MaterialType mat) {
        switch (mat) {
            case MaterialType::Gas:    return 0.15;
            case MaterialType::Liquid: return 0.10;
            case MaterialType::Solid:  return 0.25;
            case MaterialType::Sludge: return 0.30;
        }
        return 0.10;
    }

    // ── Cost ($/metre · material factor · diameter) ────────────────────────
    //
    // Cost = Length × materialCost × diameter
    // Material cost table ($/m·m):
    //   Gas    = 120
    //   Liquid = 100
    //   Solid  = 180
    //   Sludge = 220

    static double materialCost(MaterialType mat) {
        switch (mat) {
            case MaterialType::Gas:    return 120.0;
            case MaterialType::Liquid: return 100.0;
            case MaterialType::Solid:  return 180.0;
            case MaterialType::Sludge: return 220.0;
        }
        return 100.0;
    }

    static double computeCost(double length, MaterialType mat, double diam) {
        return length * materialCost(mat) * diam;
    }

    // ── Path length (sum of segment lengths) ─────────────────────────────

    static double computeLength(const std::vector<AcGePoint3d>& path) {
        double len = 0.0;
        for (size_t i = 1; i < path.size(); ++i) {
            AcGeVector3d v = path[i] - path[i-1];
            len += v.length();
        }
        return len;
    }

    // ── Default spec code per material ──────────────────────────────────

    static std::string specCode(MaterialType mat) {
        switch (mat) {
            case MaterialType::Gas:    return "CS150";
            case MaterialType::Liquid: return "CS150";
            case MaterialType::Solid:  return "HDPE";
            case MaterialType::Sludge: return "SS316";
        }
        return "CS150";
    }

    // ── Material code (one or two letters) ─────────────────────────────

    static std::string materialCode(MaterialType mat) {
        switch (mat) {
            case MaterialType::Gas:    return "G";
            case MaterialType::Liquid: return "L";
            case MaterialType::Solid:  return "S";
            case MaterialType::Sludge: return "SL";
        }
        return "L";
    }

    // ── Service code ────────────────────────────────────────────────────

    static std::string serviceCode(ServiceType svc) {
        switch (svc) {
            case ServiceType::Water:    return "W";
            case ServiceType::Air:      return "AIR";
            case ServiceType::Slurry:   return "SLURRY";
            case ServiceType::Chemical: return "CHEM";
        }
        return "W";
    }

    // ── P&ID Tag ─────────────────────────────────────────────────────────
    //
    // Format: [Line]-[Size]-[Material]-[Service]-[Spec]
    // Example: 100-100-L-W-CS150
    //
    //   Line   = 100 + pipe.id * 100
    //   Size   = diameter_mm (integer)
    //   Material / Service / Spec from lookup tables above.

    static std::string generateTag(const Pipe& p) {
        int line     = 100 + p.id * 100;
        int sizeMm   = static_cast<int>(std::round(p.diameter * 1000.0));

        std::ostringstream oss;
        oss << line
            << "-" << sizeMm
            << "-" << materialCode(p.material)
            << "-" << serviceCode(p.service)
            << "-" << p.specCode;
        return oss.str();
    }

    // ── Assign all computed fields to a Pipe ──────────────────────────────

    static void populate(Pipe& pipe) {
        pipe.diameter   = diameter(pipe.material);
        pipe.specCode   = specCode(pipe.material);
        pipe.lineNumber = 100 + pipe.id * 100;
        pipe.length     = computeLength(pipe.path);
        pipe.cost       = computeCost(pipe.length, pipe.material, pipe.diameter);
        pipe.tag        = generateTag(pipe);
    }
};
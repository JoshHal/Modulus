#pragma once
#include "../Core/Models.h"
#include <vector>
#include <string>

// ObjectARX headers (supplied by AutoCAD SDK)
#include <acdb.h>
#include <dbmain.h>
#include <dbents.h>
#include <dbmtext.h>
#include <db3dPoly.h>
#include <dbBlockTableRecord.h>
#include <acge.h>
#include <acgiviewportgeometry.h>

// ─── CADAdapter ──────────────────────────────────────────────────────────────
//
// All public methods must be called from the AutoCAD main thread.
// They write directly into modelspace of the current DWG database.
//
// Color constants follow AutoCAD color index (ACI):
//   Blue   = 5    (Liquid)
//   Yellow = 2    (Gas)
//   Brown  = custom RGB via AcCmColor  (Solid)
//   Grey   = 8    (Sludge)
// ─────────────────────────────────────────────────────────────────────────────

class CADAdapter {
public:

    // ── Machines ──────────────────────────────────────────────────────────

    // Insert a DWG block reference for each machine.
    // If the named block doesn't exist in the DWG, a placeholder rectangle
    // is drawn instead (ensures visible output even without external DWGs).
    static void drawMachines(const std::vector<Machine>& machines);

    // Place an AcDbMText label above each machine showing name + ID.
    static void labelMachines(const std::vector<Machine>& machines);

    // ── Pipes ─────────────────────────────────────────────────────────────

    // Draw a 3D polyline for each pipe path.
    // Colour and lineweight are set from the pipe's material type.
    static void drawPipes(const std::vector<Pipe>& pipes);

    // Insert flow-arrow block references along each pipe every ~2 m.
    // The arrow block is rotated to match the pipe segment direction.
    // If MODULUS_ARROW block is absent, a simple solid triangle is drawn.
    static void drawFlowArrows(const std::vector<Pipe>& pipes);

    // Place an AcDbMText tag at the midpoint of each pipe.
    // The tag text is pipe.tag; rotation follows pipe direction.
    static void drawPipeTags(const std::vector<Pipe>& pipes);

    // ── Legend ────────────────────────────────────────────────────────────

    // Place a multi-line MText block in the lower-right corner of the drawing,
    // summarising tag format, material codes, service codes, and spec codes.
    static void drawLegend(double workspaceW = Limits::WorkspaceW,
                           double workspaceH = Limits::WorkspaceH);

private:

    // ── Utilities ─────────────────────────────────────────────────────────

    // Return the model-space block table record for writing.
    static AcDbBlockTableRecord* modelSpace();

    // Return AutoCAD color index for a material type.
    static Adesk::Int16 materialColor(MaterialType mat);

    // Return lineweight (in hundredths of mm) scaled by diameter.
    static AcDb::LineWeight materialLineWeight(double diameter);

    // Append an entity to modelspace and close it.
    static Acad::ErrorStatus appendAndClose(AcDbEntity* ent);

    // Draw a solid triangular arrow at 'position', rotated by 'angle' radians.
    static void drawArrowTriangle(const AcGePoint3d& position, double angle);

    // Insert block reference by name; returns true if block exists in DB.
    static bool insertBlock(
        const std::string&  blockName,
        const AcGePoint3d&  position,
        double              rotation  = 0.0,
        double              scale     = 1.0);

    // Compute the direction angle (atan2) of the segment around the midpoint
    // of a polyline path.
    static double pathMidAngle(const std::vector<AcGePoint3d>& path);

    // Find the world-space midpoint of a polyline path.
    static AcGePoint3d pathMidpoint(const std::vector<AcGePoint3d>& path);
};
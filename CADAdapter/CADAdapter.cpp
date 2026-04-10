#include "CADAdapter.h"
#include <cmath>
#include <sstream>
#include <algorithm>

// ─── Internal helpers ─────────────────────────────────────────────────────────

AcDbBlockTableRecord* CADAdapter::modelSpace() {
    AcDbDatabase* db = acdbHostApplicationServices()->workingDatabase();
    if (!db) return nullptr;

    AcDbBlockTable* bt = nullptr;
    db->getBlockTable(bt, AcDb::kForRead);
    if (!bt) return nullptr;

    AcDbBlockTableRecord* ms = nullptr;
    bt->getAt(ACDB_MODEL_SPACE, ms, AcDb::kForWrite);
    bt->close();
    return ms;
}

Acad::ErrorStatus CADAdapter::appendAndClose(AcDbEntity* ent) {
    AcDbBlockTableRecord* ms = modelSpace();
    if (!ms) return Acad::eNoDatabase;
    AcDbObjectId id;
    Acad::ErrorStatus es = ms->appendAcDbEntity(id, ent);
    ms->close();
    ent->close();
    return es;
}

Adesk::Int16 CADAdapter::materialColor(MaterialType mat) {
    switch (mat) {
        case MaterialType::Liquid: return 5;    // ACI Blue
        case MaterialType::Gas:    return 2;    // ACI Yellow
        case MaterialType::Solid:  return 42;   // ACI Brown-ish
        case MaterialType::Sludge: return 8;    // ACI Dark Grey
    }
    return 7; // white
}

AcDb::LineWeight CADAdapter::materialLineWeight(double diameter) {
    // diameter in metres → approximate lineweight
    int lw = static_cast<int>(diameter * 1000.0 / 10.0) * 10;
    lw = std::max(25, std::min(200, lw));
    // Round to nearest valid AcDb::LineWeight enum value (25,50,70,100,140,200)
    if      (lw <= 35) return AcDb::kLnWt025;
    else if (lw <= 60) return AcDb::kLnWt050;
    else if (lw <= 85) return AcDb::kLnWt070;
    else if (lw <= 120)return AcDb::kLnWt100;
    else if (lw <= 170)return AcDb::kLnWt140;
    else               return AcDb::kLnWt200;
}

bool CADAdapter::insertBlock(
    const std::string& blockName,
    const AcGePoint3d& position,
    double rotation, double scale)
{
    AcDbDatabase* db = acdbHostApplicationServices()->workingDatabase();
    if (!db) return false;

    AcDbBlockTable* bt = nullptr;
    db->getBlockTable(bt, AcDb::kForRead);
    if (!bt) return false;

    AcDbObjectId blkId;
    bool found = (bt->getAt(blockName.c_str(), blkId) == Acad::eOk);
    bt->close();
    if (!found) return false;

    AcDbBlockReference* ref = new AcDbBlockReference(position, blkId);
    ref->setScaleFactors(AcGeScale3d(scale));
    ref->setRotation(rotation);
    appendAndClose(ref);
    return true;
}

AcGePoint3d CADAdapter::pathMidpoint(const std::vector<AcGePoint3d>& path) {
    if (path.empty()) return AcGePoint3d::kOrigin;
    double totalLen = 0.0;
    std::vector<double> lens(path.size() - 1);
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        lens[i] = (path[i+1] - path[i]).length();
        totalLen += lens[i];
    }
    double half = totalLen / 2.0;
    double acc  = 0.0;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        if (acc + lens[i] >= half) {
            double t = (half - acc) / lens[i];
            return path[i] + (path[i+1] - path[i]) * t;
        }
        acc += lens[i];
    }
    return path.back();
}

double CADAdapter::pathMidAngle(const std::vector<AcGePoint3d>& path) {
    if (path.size() < 2) return 0.0;
    // Find segment containing the midpoint and return its direction.
    double totalLen = 0.0;
    std::vector<double> lens(path.size() - 1);
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        lens[i] = (path[i+1] - path[i]).length();
        totalLen += lens[i];
    }
    double half = totalLen / 2.0;
    double acc  = 0.0;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        if (acc + lens[i] >= half) {
            AcGeVector3d v = path[i+1] - path[i];
            return std::atan2(v.y, v.x);
        }
        acc += lens[i];
    }
    AcGeVector3d v = path.back() - path[path.size()-2];
    return std::atan2(v.y, v.x);
}

// ─── Machines ─────────────────────────────────────────────────────────────────

void CADAdapter::drawMachines(const std::vector<Machine>& machines) {
    for (const auto& m : machines) {
        bool inserted = insertBlock(m.modelPath, m.position);
        if (!inserted) {
            // Fallback: draw a simple 2-D rectangle outline
            double hw = m.size.x / 2.0;
            double hh = m.size.y / 2.0;

            AcGePoint3dArray pts;
            pts.append(AcGePoint3d(m.position.x - hw, m.position.y - hh, 0));
            pts.append(AcGePoint3d(m.position.x + hw, m.position.y - hh, 0));
            pts.append(AcGePoint3d(m.position.x + hw, m.position.y + hh, 0));
            pts.append(AcGePoint3d(m.position.x - hw, m.position.y + hh, 0));
            pts.append(AcGePoint3d(m.position.x - hw, m.position.y - hh, 0)); // close

            AcDb3dPolyline* poly = new AcDb3dPolyline(AcDb::k3dSimplePoly, pts, true);
            poly->setColorIndex(7);
            appendAndClose(poly);
        }
    }
}

void CADAdapter::labelMachines(const std::vector<Machine>& machines) {
    for (const auto& m : machines) {
        std::ostringstream txt;
        txt << m.name << "  [M" << m.id << "]";

        AcDbMText* mtext = new AcDbMText();
        AcGePoint3d labelPt(m.position.x, m.position.y + m.size.y/2.0 + 1.0, 0.0);
        mtext->setLocation(labelPt);
        mtext->setContents(txt.str().c_str());
        mtext->setTextHeight(0.8);
        mtext->setAttachment(AcDbMText::kBottomCenter);
        mtext->setColorIndex(7);
        appendAndClose(mtext);
    }
}

// ─── Pipes ────────────────────────────────────────────────────────────────────

void CADAdapter::drawPipes(const std::vector<Pipe>& pipes) {
    for (const auto& pipe : pipes) {
        if (pipe.path.size() < 2) continue;

        AcGePoint3dArray pts;
        for (const auto& pt : pipe.path) pts.append(pt);

        AcDb3dPolyline* poly = new AcDb3dPolyline(AcDb::k3dSimplePoly, pts);
        poly->setColorIndex(materialColor(pipe.material));
        poly->setLineWeight(materialLineWeight(pipe.diameter));
        appendAndClose(poly);
    }
}

void CADAdapter::drawArrowTriangle(const AcGePoint3d& pos, double angle) {
    // Small solid triangle (0.5m base × 0.8m height)
    const double base   = 0.25;
    const double height = 0.40;

    double ca = std::cos(angle), sa = std::sin(angle);
    // Tip, left base, right base
    AcGePoint3dArray pts;
    pts.append(AcGePoint3d(pos.x + ca*height,         pos.y + sa*height,         pos.z));
    pts.append(AcGePoint3d(pos.x - sa*base - ca*height/2,
                           pos.y + ca*base - sa*height/2, pos.z));
    pts.append(AcGePoint3d(pos.x + sa*base - ca*height/2,
                           pos.y - ca*base - sa*height/2, pos.z));
    pts.append(pts[0]); // close

    AcDb3dPolyline* tri = new AcDb3dPolyline(AcDb::k3dSimplePoly, pts, true);
    tri->setColorIndex(7);
    appendAndClose(tri);
}

void CADAdapter::drawFlowArrows(const std::vector<Pipe>& pipes) {
    for (const auto& pipe : pipes) {
        if (pipe.path.size() < 2) continue;

        double acc = 0.0;
        double nextArrow = Limits::ArrowInterval;

        for (size_t i = 0; i + 1 < pipe.path.size(); ++i) {
            AcGeVector3d seg = pipe.path[i+1] - pipe.path[i];
            double segLen    = seg.length();
            double angle     = std::atan2(seg.y, seg.x);

            while (acc + segLen >= nextArrow) {
                double t = (nextArrow - acc) / segLen;
                AcGePoint3d arrowPt = pipe.path[i] + seg * t;

                // Prefer block, fall back to triangle
                bool ok = insertBlock("MODULUS_ARROW", arrowPt, angle, 0.5);
                if (!ok) drawArrowTriangle(arrowPt, angle);

                nextArrow += Limits::ArrowInterval;
            }
            acc += segLen;
        }
    }
}

void CADAdapter::drawPipeTags(const std::vector<Pipe>& pipes) {
    for (const auto& pipe : pipes) {
        if (pipe.path.empty()) continue;

        AcGePoint3d  mid   = pathMidpoint(pipe.path);
        double       angle = pathMidAngle(pipe.path);

        // Offset tag 0.5 m perpendicular to pipe direction
        mid.y += 0.5;

        AcDbMText* mtext = new AcDbMText();
        mtext->setLocation(mid);
        mtext->setContents(pipe.tag.c_str());
        mtext->setTextHeight(0.5);
        mtext->setRotation(angle);
        mtext->setAttachment(AcDbMText::kBottomCenter);
        mtext->setColorIndex(materialColor(pipe.material));
        appendAndClose(mtext);
    }
}

// ─── Legend ───────────────────────────────────────────────────────────────────

void CADAdapter::drawLegend(double workspaceW, double workspaceH) {
    const std::string legendText =
        "{\\fAnthropicSans|b1;MODULUS LITE — LEGEND}\\P"
        "\\P"
        "{\\b;Tag Format:}  [Line]-[Size]-[Material]-[Service]-[Spec]\\P"
        "  Example: 100-100-L-W-CS150\\P"
        "\\P"
        "{\\b;Material codes:}\\P"
        "  L = Liquid   G = Gas   S = Solid   SL = Sludge\\P"
        "\\P"
        "{\\b;Service codes:}\\P"
        "  W = Water   AIR = Air   SLURRY = Slurry   CHEM = Chemical\\P"
        "\\P"
        "{\\b;Spec codes:}\\P"
        "  CS150 = Carbon Steel 150#\\P"
        "  SS316 = Stainless 316\\P"
        "  PVC   = PVC Schedule 80\\P"
        "  HDPE  = High-Density PE\\P"
        "\\P"
        "{\\b;Pipe colours:}\\P"
        "  Blue=Liquid   Yellow=Gas   Brown=Solid   Grey=Sludge\\P"
        "\\P"
        "{\\b;Flow direction:} Triangular arrows every 2 m";

    AcGePoint3d legendPt(workspaceW + 2.0, workspaceH, 0.0);

    AcDbMText* mtext = new AcDbMText();
    mtext->setLocation(legendPt);
    mtext->setContents(legendText.c_str());
    mtext->setTextHeight(0.5);
    mtext->setWidth(20.0);
    mtext->setAttachment(AcDbMText::kTopLeft);
    mtext->setColorIndex(7);
    appendAndClose(mtext);
}
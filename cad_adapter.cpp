#include "cad_adapter.h"
#include <cmath>
#include <sstream>
#include <iomanip>

// ObjectARX headers (from provided library list)
#include "acdb.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "acedads.h"
#include "acgedefs.h"

namespace ModulusLite {

// ============================================================================
// CAD ADAPTER IMPLEMENTATION
// ============================================================================

bool CADAdapter::initialize() {
    if (!m_pDb) return false;
    return true;
}

bool CADAdapter::drawMachine(const Machine& machine) {
    if (!m_pDb) return false;
    
    // Get model space
    AcDbBlockTable* pBlkTable;
    if (acdbOpenObject(pBlkTable, m_pDb->blockTableId(), AcDb::kForRead) != Acad::eOk) {
        return false;
    }
    
    AcDbBlockTableRecord* pMs;
    if (acdbOpenObject(pMs, pBlkTable->modelSpaceId(), AcDb::kForWrite) != Acad::eOk) {
        pBlkTable->close();
        return false;
    }
    
    // Create block reference
    AcDbBlockReference* pRef = new AcDbBlockReference();
    
    // Set position
    AcGePoint3d pos(machine.position.x, machine.position.y, machine.position.z);
    pRef->setPosition(pos);
    
    // Try to set the block definition from library
    // machine.modelPath contains the block name (e.g., "PUMP_CENTRIFUGAL")
    if (!machine.modelPath.empty()) {
        AcDbObjectId blockId;
        Acad::ErrorStatus es = pBlkTable->getAt(machine.modelPath.c_str(), blockId);
        
        if (es == Acad::eOk) {
            // Block exists in DWG, reference it
            pRef->setBlockTableRecord(blockId);
        } else {
            // Block not found in DWG, insert from external file
            // Try to load from library storage
            std::string blockPath = getBlockFromLibrary(machine.modelPath);
            if (!blockPath.empty() && insertBlockFromFile(blockPath, machine.modelPath, pBlkTable)) {
                pBlkTable->getAt(machine.modelPath.c_str(), blockId);
                pRef->setBlockTableRecord(blockId);
            } else {
                // Fallback: use a placeholder block
                acutPrintf(_T("WARNING: Block %s not found; using placeholder\n"), 
                    machine.modelPath.c_str());
                pBlkTable->getAt("PLACEHOLDER_MACHINE", blockId);
                pRef->setBlockTableRecord(blockId);
            }
        }
    }
    
    // Set scale
    AcGeScale3d scale(1.0, 1.0, 1.0);
    pRef->setScaleFactors(scale);
    
    // Add to model space
    AcDbObjectId refId;
    Acad::ErrorStatus es = pMs->appendAcDbEntity(refId, pRef);
    
    pMs->close();
    pBlkTable->close();
    pRef->close();
    
    return (es == Acad::eOk);
}

bool CADAdapter::drawPipe(const Pipe& pipe) {
    if (!m_pDb || pipe.path.empty()) return false;
    
    // Get model space
    AcDbBlockTable* pBlkTable;
    if (acdbOpenObject(pBlkTable, m_pDb->blockTableId(), AcDb::kForRead) != Acad::eOk) {
        return false;
    }
    
    AcDbBlockTableRecord* pMs;
    if (acdbOpenObject(pMs, pBlkTable->modelSpaceId(), AcDb::kForWrite) != Acad::eOk) {
        pBlkTable->close();
        return false;
    }
    
    // Create 3D polyline
    AcDb3dPolyline* pPoly = new AcDb3dPolyline(AcDb::k3dSimplePoly);
    
    // Add vertices
    for (const auto& pt : pipe.path) {
        AcGePoint3d acPt(pt.x, pt.y, pt.z);
        AcDb3dPolylineVertex* pVertex = new AcDb3dPolylineVertex(acPt);
        pPoly->appendVertex(pVertex);
        pVertex->close();
    }
    
    // Set color by material
    colorByMaterial(pPoly, pipe.material);
    
    // Set lineweight by diameter
    setLineweightByDiameter(pPoly, pipe.diameter);
    
    // Add to model space
    AcDbObjectId polyId;
    Acad::ErrorStatus es = pMs->appendAcDbEntity(polyId, pPoly);
    
    pMs->close();
    pBlkTable->close();
    pPoly->close();
    
    return (es == Acad::eOk);
}

bool CADAdapter::drawFlowArrow(const Point3D& position, const Point3D& direction) {
    if (!m_pDb) return false;
    
    // Normalize direction
    Point3D dir = direction.normalize();
    
    // Get model space
    AcDbBlockTable* pBlkTable;
    if (acdbOpenObject(pBlkTable, m_pDb->blockTableId(), AcDb::kForRead) != Acad::eOk) {
        return false;
    }
    
    AcDbBlockTableRecord* pMs;
    if (acdbOpenObject(pMs, pBlkTable->modelSpaceId(), AcDb::kForWrite) != Acad::eOk) {
        pBlkTable->close();
        return false;
    }
    
    // Create arrow block reference
    AcDbBlockReference* pArrow = new AcDbBlockReference();
    
    AcGePoint3d arrowPos(position.x, position.y, position.z);
    pArrow->setPosition(arrowPos);
    
    // Rotate arrow to face direction
    double angle = std::atan2(dir.y, dir.x);
    pArrow->setRotation(angle);
    
    AcDbObjectId arrowId;
    Acad::ErrorStatus es = pMs->appendAcDbEntity(arrowId, pArrow);
    
    pMs->close();
    pBlkTable->close();
    
    return (es == Acad::eOk);
}

bool CADAdapter::drawPipeTag(const Pipe& pipe) {
    if (!m_pDb || pipe.path.empty()) return false;
    
    // Find midpoint of path
    double totalDist = 0.0;
    for (size_t i = 0; i + 1 < pipe.path.size(); ++i) {
        totalDist += pipe.path[i].distance(pipe.path[i+1]);
    }
    
    double targetDist = totalDist / 2.0;
    double currentDist = 0.0;
    
    Point3D midpoint = pipe.path[0];
    for (size_t i = 0; i + 1 < pipe.path.size(); ++i) {
        double segLen = pipe.path[i].distance(pipe.path[i+1]);
        if (currentDist + segLen >= targetDist) {
            double t = (segLen > 0) ? (targetDist - currentDist) / segLen : 0.0;
            midpoint = pipe.path[i] + (pipe.path[i+1] - pipe.path[i]) * t;
            break;
        }
        currentDist += segLen;
    }
    
    // Get model space
    AcDbBlockTable* pBlkTable;
    if (acdbOpenObject(pBlkTable, m_pDb->blockTableId(), AcDb::kForRead) != Acad::eOk) {
        return false;
    }
    
    AcDbBlockTableRecord* pMs;
    if (acdbOpenObject(pMs, pBlkTable->modelSpaceId(), AcDb::kForWrite) != Acad::eOk) {
        pBlkTable->close();
        return false;
    }
    
    // Create MText for tag
    AcDbMText* pMText = new AcDbMText();
    
    AcGePoint3d tagPos(midpoint.x, midpoint.y, midpoint.z + 0.3);
    pMText->setLocation(tagPos);
    pMText->setTextHeight(0.25);
    pMText->setContents(pipe.tag.c_str());
    
    AcDbObjectId textId;
    Acad::ErrorStatus es = pMs->appendAcDbEntity(textId, pMText);
    
    pMs->close();
    pBlkTable->close();
    pMText->close();
    
    return (es == Acad::eOk);
}

bool CADAdapter::drawMachineLabel(const Machine& machine) {
    if (!m_pDb) return false;
    
    // Create label string
    std::ostringstream label;
    label << machine.name << " (" << machine.id << ")";
    
    // Get model space
    AcDbBlockTable* pBlkTable;
    if (acdbOpenObject(pBlkTable, m_pDb->blockTableId(), AcDb::kForRead) != Acad::eOk) {
        return false;
    }
    
    AcDbBlockTableRecord* pMs;
    if (acdbOpenObject(pMs, pBlkTable->modelSpaceId(), AcDb::kForWrite) != Acad::eOk) {
        pBlkTable->close();
        return false;
    }
    
    // Create MText label above machine
    AcDbMText* pLabel = new AcDbMText();
    
    Point3D labelPos = machine.position + Point3D(0, 0, machine.size.z / 2 + 0.5);
    AcGePoint3d pos(labelPos.x, labelPos.y, labelPos.z);
    pLabel->setLocation(pos);
    pLabel->setTextHeight(0.3);
    pLabel->setContents(label.str().c_str());
    
    AcDbObjectId labelId;
    Acad::ErrorStatus es = pMs->appendAcDbEntity(labelId, pLabel);
    
    pMs->close();
    pBlkTable->close();
    pLabel->close();
    
    return (es == Acad::eOk);
}

bool CADAdapter::drawLegend() {
    if (!m_pDb) return false;
    
    // Get model space
    AcDbBlockTable* pBlkTable;
    if (acdbOpenObject(pBlkTable, m_pDb->blockTableId(), AcDb::kForRead) != Acad::eOk) {
        return false;
    }
    
    AcDbBlockTableRecord* pMs;
    if (acdbOpenObject(pMs, pBlkTable->modelSpaceId(), AcDb::kForWrite) != Acad::eOk) {
        pBlkTable->close();
        return false;
    }
    
    // Create legend text
    std::string legendText = 
        "P&ID TAG FORMAT: [Line]-[Size]-[Material]-[Service]-[Spec]\n"
        "\nMaterial Codes: L=Liquid, G=Gas, S=Solid, SL=Sludge\n"
        "Service Codes: W=Water, AIR=Air, SLURRY=Slurry, CHEM=Chemical\n"
        "Spec Codes: CS150, SS316, PVC, HDPE\n"
        "Flow Direction: Indicated by arrows\n"
        "\nColor Legend:\n"
        "Blue=Liquid, Yellow=Gas, Brown=Solid, Gray=Sludge";
    
    // Create MText legend
    AcDbMText* pLegend = new AcDbMText();
    
    AcGePoint3d legendPos(1.0, 1.0, 0.0);
    pLegend->setLocation(legendPos);
    pLegend->setTextHeight(0.2);
    pLegend->setContents(legendText.c_str());
    
    AcDbObjectId legendId;
    Acad::ErrorStatus es = pMs->appendAcDbEntity(legendId, pLegend);
    
    pMs->close();
    pBlkTable->close();
    pLegend->close();
    
    return (es == Acad::eOk);
}

void CADAdapter::colorByMaterial(AcDbEntity* pEnt, MaterialType material) {
    if (!pEnt) return;
    
    int colorIndex = getColorIndex(material);
    AcCmColor color;
    color.setColorIndex(colorIndex);
    pEnt->setColor(color);
}

int CADAdapter::getColorIndex(MaterialType material) {
    switch (material) {
        case MaterialType::Liquid: return 5;  // Cyan/Blue
        case MaterialType::Gas:    return 2;  // Yellow
        case MaterialType::Solid:  return 30; // Brown (true color)
        case MaterialType::Sludge: return 8;  // Gray
        default: return 256;  // Bylayer
    }
}

void CADAdapter::setLineweightByDiameter(AcDbEntity* pEnt, double diameter) {
    if (!pEnt) return;
    
    // Map diameter to lineweight (in 0.01 mm units)
    AcDb::LineWeight lw;
    if (diameter < 0.1) {
        lw = AcDb::kLnWt013;  // 0.13 mm
    } else if (diameter < 0.2) {
        lw = AcDb::kLnWt025;  // 0.25 mm
    } else if (diameter < 0.3) {
        lw = AcDb::kLnWt035;  // 0.35 mm
    } else {
        lw = AcDb::kLnWt050;  // 0.50 mm
    }
    
    pEnt->setLineWeight(lw);
}

// ============================================================================
// PIPELINE IMPLEMENTATION
// ============================================================================

bool Pipeline::execute(std::vector<Machine>& machines, std::vector<Pipe>& pipes) {
    // Step 1: Layout
    if (!executeLayout(machines)) {
        return false;
    }
    
    // Step 2: Route pipes
    if (!executeRouting(machines, pipes)) {
        return false;
    }
    
    // Step 3: Draw to CAD
    if (!executeDrawing(machines, pipes)) {
        return false;
    }
    
    return true;
}

bool Pipeline::executeLayout(std::vector<Machine>& machines) {
    if (machines.empty()) return false;
    
    FDGLayout::layout(machines);
    return true;
}

bool Pipeline::executeRouting(std::vector<Machine>& machines, std::vector<Pipe>& pipes) {
    for (auto& pipe : pipes) {
        // Find from and to machines
        Machine* pFrom = nullptr;
        Machine* pTo = nullptr;
        
        for (auto& m : machines) {
            if (m.id == pipe.fromMachineId) pFrom = &m;
            if (m.id == pipe.toMachineId) pTo = &m;
        }
        
        if (!pFrom || !pTo) continue;
        
        // Get port positions
        Point3D start = pFrom->ports[std::min(pipe.fromPortIndex, 
                                              (int)pFrom->ports.size() - 1)];
        Point3D goal = pTo->ports[std::min(pipe.toPortIndex, 
                                           (int)pTo->ports.size() - 1)];
        
        // Route using A*
        auto result = ManhattanRouter::route(start, goal, machines, pipes);
        if (!result.success) {
            // Fallback: straight line
            result.path.clear();
            result.path.push_back(start);
            result.path.push_back(goal);
        }
        
        pipe.path = result.path;
        
        // Compute properties
        PipeProperties::computeLength(pipe);
        PipeProperties::computeCost(pipe);
        PipeProperties::generateTag(pipe);
    }
    
    return true;
}

bool Pipeline::executeDrawing(const std::vector<Machine>& machines, 
                              const std::vector<Pipe>& pipes) {
    // Draw machines
    for (const auto& machine : machines) {
        m_adapter.drawMachine(machine);
        m_adapter.drawMachineLabel(machine);
    }
    
    // Draw pipes
    for (const auto& pipe : pipes) {
        m_adapter.drawPipe(pipe);
        
        // Add flow arrows every ~2m
        if (pipe.path.size() > 1) {
            double totalLen = 0.0;
            for (size_t i = 0; i + 1 < pipe.path.size(); ++i) {
                totalLen += pipe.path[i].distance(pipe.path[i+1]);
            }
            
            double arrowSpacing = 2.0;
            double currentLen = arrowSpacing;
            double accum = 0.0;
            
            for (size_t i = 0; i + 1 < pipe.path.size(); ++i) {
                double segLen = pipe.path[i].distance(pipe.path[i+1]);
                while (accum + segLen >= currentLen) {
                    double t = (segLen > 0) ? (currentLen - accum) / segLen : 0.0;
                    Point3D arrowPos = pipe.path[i] + (pipe.path[i+1] - pipe.path[i]) * t;
                    Point3D arrowDir = (pipe.path[i+1] - pipe.path[i]).normalize();
                    m_adapter.drawFlowArrow(arrowPos, arrowDir);
                    currentLen += arrowSpacing;
                }
                accum += segLen;
            }
        }
        
        m_adapter.drawPipeTag(pipe);
    }
    
    // Draw legend
    m_adapter.drawLegend();
    
    return true;
}

}  // namespace ModulusLite

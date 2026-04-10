#pragma once

#include "core.h"
#include <vector>
#include <string>

// Forward declarations for ObjectARX types
class AcDbDatabase;
class AcDbBlockTable;
class AcDbBlockTableRecord;
class AcDbPolyline;
class AcDb3dPolyline;
class AcDbMText;
class AcDbBlockReference;
class AcDbLinetype;
class AcDbArrow;

namespace ModulusLite {

// ============================================================================
// CAD ADAPTER - ObjectARX Interface
// ============================================================================

class CADAdapter {
public:
    CADAdapter(AcDbDatabase* pDb) : m_pDb(pDb) {}
    
    // Initialize CAD database
    bool initialize();
    
    // Draw a machine as a block reference
    bool drawMachine(const Machine& machine);
    
    // Draw a pipe as a 3D polyline with color coding
    bool drawPipe(const Pipe& pipe);
    
    // Draw flow arrow at position along pipe direction
    bool drawFlowArrow(const Point3D& position, const Point3D& direction);
    
    // Draw pipe tag (P&ID label) at midpoint
    bool drawPipeTag(const Pipe& pipe);
    
    // Draw machine label (name + ID)
    bool drawMachineLabel(const Machine& machine);
    
    // Draw legend block
    bool drawLegend();
    
    // Set entity color by material type
    void colorByMaterial(AcDbEntity* pEnt, MaterialType material);
    
    // Get color index (1-7, or 256 for true color)
    int getColorIndex(MaterialType material);
    
    // Set lineweight by diameter
    void setLineweightByDiameter(AcDbEntity* pEnt, double diameter);
    
private:
    AcDbDatabase* m_pDb;
    
    // Helper: get or create block
    AcDbBlockTableRecord* getOrCreateBlock(const std::string& blockName);
    
    // Helper: add entity to model space
    bool addEntityToModelSpace(AcDbEntity* pEnt);
    
    // Helper: create 3D polyline from path
    AcDb3dPolyline* createPolylineFromPath(const std::vector<Point3D>& path);
    
    // Helper: create MText annotation
    AcDbMText* createMText(const Point3D& position, const std::string& text, 
                          double height = 0.25, double rotation = 0.0);
};

// ============================================================================
// EXECUTION PIPELINE
// ============================================================================

class Pipeline {
public:
    Pipeline(AcDbDatabase* pDb) : m_adapter(pDb) {}
    
    // Main execution: layout -> route -> draw
    bool execute(std::vector<Machine>& machines, std::vector<Pipe>& pipes);
    
private:
    CADAdapter m_adapter;
    
    bool executeLayout(std::vector<Machine>& machines);
    bool executeRouting(std::vector<Machine>& machines, std::vector<Pipe>& pipes);
    bool executeDrawing(const std::vector<Machine>& machines, 
                       const std::vector<Pipe>& pipes);
};

}  // namespace ModulusLite

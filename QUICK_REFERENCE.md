# Modulus Lite — Developer Quick Reference

## Files Overview

| File | Lines | Purpose |
|------|-------|---------|
| `core.h` | ~500 | Pure C++ algorithms (FDG, A*, data structures) |
| `cad_adapter.h` | ~60 | CAD adapter interface |
| `cad_adapter.cpp` | ~350 | ObjectARX integration (drawing) |
| `ui_panel.h` | ~50 | Qt UI panel interface |
| `ui_panel.cpp` | ~200 | Qt UI implementation |
| `plugin_main.cpp` | ~200 | AutoCAD entry point & commands |
| `CMakeLists.txt` | ~120 | Build configuration |
| `tests/test_core.cpp` | ~350 | Unit tests (geometry, FDG, A*) |
| `tests/test_routing.cpp` | ~300 | Integration tests |
| **Total** | **~2,500** | Complete MVP plugin |

## Code Patterns

### Creating a Pipe
```cpp
Pipe pipe;
pipe.id = 0;
pipe.fromMachineId = 0;
pipe.toMachineId = 1;
pipe.material = MaterialType::Liquid;
pipe.service = ServiceType::Water;
pipe.diameter = PipeProperties::getDiameter(MaterialType::Liquid);
pipe.specCode = "CS150";
pipe.lineNumber = 100;

// Route it
auto result = ManhattanRouter::route(startPos, goalPos, machines, pipes);
pipe.path = result.success ? result.path : std::vector<Point3D>{startPos, goalPos};

// Compute properties
PipeProperties::computeLength(pipe);
PipeProperties::computeCost(pipe);
PipeProperties::generateTag(pipe);
```

### Drawing to AutoCAD
```cpp
CADAdapter adapter(pDb);

// Draw a 3D polyline
adapter.drawPipe(pipe);

// Draw a label
adapter.drawPipeTag(pipe);

// Style it
adapter.colorByMaterial(pEnt, pipe.material);
adapter.setLineweightByDiameter(pEnt, pipe.diameter);
```

### Running the Pipeline
```cpp
std::vector<Machine> machines;  // Populate
std::vector<Pipe> pipes;        // Populate

Pipeline pipeline(pDb);
if (pipeline.execute(machines, pipes)) {
    // Success: layout, routing, drawing complete
    acutPrintf(_T("Pipeline succeeded.\n"));
} else {
    acutPrintf(_T("Pipeline failed.\n"));
}
```

## Constants & Tuning

### FDG Layout
```cpp
class FDGLayout {
    static constexpr double K = 5.0;              // Spring constant
    static constexpr double REPULSION = 50.0;     // Repulsion strength
    static constexpr int ITERATIONS = 100;        // Convergence iterations
    static constexpr double DAMPING = 0.9;        // Velocity damping
    static constexpr double WORKSPACE_X = 20.0;   // Bounds (meters)
    static constexpr double WORKSPACE_Y = 15.0;
    static constexpr double WORKSPACE_Z = 5.0;
};
```

**Tuning:**
- More overlap? ↑ `REPULSION` or ↓ `DAMPING`
- Oscillation? ↑ `DAMPING` (0.85–0.95)
- Won't converge? ↑ `ITERATIONS` (100–200)

### A* Router
```cpp
class ManhattanRouter {
    static constexpr double GRID_RESOLUTION = 0.5;   // 0.5m cells
    static constexpr double MACHINE_BUFFER = 1.0;    // Safety margin
    static constexpr double PIPE_PENALTY = 5.0;      // Soft cost
};
```

**Tuning:**
- Too slow? ↑ `GRID_RESOLUTION` (1.0m)
- Pipes colliding? ↑ `PIPE_PENALTY` (10.0)
- Not finding paths? ↓ `MACHINE_BUFFER` (0.5)

### Pipe Properties
```cpp
// Diameters (meters)
Gas    → 0.15
Liquid → 0.10
Solid  → 0.25
Sludge → 0.30

// Material costs ($/meter)
Gas    → 120
Liquid → 100
Solid  → 180
Sludge → 220

// Cost formula
Cost = Length × MaterialCost × Diameter
```

## Build Checklist

- [ ] ObjectARX SDK installed (`/path/to/ObjectARX2027Mac`)
- [ ] Qt5 installed (`brew install qt@5`)
- [ ] CMake installed (`brew install cmake`)
- [ ] Create build directory: `mkdir build && cd build`
- [ ] Run CMake: `cmake .. -DOBJECTARX_SDK_PATH=... -DQt5_DIR=...`
- [ ] Build: `cmake --build . --config Release`
- [ ] Run tests: `ctest`
- [ ] Install: `cmake --install .`
- [ ] Load in AutoCAD: `APPLOAD` → ModulusLite.bundle

## Command Reference

| Command | Function | Time |
|---------|----------|------|
| `MODLAYOUT` | Run FDG layout | ~100ms |
| `MODROUTE` | Run A* + drawing | ~300ms |

## Debugging Tips

### Print Debug Info
```cpp
// In plugin_main.cpp
acutPrintf(_T("Machine %d at (%.2f, %.2f, %.2f)\n"), 
    m.id, m.position.x, m.position.y, m.position.z);
```

### Verify A* Result
```cpp
auto result = ManhattanRouter::route(start, goal, machines, pipes);
if (!result.success) {
    acutPrintf(_T("WARNING: No path found from (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)\n"),
        start.x, start.y, start.z, goal.x, goal.y, goal.z);
}
acutPrintf(_T("Path length: %zu nodes\n"), result.path.size());
```

### Check Pipe Cost
```cpp
PipeProperties::computeLength(pipe);
PipeProperties::computeCost(pipe);
acutPrintf(_T("Pipe %d: %.2fm @ $%.2f\n"), pipe.id, pipe.length, pipe.cost);
```

### Inspect CAD Entity
```cpp
// After drawPipe()
acutPrintf(_T("Polyline ID: %d, Color: %d, Lineweight: %d\n"),
    polyId, pEnt->colorIndex(), pEnt->lineWeight());
```

## Common Errors & Fixes

### Compilation

```
error: 'AcDb3dPolyline' was not declared
→ Add #include "dbents.h" to cad_adapter.cpp

error: undefined reference to '_AcRxRegisterApp'
→ Link ObjectARX libraries in CMakeLists.txt

error: framework not found Cocoa
→ Add "-framework Cocoa" to target_link_libraries
```

### Runtime

```
Assertion failed: distance > 0.01
→ Start and goal points too close; add check:
  if (start.distance(goal) < 0.1) return straight line;

openSet empty, no path found
→ Goal unreachable; check:
  - Machine not blocking goal
  - Grid resolution not too fine
  - Workspace bounds adequate

Machines overlapping after layout
→ Increase REPULSION; decrease DAMPING
```

## Testing & Validation

### Unit Test All Modules
```bash
cd build
ctest --verbose
```

### Test Single Feature
```bash
# Run only core tests
./ModulusLiteTests | grep "test_fdg"

# Run only routing tests
./ModulusLiteTests | grep "test_astar"
```

### Verify Build Artifacts
```bash
# Check library links
otool -L ModulusLite.bundle

# Check ObjectARX symbols
nm -g /path/to/ObjectARX2027Mac/lib/libAcCore.a | head -20

# Verify Qt framework
ls -la /usr/local/opt/qt@5/lib/
```

## Performance Profiling

### Time Individual Operations
```cpp
#include <chrono>

auto t0 = std::chrono::high_resolution_clock::now();
FDGLayout::layout(machines);
auto t1 = std::chrono::high_resolution_clock::now();
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
acutPrintf(_T("FDG took %lld ms\n"), ms);
```

### Profile A* Cost
```cpp
// Count iterations in ManhattanRouter::route()
int iterations = 0;
while (!openSet.empty() && --maxIter > 0) {
    iterations++;
    // ...
}
acutPrintf(_T("A* used %d / 10000 iterations\n"), iterations);
```

### CAD Drawing Stats
```cpp
int entityCount = 0;
for (const auto& m : machines) {
    adapter.drawMachine(m);        entityCount++;
    adapter.drawMachineLabel(m);   entityCount++;
}
for (const auto& p : pipes) {
    adapter.drawPipe(p);           entityCount++;
    adapter.drawPipeTag(p);        entityCount++;
}
adapter.drawLegend();              entityCount++;
acutPrintf(_T("Created %d entities\n"), entityCount);
```

## Qt UI Customization

### Add a New Button
```cpp
// ui_panel.cpp, in setupUI()
QPushButton* pBtnNew = new QPushButton("Export CSV", this);
pBtnLayout->addWidget(pBtnNew);

connect(pBtnNew, &QPushButton::clicked, this, &UIPanel::onExportCSV);

// Add slot
void UIPanel::onExportCSV() {
    // Implementation
}
```

### Change Button Icons
```cpp
// Requires Qt resource file (.qrc) with images
m_pBtnLayout->setIcon(QIcon(":/icons/layout.png"));
m_pBtnRoute->setIcon(QIcon(":/icons/route.png"));
```

### Add Pipe Color Display
```cpp
// In updateInfo()
QColor pipeColor = (pipe.material == MaterialType::Liquid) 
    ? QColor(0, 255, 255) 
    : QColor(255, 255, 0);
m_pInfoLabel->setStyleSheet(
    QString("background-color: rgb(%1,%2,%3);")
        .arg(pipeColor.red())
        .arg(pipeColor.green())
        .arg(pipeColor.blue())
);
```

## Extending the Plugin

### Add New Service Type
1. Extend `ServiceType` enum
2. Add code in `PipeProperties::getServiceCode()`
3. Update tag generation
4. Update P&ID legend in `drawLegend()`
5. Add test case

### Support Different Grid Resolutions
```cpp
// Make configurable
ManhattanRouter::route(start, goal, machines, pipes, gridResolution)
```

### Implement Pipe Collision Warnings
```cpp
bool checkPipeCollision(const Pipe& p1, const Pipe& p2) {
    for (const auto& pt1 : p1.path) {
        for (const auto& pt2 : p2.path) {
            if (pt1.distance(pt2) < 0.1) return true;  // Collision
        }
    }
    return false;
}
```

### Add Cost Breakdown Report
```cpp
void printCostReport(const std::vector<Pipe>& pipes) {
    double totalCost = 0;
    for (const auto& p : pipes) {
        totalCost += p.cost;
        acutPrintf(_T("Pipe %d: $%.2f (%s, %.2fm)\n"), 
            p.id, p.cost, p.tag.c_str(), p.length);
    }
    acutPrintf(_T("TOTAL: $%.2f\n"), totalCost);
}
```

## Deployment

### Create Installer
```bash
# CMake handles installation
cmake --install . --prefix /Volumes/ModulusLite

# Create DMG
hdiutil create -volname ModulusLite -srcfolder /Volumes/ModulusLite \
    -ov -format UDZO ModulusLite.dmg
```

### Version Numbering
```cpp
// plugin_main.cpp
#define MODULUSLITE_VERSION "1.0.0"
acutPrintf(_T("\nModulus Lite v%s loaded.\n"), _T(MODULUSLITE_VERSION));
```

### Update Check
```cpp
// Could add HTTP call to check for new version
bool checkForUpdates() {
    // fetch https://example.com/modulus-lite/latest.txt
    // compare version strings
    return newerAvailable;
}
```

## Support Resources

| Resource | URL |
|----------|-----|
| ObjectARX Docs | https://www.autodesk.com/developer/autocad |
| Qt Documentation | https://doc.qt.io/qt-5/ |
| CMake Guide | https://cmake.org/cmake/help/latest/ |
| P&ID Standards | ASME Y14.48M, ISA SP5.1 |

## Version History

| Version | Date | Notes |
|---------|------|-------|
| 1.0 | 2024 | Initial MVP: 5 machines, 6 pipes, FDG + A* |
| 1.1 | TBD | Worker thread for 10+ pipes |
| 1.2 | TBD | CSV import/export |
| 2.0 | TBD | Full P&ID editor, thermal analysis |

---

**Last Updated:** 2024  
**Contact:** [Your email]  
**Issues:** [GitHub issues or support link]

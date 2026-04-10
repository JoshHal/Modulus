# Modulus Lite Implementation Guide

## Quick Start

### 1. Project Structure
```
ModulusLite/
├── CMakeLists.txt              # Build config
├── core.h                       # Pure C++ algorithms
├── cad_adapter.h / .cpp         # ObjectARX integration
├── ui_panel.h / .cpp            # Qt UI
├── plugin_main.cpp              # Entry point
├── tests/
│   ├── test_core.cpp
│   └── test_routing.cpp
├── README.md
└── LICENSE
```

### 2. Build Process

```
User runs CMake
     ↓
CMake detects:
  - ObjectARX SDK (inc + lib paths)
  - Qt5 (Core, Gui, Widgets)
  - Clang/Apple compiler
     ↓
Generates Xcode project
     ↓
CMake builds 4 targets:
  1. ModulusLiteCore (header-only / static)
  2. ModulusLiteCAD (static library, links ObjectARX)
  3. ModulusLiteUI (static library, links Qt)
  4. ModulusLitePlugin (dylib/bundle, links all 3)
     ↓
Final artifact: ModulusLite.bundle
(installs to ~/Library/Application Support/Autodesk/ApplicationPlugins/)
```

### 3. Runtime Flow

```
AutoCAD starts
     ↓
Loads ModulusLite.bundle
     ↓
acrxEntryPoint(kInitAppMsg) called
  - Registers MODLAYOUT and MODROUTE commands
  - Creates ModulusLitePlugin singleton
  - Initializes sample machines/pipes
  - (Optional) Creates Qt UI panel
     ↓
User types "MODLAYOUT"
  ↓
cmdGenerateLayout() executes
  - Calls FDGLayout::layout(machines)
  - Machines reposition based on forces
     ↓
User types "MODROUTE"
  ↓
cmdRoutePipes() executes
  - Pipeline::execute() orchestrates:
    1. Run A* for each pipe
    2. Compute length/cost/tag
    3. Draw to AutoCAD via CADAdapter
  - Displays pipe info in console
     ↓
User sees 3D model with machines, pipes, tags, legend
```

## Execution Pipeline Detail

### Step 1: Layout Generation (FDGLayout)

```cpp
FDGLayout::layout(machines);  // ~100ms for 5 machines
```

**Each iteration:**
1. Zero out force vectors
2. For each machine pair:
   - Compute repulsion: `F_r = k² / d`
3. For each machine:
   - Compute center attraction
4. Update velocity: `v' = (v + F) × damping`
5. Update position: `p' = p + v`
6. Clamp to bounds [0, 20] × [0, 15] × [0, 5]

**Result:** Evenly spaced non-overlapping machines.

### Step 2: Pipe Routing (ManhattanRouter)

For each pipe (sequential):

```cpp
// Get start & goal ports
Point3D start = machines[fromId].ports[fromPort];
Point3D goal = machines[toId].ports[toPort];

// Run A*
auto result = ManhattanRouter::route(start, goal, machines, pipes);

// Fallback if no path
if (!result.success) {
    result.path = {start, goal};  // Straight line
}

pipe.path = result.path;
```

**A* Implementation:**
- Convert 3D coordinates to grid: `x / 0.5, y / 0.5, z / 0.5`
- Priority queue: sorted by `f = g + h`
- Neighbor generation: 6 moves (±x, ±y, ±z)
- Heuristic: Manhattan distance `|dx| + |dy| + |dz|`
- Cost function:
  - Machine cell: 1e6 (impassable)
  - Pipe cell: +5.0 (soft penalty)
  - Default: 1.0
- Max iterations: 10,000
- Grid resolution: 0.5m

**Result:** Path avoids machines; soft-penalizes existing pipes.

### Step 3: Compute Pipe Properties

```cpp
PipeProperties::computeLength(pipe);
// Sum segment distances: |p1-p2| + |p2-p3| + ...

PipeProperties::computeCost(pipe);
// Cost = length × materialCost(material) × diameter

PipeProperties::generateTag(pipe);
// Tag = "[lineNum]-[diameterMm]-[matCode]-[svcCode]-[specCode]"
// Example: "100-100-L-W-CS150"
```

### Step 4: Draw to AutoCAD

```cpp
// For each machine
for (const auto& m : machines) {
    adapter.drawMachine(m);           // AcDbBlockReference
    adapter.drawMachineLabel(m);      // AcDbMText
}

// For each pipe
for (const auto& p : pipes) {
    adapter.drawPipe(p);              // AcDb3dPolyline
    
    // Add flow arrows every 2m
    for (each 2m segment) {
        adapter.drawFlowArrow(pos, direction);  // AcDbBlockReference
    }
    
    adapter.drawPipeTag(p);           // AcDbMText at midpoint
}

// Legend
adapter.drawLegend();                 // AcDbMText with info
```

## Key Algorithms

### FDG Layout

**Physics Model:**
```
F_repulsion = k² / d²       (between each pair)
F_attraction = d² / k       (toward center, weak)

Total force per machine:
F_i = Σ(repulsion from all j) + center_attraction

Velocity update (with damping):
v'_i = (v_i + F_i) × damping

Position update:
p'_i = p_i + v'_i

Clamping:
p'_i.x = clamp(p'_i.x, 0.5, 19.5)
```

**Convergence:** 100 iterations typically equilibrates. Machines spread evenly with damping preventing oscillation.

### Manhattan A*

**Core loop:**
```
while (openSet not empty):
    current = openSet.pop_min(f_score)
    
    if current == goal:
        return reconstruct_path()
    
    for each neighbor of current:
        tentative_g = g_score[current] + cost(neighbor)
        
        if tentative_g < g_score[neighbor]:
            cameFrom[neighbor] = current
            g_score[neighbor] = tentative_g
            f_score[neighbor] = g + heuristic(neighbor, goal)
            openSet.push(neighbor)
```

**Cost Model:**
```
cost(node) = {
    1e6           if machine_collision(node),
    1.0 + 5.0×n   if within n pipes,
    1.0           otherwise
}
```

### Pipe Tag Generation

**Format:** `[Line]-[Size]-[Material]-[Service]-[Spec]`

Example values:
| Field | Example | Derivation |
|-------|---------|-----------|
| Line | 100 | `100 * (pipeId + 1)` |
| Size | 100 | `diameter * 1000` (mm) |
| Material | L | Liquid → "L", Gas → "G", Solid → "S", Sludge → "SL" |
| Service | W | Water → "W", Air → "AIR", Slurry → "SLURRY", Chemical → "CHEM" |
| Spec | CS150 | From `pipe.specCode` |

## ObjectARX Integration Points

### CADAdapter Interface

```cpp
class CADAdapter {
    // Database operations
    bool addEntityToModelSpace(AcDbEntity* pEnt);
    
    // Create entities
    AcDb3dPolyline* createPolylineFromPath(const std::vector<Point3D>& path);
    AcDbMText* createMText(const Point3D& pos, const std::string& text, ...);
    
    // Styling
    void colorByMaterial(AcDbEntity* pEnt, MaterialType mat);
    void setLineweightByDiameter(AcDbEntity* pEnt, double diameter);
};
```

### Entity Creation

**3D Polyline (Pipe):**
```cpp
AcDb3dPolyline* pPoly = new AcDb3dPolyline(AcDb::k3dSimplePoly);
for (const auto& pt : path) {
    AcDb3dPolylineVertex* pVtx = new AcDb3dPolylineVertex(
        AcGePoint3d(pt.x, pt.y, pt.z)
    );
    pPoly->appendVertex(pVtx);
    pVtx->close();
}
pMs->appendAcDbEntity(polyId, pPoly);
```

**MText (Label):**
```cpp
AcDbMText* pText = new AcDbMText();
pText->setLocation(AcGePoint3d(x, y, z));
pText->setTextHeight(0.25);
pText->setContents("100-100-L-W-CS150");
pMs->appendAcDbEntity(textId, pText);
```

**Block Reference (Machine):**
```cpp
AcDbBlockReference* pRef = new AcDbBlockReference();
pRef->setPosition(AcGePoint3d(x, y, z));
pMs->appendAcDbEntity(refId, pRef);
```

### Color Mapping

| Material | ACI | RGB | Color |
|----------|-----|-----|-------|
| Liquid | 5 | (0, 255, 255) | Cyan |
| Gas | 2 | (255, 255, 0) | Yellow |
| Solid | 30 | (165, 42, 42) | Brown |
| Sludge | 8 | (128, 128, 128) | Gray |

```cpp
AcCmColor color;
color.setColorIndex(colorIndex);
pEnt->setColor(color);
```

## UI Panel Details

### Qt Layout
```
┌─ Modulus Lite ───────────┐
│ ┌─ Controls ────────────┐ │
│ │ [Generate Layout] [ ] │ │  ← Buttons
│ │ [Route Pipes]    [ ] │ │
│ │ Status: Ready         │ │
│ ├────────────────────────┤ │
│ │ Pipes:                 │ │
│ │ ☑ Pipe 0 (100-100-L-W) │ │  ← List (click to select)
│ │ ☐ Pipe 1 (200-150-G-A) │ │
│ │ ☐ Pipe 2 (300-300-SL-S)│ │
│ ├────────────────────────┤ │
│ │ Pipe 0 Details:        │ │  ← Info panel
│ │ Tag: 100-100-L-W-CS150 │ │
│ │ Length: 12.34 m        │ │
│ │ Cost: $1234.00         │ │
│ │ Spec: CS150            │ │
│ └────────────────────────┘ │
└──────────────────────────────┘
```

### Qt Signal Flow
```
User clicks "Generate Layout"
            ↓
    m_pBtnLayout::clicked
            ↓
    UIPanel::onGenerateLayout()
            ↓
    m_pPipeline->execute()  [in FDG mode only]
            ↓
    updateStatus("Layout generated...")
```

## Determinism Guarantee

**No randomness after initialization:**

- FDG uses deterministic force calculations
- Initial machine positions: hardcoded in `setupSampleData()`
- A* uses deterministic tie-breaking (priority queue order)
- Grid nodes processed in consistent order
- No floating-point ordering issues (grid-aligned)

**Reproducibility:** Run same command twice → identical layout/routing.

## Performance Characteristics

| Operation | Time (5 mach, 6 pipes) | Notes |
|-----------|----------------------|-------|
| FDG layout | ~50–100ms | 100 iterations × 10 force calcs |
| A* per pipe | ~10–50ms | Grid resolution 0.5m, max 10k nodes |
| Total routing | ~100–300ms | 6 pipes sequential |
| Drawing | ~50–100ms | Create ~50 entities (polylines + texts) |
| **Total** | **~300–600ms** | All in main thread (acceptable) |

## Testing Strategy

### Unit Tests (`tests/test_core.cpp`)
- Point3D: distance, magnitude, normalize, dot, cross
- FDG: no overlap, bounds clamping
- A*: simple path, obstacle avoidance
- Properties: diameter, cost, tag format

### Integration Tests (`tests/test_routing.cpp`)
- Full pipeline: layout → route → compute tags
- Pipe sequencing: 6 pipes routed with mutual avoidance
- Edge cases: single-step routing, 3D paths, multi-obstacle maze

### Test Coverage
```
✓ Geometry: 10 tests
✓ Layout: 5 tests
✓ Routing: 8 tests
✓ Integration: 6 tests
─────────────────────
Total: 29 tests, ~500 lines of test code
```

## Troubleshooting Checklist

### Build Issues

| Error | Solution |
|-------|----------|
| `error: cannot find -lObjectARX` | Set `OBJECTARX_SDK_PATH` correctly; check `lib/` subdirectory |
| `Qt5_DIR not found` | `brew install qt@5` and set `-DQt5_DIR=/usr/local/opt/qt@5/lib/cmake/Qt5` |
| `Undefined reference to AcApDocument` | Link ObjectARX libraries; check CMakeLists linker |
| Arch mismatch (arm64 vs x86_64) | Set `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` |

### Runtime Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| Plugin won't load | ObjectARX SDK path wrong | Check APPLOAD error dialog; use full path |
| Machines overlap | FDG damping too low | Increase `DAMPING` to 0.95; increase iterations |
| A* hangs | Grid resolution too fine | Increase `GRID_RESOLUTION` to 1.0 |
| Tags not visible | Font height too small | Increase `setTextHeight(0.5)` in drawPipeTag |
| UI panel not docking | Qt framework issue | Restart AutoCAD; check console for Qt errors |

## Example: Adding a New Material Type

1. **Add enum value** (`core.h`):
   ```cpp
   enum class MaterialType { Liquid, Gas, Solid, Sludge, Foam };  // +Foam
   ```

2. **Add diameter** (`core.h`):
   ```cpp
   case MaterialType::Foam: return 0.20;
   ```

3. **Add cost** (`core.h`):
   ```cpp
   case MaterialType::Foam: return 110.0;
   ```

4. **Add code** (`core.h`):
   ```cpp
   case MaterialType::Foam: return "F";
   ```

5. **Add color** (`cad_adapter.cpp`):
   ```cpp
   case MaterialType::Foam: return 4;  // Magenta
   ```

6. **Test** (`tests/test_core.cpp`):
   ```cpp
   assert_equal(PipeProperties::getDiameter(MaterialType::Foam), 0.20);
   ```

7. **Rebuild:**
   ```bash
   cmake --build build --config Release
   ```

Done! Plugin now supports foam pipes.

## Future Expansion: Worker Thread

For 10+ pipes, consider async routing:

```cpp
// ui_panel.cpp
void UIPanel::onRoutePipes() {
    QThread* pThread = new QThread(this);
    RoutingWorker* pWorker = new RoutingWorker(machines, pipes);
    
    pWorker->moveToThread(pThread);
    connect(pThread, &QThread::started, pWorker, &RoutingWorker::route);
    connect(pWorker, &RoutingWorker::finished, this, &UIPanel::onRoutingDone);
    connect(pWorker, &RoutingWorker::finished, pThread, &QThread::quit);
    
    pThread->start();
}
```

Pros: UI stays responsive
Cons: Requires careful CAD API locking

## References

- **Force-Directed Placement:** Fruchterman & Reingold, "Graph Drawing by Force-directed Placement," *Software—Practice & Experience*, 1991
- **A* Algorithm:** Hart, Nilsson, Raphael, "A Formal Basis for the Heuristic Determination of Minimum Cost Paths," *IEEE Transactions on SSC*, 1968
- **P&ID Standards:** ISA SP5.1, ANSI Y32.11, ASME Y14.48M
- **ObjectARX:** Autodesk ObjectARX 2027 Documentation
- **Qt5:** Qt Documentation (Widgets, Layout, Signals/Slots)

---

**Version:** 1.0  
**Last Updated:** 2024  
**Compatibility:** AutoCAD 2027 Mac, ObjectARX SDK, Qt 5.15+

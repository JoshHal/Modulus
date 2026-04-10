# Modulus Lite — AutoCAD ObjectARX Plugin

A deterministic C++ plugin for AutoCAD (2027 Mac) that generates industrial piping layouts using force-directed graphs, Manhattan A* routing, and P&ID tagging.

## Features

- **Machines**: Support for up to 5 machines with configurable ports
- **Piping**: Route up to 6 pipes with automatic obstacle avoidance
- **Layout**: Force-directed graph (FDG) algorithm to auto-position machines
- **Routing**: Manhattan A* pathfinding with machine and pipe avoidance
- **P&ID Tags**: Automatic generation of P&ID-style labels (format: `[Line]-[Size]-[Material]-[Service]-[Spec]`)
- **Cost Analysis**: Compute pipe length and material cost
- **Visualization**: Color-coded pipes by material, flow arrows, and legend
- **Qt UI Panel**: Dockable control panel with layout/routing buttons and pipe list
- **Deterministic**: Reproducible output; no randomness after initialization

## Architecture

```
ModulusLite/
├── core.h                 # Pure C++: FDG, A*, data structures
├── cad_adapter.h/cpp      # ObjectARX integration: CAD drawing
├── ui_panel.h/cpp         # Qt UI: dockable panel
├── plugin_main.cpp        # Entry point & command handlers
├── CMakeLists.txt         # Build configuration
├── tests/
│   ├── test_core.cpp      # Unit tests for algorithms
│   └── test_routing.cpp   # Integration tests
└── README.md              # This file
```

### Module Breakdown

#### 1. **Core Module** (`core.h`)
Pure C++, no dependencies on ObjectARX or Qt.
- `Point3D`: 3D vector/point with operations (dot, cross, distance, normalize)
- `Machine`, `Pipe`: Data structures for machines and pipes
- `FDGLayout`: Force-directed graph layout (repulsion/attraction forces, 100 iterations)
- `ManhattanRouter`: A* pathfinding on 3D grid (0.5m resolution, 6 directions)
- `PipeProperties`: Diameter, cost, and tag generation

#### 2. **CAD Adapter Module** (`cad_adapter.h/cpp`)
ObjectARX bindings for AutoCAD drawing.
- `CADAdapter`: Create 3D polylines, block references, MText annotations
- `Pipeline`: Orchestrate layout → routing → drawing workflow
- Methods: `drawMachine()`, `drawPipe()`, `drawFlowArrow()`, `drawPipeTag()`, `drawLegend()`

#### 3. **UI Module** (`ui_panel.h/cpp`)
Qt-based dockable panel.
- "Generate Layout" button → runs FDG
- "Route Pipes" button → runs A* + drawing
- Pipe list with per-pipe details (length, cost, tag)

#### 4. **Plugin Entry Point** (`plugin_main.cpp`)
ObjectARX command registration and plugin lifecycle.
- `acrxEntryPoint()`: Initialize/unload hooks
- `cmdGenerateLayout()`, `cmdRoutePipes()`: Command handlers
- Sample data initialization

## Building

### Requirements

- **macOS 10.13+** (Intel or Apple Silicon)
- **Xcode 12+**
- **CMake 3.16+**
- **Qt 5.15+** (`brew install qt@5`)
- **ObjectARX 2027 SDK for Mac** (from Autodesk)

### Steps

1. **Clone and setup:**
   ```bash
   git clone <repo> ModulusLite
   cd ModulusLite
   mkdir build && cd build
   ```

2. **Configure CMake:**
   ```bash
   cmake .. \
     -DOBJECTARX_SDK_PATH=/path/to/ObjectARX2027Mac \
     -DQt5_DIR=/usr/local/opt/qt@5/lib/cmake/Qt5 \
     -DCMAKE_BUILD_TYPE=Release
   ```

3. **Build:**
   ```bash
   cmake --build . --config Release
   ```

4. **Install:**
   ```bash
   cmake --install .
   ```

   Installs to:
   ```
   ~/Library/Application Support/Autodesk/ApplicationPlugins/ModulusLite.bundle
   ```

### Testing

```bash
cd build
ctest --output-on-failure
```

Tests cover:
- Point3D operations (magnitude, distance, normalize)
- FDG layout (no overlaps, bounds)
- A* routing (simple paths, obstacle avoidance, 3D)
- Pipe properties (diameter, cost, tags)
- Full pipeline integration

## Usage

### In AutoCAD

1. **Load plugin:**
   ```
   APPLOAD → ModulusLite.bundle
   ```

2. **Run commands:**
   - `MODLAYOUT` — Generate machine layout using FDG
   - `MODROUTE` — Route pipes using A* and draw to DWG

3. **UI Panel:**
   - Opens as dockable panel (View → Palettes → Modulus Lite)
   - Click buttons to trigger commands
   - Select pipe in list to view details

### Example Output

```
Modulus Lite v1.0 loaded successfully.
Commands: MODLAYOUT, MODROUTE

MODLAYOUT
Layout generated. 5 machines positioned.

MODROUTE
Pipeline executed successfully.
Routed 6 pipes.
Pipe 0: 100-100-L-W-CS150, Length=12.34m, Cost=$1234.00
Pipe 1: 200-150-G-AIR-SS316, Length=8.56m, Cost=$685.00
...
```

## Data Structures

### Machine
```cpp
struct Machine {
    int id;
    std::string name;
    Point3D position;
    Point3D size;
    std::vector<Point3D> ports;  // Connection points
    std::string modelPath;        // DWG block path
    Point3D velocity;             // Internal (FDG)
};
```

### Pipe
```cpp
struct Pipe {
    int id;
    int fromMachineId, toMachineId;
    int fromPortIndex, toPortIndex;
    
    std::vector<Point3D> path;    // A* result
    double diameter, length, cost;
    
    MaterialType material;  // Liquid, Gas, Solid, Sludge
    ServiceType service;    // Water, Air, Slurry, Chemical
    
    int lineNumber;
    std::string specCode;   // CS150, SS316, PVC, HDPE
    std::string tag;        // "[Line]-[Size]-[Material]-[Service]-[Spec]"
};
```

## Algorithms

### Force-Directed Graph Layout

**Repulsive force:** `F_r = k² / d`
**Attractive force:** `F_a = d² / k`

Iterates 100 times, clamping positions to workspace bounds (20×15×5m). Velocity damping prevents oscillation.

### Manhattan A* Routing

**Grid resolution:** 0.5m
**Moves:** ±X, ±Y, ±Z (6 neighbors)
**Heuristic:** Manhattan distance
**Obstacles:**
- Machines (hard block, cost = 1e6)
- Existing pipes (soft penalty, cost += 5.0)

### Pipe Properties

**Diameter (mm):**
- Gas: 150
- Liquid: 100
- Solid: 250
- Sludge: 300

**Material cost ($/m):**
- Gas: 120
- Liquid: 100
- Solid: 180
- Sludge: 220

**Cost formula:** `Cost = Length × MaterialCost × Diameter`

**Tag format:** `[LineNum]-[SizeInMm]-[MatCode]-[ServiceCode]-[SpecCode]`

Example: `100-100-L-W-CS150` (Line 100, 100mm Liquid Water Carbon Steel)

## Color Legend

| Material | Color  | RGB    |
|----------|--------|--------|
| Liquid   | Cyan   | (0,255,255) |
| Gas      | Yellow | (255,255,0) |
| Solid    | Brown  | (165,42,42) |
| Sludge   | Gray   | (128,128,128) |

## Configuration

### Workspace Bounds
- X: 0–20m
- Y: 0–15m
- Z: 0–5m

### FDG Parameters
- `K` (spring constant): 5.0
- `REPULSION`: 50.0
- `ITERATIONS`: 100
- `DAMPING`: 0.9

### A* Router
- `GRID_RESOLUTION`: 0.5m
- `MACHINE_BUFFER`: 1.0m
- `PIPE_PENALTY`: 5.0

Edit `core.h` to adjust.

## Constraints

- **Max 5 machines**
- **Max 6 pipes**
- **Deterministic:** No randomness after initialization
- **Single-threaded:** Safe for AutoCAD API calls
- **DWG blocks only:** No external file formats

## Implementation Notes

### ObjectARX Specifics

- **No MFC:** Uses only core ObjectARX + Qt
- **macOS only:** No Windows-specific code (no HWND, MFC dialogs)
- **Entity creation:** Uses `AcDb3dPolyline`, `AcDbBlockReference`, `AcDbMText`
- **Color:** `AcCmColor::setColorIndex()` for material coloring
- **Lineweight:** `AcDb::LineWeight` enum scaled by diameter

### Thread Safety

- All CAD API calls in main thread only
- UI panel signals/slots (Qt) run in main thread
- No worker threads in MVP (acceptable for 5 machines, 6 pipes)

### Memory Management

- ObjectARX: Automatic via smart pointers (C++11)
- Qt: Parent-child widget ownership
- C++ objects: Stack-allocated in plugin_main (static lifetime)

## Testing

Run unit tests:
```bash
build/ModulusLiteTests
```

Output example:
```
=== Modulus Lite Core Tests ===
✓ Point3D distance test passed
✓ Point3D magnitude test passed
...
=== All tests passed! ===
```

## Future Enhancements

- [ ] Worker thread for long-running A* (10+ pipes)
- [ ] Interactive UI for machine/pipe editing
- [ ] Import/export machine definitions from CSV
- [ ] Pipe collapsing (parallel routing to same destination)
- [ ] Thermal/structural analysis integration
- [ ] Animation of FDG convergence
- [ ] Multi-document support

## Troubleshooting

### Plugin fails to load
- Check ObjectARX SDK path in CMake
- Verify Qt framework links (use `otool -L ModulusLite.bundle`)
- AutoCAD → Manage → Application Plug-ins for load messages

### Layout doesn't converge
- Increase `ITERATIONS` in `FDGLayout::layout()`
- Increase `DAMPING` (closer to 1.0)

### A* times out
- Reduce workspace bounds
- Increase grid resolution (0.5 → 1.0)
- Simplify obstacles

### Tags not displaying
- Check font availability in AutoCAD
- Verify `drawPipeTag()` adds to model space correctly

## License

MIT — See LICENSE file

## Authors

- **Senior C++ Engineer** — Core algorithms & ObjectARX adapter
- **Qt UI Developer** — Dockable panel

## References

- ObjectARX 2027 Documentation
- Fruchterman & Reingold, "Graph Drawing by Force-directed Placement" (FDG)
- A* Pathfinding by Peter Norvig
- P&ID Standards: ISA SP5.1, ANSI Y32.11

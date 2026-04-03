using Autodesk.AutoCAD.Runtime;
using Autodesk.AutoCAD.ApplicationServices;
using Autodesk.AutoCAD.EditorInput;
using Modulus.Core;
using Modulus.Core.Phase2;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

namespace Modulus
{
    /// <summary>
    /// Unified MODULUS Orchestrator
    /// Single command that seamlessly merges Phase I (Logic) and Phase II (Placement)
    /// into one complete factory layout optimization workflow.
    /// 
    /// User experience: Run MODULUS once, get optimized 3D layout in AutoCAD
    /// </summary>
    public class ModulusOrchestrator
    {
        private Document _workspace;
        private Editor _editor;
        private RequirementManifest _manifest;
        private ProcessTopology _topology;
        private SpatialLayout _layout;
        private Stopwatch _totalTimer;

        public ModulusOrchestrator(Document workspace)
        {
            _workspace = workspace ?? throw new ArgumentNullException(nameof(workspace));
            _editor = workspace.Editor;
            _totalTimer = new Stopwatch();
        }

        /// <summary>
        /// Main MODULUS Command
        /// Single entry point that runs complete optimization pipeline in one call.
        /// </summary>
        [CommandMethod("Modulus")]
        public void Run()
        {
            _totalTimer.Restart();

            try
            {
                WriteHeader();

                // Load manifest
                _editor.WriteMessage("\n[LOADING] Requirement Manifest...");
                if (!LoadManifest())
                    return;

                // ===== PHASE I: TOPOLOGICAL ANALYSIS =====
                _editor.WriteMessage("\n[PHASE I] Analyzing Process Logic...");
                var sw = Stopwatch.StartNew();

                if (!RunPhaseI())
                    return;

                sw.Stop();
                _editor.WriteMessage($"✓ Complete ({sw.ElapsedMilliseconds}ms)\n");

                // ===== PHASE II: SPATIAL OPTIMIZATION =====
                _editor.WriteMessage("[PHASE II] Optimizing Spatial Layout...");
                sw.Restart();

                if (!RunPhaseII())
                    return;

                sw.Stop();
                _editor.WriteMessage($"✓ Complete ({sw.ElapsedMilliseconds}ms)\n");

                // ===== FINAL RESULTS =====
                _totalTimer.Stop();
                DisplayFinalResults();

                _editor.WriteMessage($"\n✓ MODULUS Complete ({_totalTimer.ElapsedMilliseconds}ms total)");
            }
            catch (System.Exception ex)
            {
                _editor.WriteMessage($"\n[ERROR] {ex.Message}");
            }
        }

        /// <summary>
        /// Load and validate requirement manifest.
        /// </summary>
        private bool LoadManifest()
        {
            try
            {
                var opts = new PromptStringOptions("\nEnter manifest path (JSON): ");
                opts.AllowSpaces = true;

                var result = _editor.GetString(opts);
                if (result.Status != PromptStatus.OK || string.IsNullOrEmpty(result.StringResult))
                {
                    _editor.WriteMessage("\n[CANCELLED] No manifest provided.");
                    return false;
                }

                _manifest = ManifestParser.LoadFromJson(result.StringResult.Trim());
                _editor.WriteMessage($"✓ Loaded: {_manifest.ProjectName}");
                _editor.WriteMessage($"  {_manifest.Machines.Count} machines, {_manifest.ProcessMatrix.Flows.Count} flows");
                _editor.WriteMessage($"  Envelope: {_manifest.EnvelopeLength}×{_manifest.EnvelopeWidth}×{_manifest.EnvelopeHeight}mm");

                return true;
            }
            catch (System.Exception ex)
            {
                _editor.WriteMessage($"\n[ERROR] Failed to load manifest: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Phase I: Topological Sorting
        /// Validates process and assigns machine ranks
        /// </summary>
        private bool RunPhaseI()
        {
            try
            {
                // Build and analyze process DAG
                var engine = new TopologicalSortingEngine(_manifest);

                // Construct DAG
                _editor.WriteMessage("\n  [I.1] Constructing process graph...");
                engine.ConstructDAG();

                // Assign ranks
                _editor.WriteMessage("  [I.2] Assigning machine ranks...");
                engine.AssignRanks();

                // Analyze topology
                _editor.WriteMessage("  [I.3] Analyzing topology...");
                _topology = engine.AnalyzeTopology();

                // Validate result
                if (_topology.SourceMachines.Count == 0)
                {
                    _editor.WriteMessage("\n[ERROR] No source machines found. Invalid process.");
                    return false;
                }

                if (_topology.SinkMachines.Count == 0)
                {
                    _editor.WriteMessage("\n[ERROR] No sink machines found. Invalid process.");
                    return false;
                }

                _editor.WriteMessage($"  ✓ {_topology.SourceMachines.Count} source, " +
                    $"{_topology.IntermediateMachines.Count} intermediate, " +
                    $"{_topology.SinkMachines.Count} sink");
                _editor.WriteMessage($"  ✓ Process ranks: 0 to {_topology.GetMaxRank()}");

                return true;
            }
            catch (System.Exception ex)
            {
                _editor.WriteMessage($"\n[ERROR] Phase I failed: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Phase II: Spatial Optimization
        /// Partitions floor, optimizes positions, detects collisions, places 3D models
        /// </summary>
        private bool RunPhaseII()
        {
            try
            {
                // Create spatial layout
                _layout = new SpatialLayout
                {
                    LayoutId = Guid.NewGuid().ToString(),
                    ProjectName = _manifest.ProjectName,
                    CreatedDate = DateTime.Now,
                    EnvelopeLength = _manifest.EnvelopeLength,
                    EnvelopeWidth = _manifest.EnvelopeWidth,
                    EnvelopeHeight = _manifest.EnvelopeHeight,
                    ForbiddenZones = _manifest.RestrictedZones
                };

                var config = new LayoutConfiguration();
                var machines = new List<MachineObject>(_manifest.Machines.Values);

                // 1. Recursive Bipartitioning
                _editor.WriteMessage("\n  [II.1] Partitioning floor into zones...");
                var bipartEngine = new RecursiveBipartitioningEngine(_layout, _topology, machines, config);
                var rootZone = bipartEngine.PartitionFloorplan();
                ZoneBasedInitialPlacement.PlaceMachinesInZones(_layout, _topology, machines, rootZone);
                _editor.WriteMessage("  ✓ Zones created, initial placement done");

                // 2. Force-Directed Refinement
                _editor.WriteMessage("  [II.2] Optimizing positions...");
                var connections = ConnectionConverter.ConvertFlowsToConnections(
                    _manifest.ProcessMatrix.Flows,
                    _manifest.Machines
                );
                _layout.Connections = connections;

                var forceEngine = new ForceDirectedRefinementEngine(_layout, _topology, config, connections);
                var forceMetrics = forceEngine.OptimizeLayout();
                _editor.WriteMessage($"  ✓ Converged in {forceMetrics.ConvergedToThreshold} iterations");

                // 3. Collision Detection & Resolution
                _editor.WriteMessage("  [II.3] Resolving collisions...");
                var collisionEngine = new CollisionDetectionEngine(_layout, config);
                var collisionMetrics = collisionEngine.ResolveAllCollisions();

                if (!collisionMetrics.IsCollisionFree)
                {
                    _editor.WriteMessage($"  ⚠ Layout has {collisionMetrics.CollisionsDetected} remaining collisions");
                    _editor.WriteMessage($"    (Resolved in {collisionMetrics.TotalPasses} passes)");
                }
                else
                {
                    _editor.WriteMessage("  ✓ Collision-free layout achieved");
                }

                _layout.IsCollisionFree = collisionMetrics.IsCollisionFree;
                _layout.MaxCollisionsResolved = collisionMetrics.MaxIterationsInPass;

                // 4. 3D Model Placement in AutoCAD
                _editor.WriteMessage("  [II.4] Placing 3D models in AutoCAD...");
                var modelPlacer = new AutoCADModelPlacement(_workspace, _layout);
                modelPlacer.DrawFactoryBoundary();
                modelPlacer.PlaceAllMachines();
                modelPlacer.DrawConnections(connections);
                _editor.WriteMessage("  ✓ Models placed, connections drawn");

                return true;
            }
            catch (System.Exception ex)
            {
                _editor.WriteMessage($"\n[ERROR] Phase II failed: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Display final results and metrics.
        /// </summary>
        private void DisplayFinalResults()
        {
            _editor.WriteMessage("\n" + "═".PadRight(70, '═'));
            _editor.WriteMessage("\nFINAL OPTIMIZATION RESULTS");
            _editor.WriteMessage("\n" + "═".PadRight(70, '═'));

            // Project info
            _editor.WriteMessage($"\nProject: {_manifest.ProjectName}");
            _editor.WriteMessage($"Factory: {_manifest.EnvelopeLength}×{_manifest.EnvelopeWidth}×{_manifest.EnvelopeHeight}mm");
            _editor.WriteMessage($"Machines: {_layout.Placements.Count} placed | Flows: {_layout.Connections.Count}");

            // Topology summary
            _editor.WriteMessage($"\nProcess Topology:");
            _editor.WriteMessage($"  Sources: {_topology.SourceMachines.Count}");
            _editor.WriteMessage($"  Intermediates: {_topology.IntermediateMachines.Count}");
            _editor.WriteMessage($"  Sinks: {_topology.SinkMachines.Count}");
            _editor.WriteMessage($"  Ranks: 0 to {_topology.GetMaxRank()}");

            // Spatial quality
            _editor.WriteMessage($"\nSpatial Quality:");
            _editor.WriteMessage($"  Floor Space Utilization: {_layout.GetFloorspaceUtilization():F1}%");
            _editor.WriteMessage($"  Collision-Free: {(_layout.IsCollisionFree ? "✓ Yes" : "✗ No")}");

            // Machine placements summary
            _editor.WriteMessage($"\nMachine Placements:");
            foreach (var placement in _layout.Placements.Values
                .OrderBy(p => _topology.MachineRanks.ContainsKey(p.MachineId) 
                    ? _topology.MachineRanks[p.MachineId] : 0)
                .Take(5))
            {
                int rank = _topology.MachineRanks.ContainsKey(placement.MachineId)
                    ? _topology.MachineRanks[placement.MachineId] : 0;
                _editor.WriteMessage($"  [{rank}] {placement.MachineId}: " +
                    $"({placement.Position.X:F0}, {placement.Position.Y:F0}, {placement.Position.Z:F0})");
            }

            if (_layout.Placements.Count > 5)
                _editor.WriteMessage($"  ... and {_layout.Placements.Count - 5} more");

            // Feasibility
            string feasibility = _layout.IsCollisionFree ? "✓ FEASIBLE" : "✗ INFEASIBLE";
            _editor.WriteMessage($"\nLayout Status: {feasibility}");

            _editor.WriteMessage("\n" + "═".PadRight(70, '═'));
        }

        private void WriteHeader()
        {
            _editor.WriteMessage("\n╔" + "═".PadRight(68, '═') + "╗");
            _editor.WriteMessage("║" + "MODULUS v2.0.0 - Unified Factory Layout Optimization".PadRight(69) + "║");
            _editor.WriteMessage("╚" + "═".PadRight(68, '═') + "╝");
        }
    }

    /// <summary>
    /// Simplified API: Direct method calls for batch processing or custom integration.
    /// </summary>
    public static class ModulusAPI
    {
        public static SpatialLayout OptimizeFactory(
            RequirementManifest manifest,
            Action<string> logCallback = null)
        {
            logCallback?.Invoke("Loading manifest validation...");

            // Validate manifest
            manifest.Validate();

            // Phase I: Topological Analysis
            logCallback?.Invoke("Phase I: Analyzing process topology...");
            var topoEngine = new TopologicalSortingEngine(manifest);
            topoEngine.ConstructDAG();
            topoEngine.AssignRanks();
            var topology = topoEngine.AnalyzeTopology();

            // Phase II: Spatial Optimization
            logCallback?.Invoke("Phase II: Optimizing spatial layout...");

            var layout = new SpatialLayout
            {
                LayoutId = Guid.NewGuid().ToString(),
                ProjectName = manifest.ProjectName,
                CreatedDate = DateTime.Now,
                EnvelopeLength = manifest.EnvelopeLength,
                EnvelopeWidth = manifest.EnvelopeWidth,
                EnvelopeHeight = manifest.EnvelopeHeight,
                ForbiddenZones = manifest.RestrictedZones
            };

            var config = new LayoutConfiguration();
            var machines = new List<MachineObject>(manifest.Machines.Values);

            // Bipartitioning
            logCallback?.Invoke("  - Partitioning floor...");
            var bipartEngine = new RecursiveBipartitioningEngine(layout, topology, machines, config);
            var rootZone = bipartEngine.PartitionFloorplan();
            ZoneBasedInitialPlacement.PlaceMachinesInZones(layout, topology, machines, rootZone);

            // Force-Directed Refinement
            logCallback?.Invoke("  - Optimizing positions...");
            var connections = ConnectionConverter.ConvertFlowsToConnections(
                manifest.ProcessMatrix.Flows,
                manifest.Machines
            );
            layout.Connections = connections;

            var forceEngine = new ForceDirectedRefinementEngine(layout, topology, config, connections);
            var forceMetrics = forceEngine.OptimizeLayout();

            // Collision Resolution
            logCallback?.Invoke("  - Resolving collisions...");
            var collisionEngine = new CollisionDetectionEngine(layout, config);
            var collisionMetrics = collisionEngine.ResolveAllCollisions();

            layout.IsCollisionFree = collisionMetrics.IsCollisionFree;
            layout.MaxCollisionsResolved = collisionMetrics.MaxIterationsInPass;
            layout.FloorSpaceUtilization = layout.GetFloorspaceUtilization();

            logCallback?.Invoke("✓ Optimization complete");

            return layout;
        }

        /// <summary>
        /// Quick helper: Load manifest, optimize, and return layout.
        /// </summary>
        public static SpatialLayout OptimizeFromFile(
            string manifestPath,
            Action<string> logCallback = null)
        {
            var manifest = ManifestParser.LoadFromJson(manifestPath);
            return OptimizeFactory(manifest, logCallback);
        }

        /// <summary>
        /// Validate a manifest without running full optimization.
        /// </summary>
        public static bool ValidateManifest(RequirementManifest manifest, out string errorMessage)
        {
            try
            {
                manifest.Validate();

                var engine = new TopologicalSortingEngine(manifest);
                engine.ConstructDAG();

                errorMessage = null;
                return true;
            }
            catch (System.Exception ex)
            {
                errorMessage = ex.Message;
                return false;
            }
        }
    }
}
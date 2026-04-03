using System;
using System.Collections.Generic;
using System.Linq;
using Autodesk.AutoCAD.Geometry;

namespace Modulus.Core.Phase2
{
    /// <summary>
    /// Algorithm 2: Force-Directed Refinement
    /// Uses spring and repulsion forces to optimize machine positions.
    /// Machines connected by flows are attracted (springs).
    /// Machines that are too close are repelled.
    /// </summary>
    public class ForceDirectedRefinementEngine
    {
        private readonly SpatialLayout _layout;
        private readonly ProcessTopology _topology;
        private readonly LayoutConfiguration _config;
        private readonly List<LayoutConnection> _connections;
        private Dictionary<string, MachineForce> _forces;

        public ForceDirectedRefinementEngine(
            SpatialLayout layout,
            ProcessTopology topology,
            LayoutConfiguration config,
            List<LayoutConnection> connections)
        {
            _layout = layout ?? throw new ArgumentNullException(nameof(layout));
            _topology = topology ?? throw new ArgumentNullException(nameof(topology));
            _config = config ?? throw new ArgumentNullException(nameof(config));
            _connections = connections ?? throw new ArgumentNullException(nameof(connections));
            _forces = new Dictionary<string, MachineForce>();
        }

        /// <summary>
        /// Main entry point: Run force-directed optimization.
        /// Returns quality metrics about the layout.
        /// </summary>
        public LayoutQualityMetrics OptimizeLayout()
        {
            // Initialize forces for all machines
            InitializeForces();

            var metrics = new LayoutQualityMetrics
            {
                LayoutId = _layout.LayoutId,
                TotalZones = 0  // Will be calculated
            };

            // Run iterative force simulation
            int iteration = 0;
            double maxForce = double.MaxValue;

            while (iteration < _config.MaxIterations && maxForce > _config.ForceThreshold)
            {
                iteration++;

                // Clear forces
                foreach (var force in _forces.Values)
                    force.Clear();

                // Calculate attraction forces (springs)
                CalculateAttractionForces();

                // Calculate repulsion forces
                CalculateRepulsionForces();

                // Update positions based on forces
                UpdatePositions();

                // Get maximum force for convergence check
                maxForce = _forces.Values.Max(f => f.Magnitude);
                metrics.MaximumForce = Math.Max(metrics.MaximumForce, maxForce);
                metrics.AverageForce = _forces.Values.Average(f => f.Magnitude);
            }

            metrics.ConvergedToThreshold = iteration < _config.MaxIterations ? iteration : 0;

            // Calculate layout quality
            CalculateLayoutQuality(metrics);

            return metrics;
        }

        /// <summary>
        /// Initialize force vector for each machine.
        /// </summary>
        private void InitializeForces()
        {
            _forces.Clear();
            foreach (var machineId in _layout.Placements.Keys)
            {
                _forces[machineId] = new MachineForce { MachineId = machineId };
            }
        }

        /// <summary>
        /// Calculate attraction forces for connected machines (springs).
        /// Machines connected by flows are pulled together.
        /// </summary>
        private void CalculateAttractionForces()
        {
            foreach (var connection in _connections)
            {
                if (!_layout.Placements.ContainsKey(connection.SourceMachineId) ||
                    !_layout.Placements.ContainsKey(connection.DestinationMachineId))
                    continue;

                var sourcePlacement = _layout.Placements[connection.SourceMachineId];
                var destPlacement = _layout.Placements[connection.DestinationMachineId];

                // Vector from source to destination
                Vector3d delta = destPlacement.Position - sourcePlacement.Position;
                Vector2d delta2d = new Vector2d(delta.X, delta.Y);

                double distance = delta2d.Length;
                if (distance < 0.001) return;  // Avoid division by zero

                // Spring force: F = -k * (distance - rest_length)
                // Rest length is approximately the sum of their radii
                double restLength = (sourcePlacement.FootprintLength + destPlacement.FootprintLength) / 4.0;

                double springForce = _config.SpringConstant * (distance - restLength);

                // Direction: normalized delta
                Vector2d direction = delta2d;
                direction.Normalize();

                // Apply force to both machines (Newton's third law)
                if (_forces.ContainsKey(connection.SourceMachineId))
                {
                    _forces[connection.SourceMachineId].AddForce(
                        direction.X * springForce,
                        direction.Y * springForce
                    );
                }

                if (_forces.ContainsKey(connection.DestinationMachineId))
                {
                    _forces[connection.DestinationMachineId].AddForce(
                        -direction.X * springForce,
                        -direction.Y * springForce
                    );
                }
            }
        }

        /// <summary>
        /// Calculate repulsion forces between machines.
        /// Machines too close together are pushed apart.
        /// </summary>
        private void CalculateRepulsionForces()
        {
            var placements = _layout.Placements.Values.ToList();

            for (int i = 0; i < placements.Count; i++)
            {
                for (int j = i + 1; j < placements.Count; j++)
                {
                    var placement1 = placements[i];
                    var placement2 = placements[j];

                    Vector3d delta = placement2.Position - placement1.Position;
                    Vector2d delta2d = new Vector2d(delta.X, delta.Y);
                    double distance = delta2d.Length;

                    if (distance < 0.001) continue;

                    // Minimum distance (with safety clearance)
                    double minDistance = _config.SafetyClearance +
                        (placement1.FootprintLength + placement2.FootprintLength) / 4.0;

                    // Only apply repulsion if too close
                    if (distance < minDistance)
                    {
                        // Repulsion force: F = k / distance^2 (inverse square law)
                        double repulsionForce = _config.RepulsionConstant / (distance * distance + 0.1);

                        // Direction: away from each other
                        Vector2d direction = delta2d;
                        direction.Normalize();

                        // Apply force
                        if (_forces.ContainsKey(placement1.MachineId))
                        {
                            _forces[placement1.MachineId].AddForce(
                                -direction.X * repulsionForce,
                                -direction.Y * repulsionForce
                            );
                        }

                        if (_forces.ContainsKey(placement2.MachineId))
                        {
                            _forces[placement2.MachineId].AddForce(
                                direction.X * repulsionForce,
                                direction.Y * repulsionForce
                            );
                        }
                    }
                }
            }

            // Boundary repulsion: push machines away from walls
            foreach (var placement in placements)
            {
                // Left wall
                if (placement.Position.X - placement.FootprintLength / 2.0 < _config.SafetyClearance)
                {
                    _forces[placement.MachineId].AddForce(100, 0);
                }

                // Right wall
                if (placement.Position.X + placement.FootprintLength / 2.0 > _layout.EnvelopeLength - _config.SafetyClearance)
                {
                    _forces[placement.MachineId].AddForce(-100, 0);
                }

                // Bottom wall
                if (placement.Position.Y - placement.FootprintWidth / 2.0 < _config.SafetyClearance)
                {
                    _forces[placement.MachineId].AddForce(0, 100);
                }

                // Top wall
                if (placement.Position.Y + placement.FootprintWidth / 2.0 > _layout.EnvelopeWidth - _config.SafetyClearance)
                {
                    _forces[placement.MachineId].AddForce(0, -100);
                }
            }
        }

        /// <summary>
        /// Update machine positions based on accumulated forces.
        /// Uses damping to dissipate energy and stabilize the system.
        /// </summary>
        private void UpdatePositions()
        {
            foreach (var placement in _layout.Placements.Values)
            {
                if (!_forces.ContainsKey(placement.MachineId))
                    continue;

                var force = _forces[placement.MachineId];

                // Position update: Δposition = force * damping
                double displacement_x = force.ForceX * _config.DampingFactor;
                double displacement_y = force.ForceY * _config.DampingFactor;

                // Update position
                placement.Position = new Point3d(
                    placement.Position.X + displacement_x,
                    placement.Position.Y + displacement_y,
                    placement.Position.Z
                );

                // Clamp to factory bounds
                double halfLength = placement.FootprintLength / 2.0;
                double halfWidth = placement.FootprintWidth / 2.0;

                placement.Position = new Point3d(
                    Math.Max(_config.SafetyClearance + halfLength,
                        Math.Min(_layout.EnvelopeLength - _config.SafetyClearance - halfLength,
                            placement.Position.X)),
                    Math.Max(_config.SafetyClearance + halfWidth,
                        Math.Min(_layout.EnvelopeWidth - _config.SafetyClearance - halfWidth,
                            placement.Position.Y)),
                    placement.Position.Z
                );

                // Recalculate bounding box
                placement.CalculateBoundingBox();
                placement.ForceValue = force.Magnitude;
            }
        }

        /// <summary>
        /// Calculate quality metrics for the optimized layout.
        /// </summary>
        private void CalculateLayoutQuality(LayoutQualityMetrics metrics)
        {
            // Floor space utilization
            metrics.FloorSpaceUtilization = _layout.GetFloorspaceUtilization();

            // Safety violations (not yet - will be checked in collision phase)
            metrics.SafetyViolations = 0;

            // Total pipe length (will be calculated by path finding)
            metrics.TotalPipeLength = 0;

            // Average distance between connected machines
            double totalDistance = 0;
            int connectionCount = 0;

            foreach (var connection in _connections)
            {
                if (_layout.Placements.ContainsKey(connection.SourceMachineId) &&
                    _layout.Placements.ContainsKey(connection.DestinationMachineId))
                {
                    var src = _layout.Placements[connection.SourceMachineId];
                    var dst = _layout.Placements[connection.DestinationMachineId];

                    double distance = src.Position.DistanceTo(dst.Position);
                    totalDistance += distance;
                    connectionCount++;
                }
            }

            if (connectionCount > 0)
                metrics.AverageDistanceBetweenMachines = totalDistance / connectionCount;

            metrics.IsFeasible = metrics.SafetyViolations == 0 && metrics.CollisionsRemaining == 0;
        }

        /// <summary>
        /// Generate a report of force optimization convergence.
        /// </summary>
        public string GenerateOptimizationReport(LayoutQualityMetrics metrics)
        {
            var report = new System.Text.StringBuilder();
            report.AppendLine("=== FORCE-DIRECTED REFINEMENT REPORT ===\n");

            report.AppendLine($"Convergence: {metrics.ConvergedToThreshold} iterations");
            report.AppendLine($"Max Force: {metrics.MaximumForce:F4}");
            report.AppendLine($"Avg Force: {metrics.AverageForce:F4}");
            report.AppendLine($"Threshold: {_config.ForceThreshold:F4}\n");

            report.AppendLine($"Layout Quality:");
            report.AppendLine($"  Floorspace Utilization: {metrics.FloorSpaceUtilization:F1}%");
            report.AppendLine($"  Avg Distance Between Connected Machines: {metrics.AverageDistanceBetweenMachines:F0} mm");
            report.AppendLine($"  Safety Violations: {metrics.SafetyViolations}");
            report.AppendLine($"  Feasible: {(metrics.IsFeasible ? "✓ Yes" : "✗ No")}");

            report.AppendLine($"\nConfiguration:");
            report.AppendLine($"  Spring Constant: {_config.SpringConstant}");
            report.AppendLine($"  Repulsion Constant: {_config.RepulsionConstant}");
            report.AppendLine($"  Safety Clearance: {_config.SafetyClearance} mm");
            report.AppendLine($"  Damping Factor: {_config.DampingFactor}");

            return report.ToString();
        }
    }

    /// <summary>
    /// Helper: Convert ProcessFlow objects to LayoutConnections.
    /// </summary>
    public class ConnectionConverter
    {
        public static List<LayoutConnection> ConvertFlowsToConnections(
            List<ProcessFlow> flows,
            Dictionary<string, MachineObject> machines)
        {
            var connections = new List<LayoutConnection>();

            foreach (var flow in flows)
            {
                var connection = new LayoutConnection
                {
                    ConnectionId = flow.FlowId,
                    SourceMachineId = flow.SourceMachineId,
                    DestinationMachineId = flow.DestinationMachineId,
                    MaterialType = flow.MaterialType,
                    FlowRateKgPerHour = flow.FlowRateKgPerHour
                };

                // Get port positions (if available)
                if (machines.ContainsKey(flow.SourceMachineId) &&
                    machines[flow.SourceMachineId].Ports.ContainsKey(flow.SourcePortName))
                {
                    connection.SourcePort = machines[flow.SourceMachineId].Ports[flow.SourcePortName];
                }

                if (machines.ContainsKey(flow.DestinationMachineId) &&
                    machines[flow.DestinationMachineId].Ports.ContainsKey(flow.DestinationPortName))
                {
                    connection.DestinationPort = machines[flow.DestinationMachineId].Ports[flow.DestinationPortName];
                }

                // Set attraction force based on flow rate
                connection.AttractionForce = Math.Min(1.0, flow.FlowRateKgPerHour / 5000.0);

                connections.Add(connection);
            }

            return connections;
        }
    }
}
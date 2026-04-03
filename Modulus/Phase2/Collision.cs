using System;
using System.Collections.Generic;
using System.Linq;
using Autodesk.AutoCAD.Geometry;

namespace Modulus.Core.Phase2
{
    /// <summary>
    /// Algorithm 3: Sweep-and-Prune Collision Detection & Resolution
    /// Efficiently detects overlapping bounding boxes and resolves collisions
    /// by shifting machines along optimal directions.
    /// </summary>
    public class CollisionDetectionEngine
    {
        private readonly SpatialLayout _layout;
        private readonly LayoutConfiguration _config;
        private List<AABBRectangle> _aabbs;

        public CollisionDetectionEngine(SpatialLayout layout, LayoutConfiguration config)
        {
            _layout = layout ?? throw new ArgumentNullException(nameof(layout));
            _config = config ?? throw new ArgumentNullException(nameof(config));
        }

        /// <summary>
        /// Main entry point: Detect and resolve all collisions.
        /// Returns quality metrics about collision resolution.
        /// </summary>
        public CollisionResolutionMetrics ResolveAllCollisions()
        {
            var metrics = new CollisionResolutionMetrics();

            // Run multiple passes until no collisions remain
            for (int pass = 0; pass < _config.MaxCollisionPasses; pass++)
            {
                // Build AABB list
                BuildAABBList();

                // Detect collisions using sweep-and-prune
                var collisions = DetectCollisions();

                if (collisions.Count == 0)
                {
                    metrics.TotalPasses = pass + 1;
                    metrics.IsCollisionFree = true;
                    break;
                }

                metrics.CollisionsDetected += collisions.Count;

                // Resolve collisions
                foreach (var collision in collisions)
                {
                    ResolveCollision(collision);
                    metrics.CollisionsResolved++;
                }

                // Update metrics
                metrics.MaxIterationsInPass = Math.Max(metrics.MaxIterationsInPass, pass + 1);
            }

            metrics.IsFeasible = metrics.IsCollisionFree;
            return metrics;
        }

        /// <summary>
        /// Build Axis-Aligned Bounding Box list from current placements.
        /// </summary>
        private void BuildAABBList()
        {
            _aabbs = new List<AABBRectangle>();

            foreach (var placement in _layout.Placements.Values)
            {
                double minX = placement.BoundingBox.MinPoint.X;
                double maxX = placement.BoundingBox.MaxPoint.X;
                double minY = placement.BoundingBox.MinPoint.Y;
                double maxY = placement.BoundingBox.MaxPoint.Y;

                _aabbs.Add(new AABBRectangle(placement.MachineId, minX, maxX, minY, maxY));
            }
        }

        /// <summary>
        /// Sweep-and-Prune: Efficient O(N log N) collision detection.
        /// Returns list of colliding machine pairs.
        /// </summary>
        private List<CollisionPair> DetectCollisions()
        {
            var collisions = new List<CollisionPair>();

            // Sweep along X-axis
            var sortedByX = _aabbs.OrderBy(a => a.MinX).ToList();

            for (int i = 0; i < sortedByX.Count; i++)
            {
                for (int j = i + 1; j < sortedByX.Count; j++)
                {
                    var aabb1 = sortedByX[i];
                    var aabb2 = sortedByX[j];

                    // If X ranges don't overlap, no need to check further
                    if (aabb2.MinX > aabb1.MaxX + _config.SafetyClearance)
                        break;

                    // Check full 2D overlap
                    if (aabb1.OverlapsWith(aabb2))
                    {
                        collisions.Add(new CollisionPair
                        {
                            MachineId1 = aabb1.ObjectId,
                            MachineId2 = aabb2.ObjectId,
                            OverlapX = aabb1.GetOverlapX(aabb2),
                            OverlapY = aabb1.GetOverlapY(aabb2)
                        });
                    }
                }
            }

            return collisions;
        }

        /// <summary>
        /// Resolve a single collision by separating machines.
        /// Chooses optimal direction based on overlap and factory constraints.
        /// </summary>
        private void ResolveCollision(CollisionPair collision)
        {
            var machine1 = _layout.Placements[collision.MachineId1];
            var machine2 = _layout.Placements[collision.MachineId2];

            // Determine separation direction (X or Y)
            // Choose direction with least overlap for most efficient separation
            if (collision.OverlapX < collision.OverlapY)
            {
                // Separate along X-axis
                SeparateAlongAxis(machine1, machine2, true);
            }
            else
            {
                // Separate along Y-axis
                SeparateAlongAxis(machine1, machine2, false);
            }

            machine1.CalculateBoundingBox();
            machine2.CalculateBoundingBox();
            machine1.CollisionIterations++;
            machine2.CollisionIterations++;
        }

        /// <summary>
        /// Separate two overlapping machines along a specific axis.
        /// </summary>
        private void SeparateAlongAxis(MachinePlacement m1, MachinePlacement m2, bool separateX)
        {
            if (separateX)
            {
                // Separate along X-axis
                double separation = (m1.FootprintLength + m2.FootprintLength) / 2.0 + _config.SafetyClearance;

                // Determine which machine to move
                if (m1.Position.X < m2.Position.X)
                {
                    // m1 is to the left, move it left (if possible)
                    double newX1 = m2.Position.X - separation / 2.0;
                    double newX2 = m2.Position.X + separation / 2.0;

                    // Check which direction has more space
                    bool canMoveLeft = newX1 - m1.FootprintLength / 2.0 >= _config.SafetyClearance;
                    bool canMoveRight = newX2 + m2.FootprintLength / 2.0 <= _layout.EnvelopeLength - _config.SafetyClearance;

                    if (canMoveLeft)
                    {
                        m1.Position = new Point3d(newX1, m1.Position.Y, m1.Position.Z);
                    }
                    else if (canMoveRight)
                    {
                        m2.Position = new Point3d(newX2, m2.Position.Y, m2.Position.Z);
                    }
                    else
                    {
                        // Push both apart
                        m1.Position = new Point3d(m1.Position.X - separation / 4.0, m1.Position.Y, m1.Position.Z);
                        m2.Position = new Point3d(m2.Position.X + separation / 4.0, m2.Position.Y, m2.Position.Z);
                    }
                }
                else
                {
                    // m2 is to the left, move it left
                    double newX2 = m1.Position.X - separation / 2.0;
                    double newX1 = m1.Position.X + separation / 2.0;

                    bool canMoveLeft = newX2 - m2.FootprintLength / 2.0 >= _config.SafetyClearance;
                    bool canMoveRight = newX1 + m1.FootprintLength / 2.0 <= _layout.EnvelopeLength - _config.SafetyClearance;

                    if (canMoveLeft)
                    {
                        m2.Position = new Point3d(newX2, m2.Position.Y, m2.Position.Z);
                    }
                    else if (canMoveRight)
                    {
                        m1.Position = new Point3d(newX1, m1.Position.Y, m1.Position.Z);
                    }
                    else
                    {
                        m1.Position = new Point3d(m1.Position.X + separation / 4.0, m1.Position.Y, m1.Position.Z);
                        m2.Position = new Point3d(m2.Position.X - separation / 4.0, m2.Position.Y, m2.Position.Z);
                    }
                }
            }
            else
            {
                // Separate along Y-axis
                double separation = (m1.FootprintWidth + m2.FootprintWidth) / 2.0 + _config.SafetyClearance;

                if (m1.Position.Y < m2.Position.Y)
                {
                    // m1 is below, move it down (if possible)
                    double newY1 = m2.Position.Y - separation / 2.0;
                    double newY2 = m2.Position.Y + separation / 2.0;

                    bool canMoveDown = newY1 - m1.FootprintWidth / 2.0 >= _config.SafetyClearance;
                    bool canMoveUp = newY2 + m2.FootprintWidth / 2.0 <= _layout.EnvelopeWidth - _config.SafetyClearance;

                    if (canMoveDown)
                    {
                        m1.Position = new Point3d(m1.Position.X, newY1, m1.Position.Z);
                    }
                    else if (canMoveUp)
                    {
                        m2.Position = new Point3d(m2.Position.X, newY2, m2.Position.Z);
                    }
                    else
                    {
                        m1.Position = new Point3d(m1.Position.X, m1.Position.Y - separation / 4.0, m1.Position.Z);
                        m2.Position = new Point3d(m2.Position.X, m2.Position.Y + separation / 4.0, m2.Position.Z);
                    }
                }
                else
                {
                    // m2 is below, move it down
                    double newY2 = m1.Position.Y - separation / 2.0;
                    double newY1 = m1.Position.Y + separation / 2.0;

                    bool canMoveDown = newY2 - m2.FootprintWidth / 2.0 >= _config.SafetyClearance;
                    bool canMoveUp = newY1 + m1.FootprintWidth / 2.0 <= _layout.EnvelopeWidth - _config.SafetyClearance;

                    if (canMoveDown)
                    {
                        m2.Position = new Point3d(m2.Position.X, newY2, m2.Position.Z);
                    }
                    else if (canMoveUp)
                    {
                        m1.Position = new Point3d(m1.Position.X, newY1, m1.Position.Z);
                    }
                    else
                    {
                        m1.Position = new Point3d(m1.Position.X, m1.Position.Y + separation / 4.0, m1.Position.Z);
                        m2.Position = new Point3d(m2.Position.X, m2.Position.Y - separation / 4.0, m2.Position.Z);
                    }
                }
            }
        }

        /// <summary>
        /// Generate collision detection report.
        /// </summary>
        public string GenerateCollisionReport(CollisionResolutionMetrics metrics)
        {
            var report = new System.Text.StringBuilder();
            report.AppendLine("=== COLLISION DETECTION & RESOLUTION REPORT ===\n");

            report.AppendLine($"Collision Passes: {metrics.TotalPasses}");
            report.AppendLine($"Collisions Detected: {metrics.CollisionsDetected}");
            report.AppendLine($"Collisions Resolved: {metrics.CollisionsResolved}");
            report.AppendLine($"Max Iterations in Pass: {metrics.MaxIterationsInPass}");
            report.AppendLine($"Collision-Free: {(metrics.IsCollisionFree ? "✓ Yes" : "✗ No")}");
            report.AppendLine($"Feasible: {(metrics.IsFeasible ? "✓ Yes" : "✗ No")}");

            report.AppendLine($"\nCurrent Collisions:");
            var currentCollisions = DetectCollisions();
            if (currentCollisions.Count == 0)
            {
                report.AppendLine("  None - Layout is collision-free!");
            }
            else
            {
                foreach (var collision in currentCollisions)
                {
                    report.AppendLine($"  {collision.MachineId1} ↔ {collision.MachineId2}");
                }
            }

            report.AppendLine($"\nMachine Collision Iteration Counts:");
            foreach (var placement in _layout.Placements.Values.OrderByDescending(p => p.CollisionIterations).Take(5))
            {
                report.AppendLine($"  {placement.MachineId}: {placement.CollisionIterations} iterations");
            }

            return report.ToString();
        }
    }

    /// <summary>
    /// Represents a collision between two machines.
    /// </summary>
    public class CollisionPair
    {
        public string MachineId1 { get; set; }
        public string MachineId2 { get; set; }
        public double OverlapX { get; set; }
        public double OverlapY { get; set; }
    }

    /// <summary>
    /// Metrics for collision resolution process.
    /// </summary>
    public class CollisionResolutionMetrics
    {
        public int TotalPasses { get; set; }
        public int CollisionsDetected { get; set; }
        public int CollisionsResolved { get; set; }
        public int MaxIterationsInPass { get; set; }
        public bool IsCollisionFree { get; set; }
        public bool IsFeasible { get; set; }

        public override string ToString()
        {
            return $@"
Collision Resolution:
  Total Passes: {TotalPasses}
  Collisions Detected: {CollisionsDetected}
  Collisions Resolved: {CollisionsResolved}
  Collision-Free: {(IsCollisionFree ? "✓" : "✗")}
  Feasible: {(IsFeasible ? "✓" : "✗")}
";
        }
    }
}
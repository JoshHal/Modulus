using System;
using System.Collections.Generic;
using Autodesk.AutoCAD.Geometry;

namespace Modulus.Core.Phase2
{
    /// <summary>
    /// Phase II: Spatial placement record for a single machine.
    /// Stores position, orientation, and collision data.
    /// </summary>
    public class MachinePlacement
    {
        public string MachineId { get; set; }
        public string MachineName { get; set; }

        // Physical placement
        public Point3d Position { get; set; }        // Center point (X, Y, Z) in mm
        public double RotationAngleDegrees { get; set; } = 0.0;  // Z-axis rotation

        // Machine dimensions (from original specs)
        public double FootprintLength { get; set; }
        public double FootprintWidth { get; set; }
        public double FootprintHeight { get; set; }

        // Calculated bounding box (after rotation)
        public Extents3d BoundingBox { get; set; }

        // Assigned zone (from recursive partitioning)
        public string ZoneId { get; set; }

        // Rank (from Phase I topological analysis)
        public int Rank { get; set; }

        // Quality metrics
        public double ForceValue { get; set; }  // Final force magnitude
        public int CollisionIterations { get; set; }  // Collision resolution count

        public override string ToString()
        {
            return $"[{MachineId}] @ ({Position.X:F0}, {Position.Y:F0}, {Position.Z:F0})";
        }

        /// <summary>
        /// Calculate bounding box for axis-aligned placement.
        /// </summary>
        public void CalculateBoundingBox()
        {
            double halfLength = FootprintLength / 2.0;
            double halfWidth = FootprintWidth / 2.0;
            double halfHeight = FootprintHeight / 2.0;

            var minPoint = new Point3d(
                Position.X - halfLength,
                Position.Y - halfWidth,
                Position.Z - halfHeight
            );

            var maxPoint = new Point3d(
                Position.X + halfLength,
                Position.Y + halfWidth,
                Position.Z + halfHeight
            );

            BoundingBox = new Extents3d(minPoint, maxPoint);
        }

        /// <summary>
        /// Check if this machine's bounding box intersects another.
        /// </summary>
        public bool IntersectsWith(MachinePlacement other, double clearance = 0.0)
        {
            if (other == null) return false;

            double minX1 = BoundingBox.MinPoint.X - clearance;
            double maxX1 = BoundingBox.MaxPoint.X + clearance;
            double minY1 = BoundingBox.MinPoint.Y - clearance;
            double maxY1 = BoundingBox.MaxPoint.Y + clearance;

            double minX2 = other.BoundingBox.MinPoint.X - clearance;
            double maxX2 = other.BoundingBox.MaxPoint.X + clearance;
            double minY2 = other.BoundingBox.MinPoint.Y - clearance;
            double maxY2 = other.BoundingBox.MaxPoint.Y + clearance;

            // Check for overlap in both X and Y
            return !(maxX1 < minX2 || minX1 > maxX2 ||
                     maxY1 < minY2 || minY1 > maxY2);
        }
    }

    /// <summary>
    /// Rectangular zone created by recursive bipartitioning.
    /// </summary>
    public class LayoutZone
    {
        public string ZoneId { get; set; }
        public string ParentZoneId { get; set; }

        // Zone boundaries (in mm)
        public double MinX { get; set; }
        public double MaxX { get; set; }
        public double MinY { get; set; }
        public double MaxY { get; set; }

        // Assigned machines and area tracking
        public List<string> AssignedMachineIds { get; set; } = new List<string>();
        public double UsedArea { get; set; }
        public double TotalArea => (MaxX - MinX) * (MaxY - MinY);

        // Partition info
        public int Rank { get; set; }  // Which process rank this zone represents
        public bool IsLeafZone { get; set; } = true;

        // Child zones (if subdivided)
        public LayoutZone LeftChild { get; set; }
        public LayoutZone RightChild { get; set; }

        public LayoutZone(string id, double minX, double maxX, double minY, double maxY)
        {
            ZoneId = id;
            MinX = minX;
            MaxX = maxX;
            MinY = minY;
            MaxY = maxY;
        }

        public double Width => MaxX - MinX;
        public double Height => MaxY - MinY;

        public Point3d Center => new Point3d(
            (MinX + MaxX) / 2.0,
            (MinY + MaxY) / 2.0,
            0.0
        );

        public bool ContainsPoint(Point3d point)
        {
            return point.X >= MinX && point.X <= MaxX &&
                   point.Y >= MinY && point.Y <= MaxY;
        }

        public override string ToString()
        {
            return $"Zone[{ZoneId}]: ({MinX:F0}-{MaxX:F0}) × ({MinY:F0}-{MaxY:F0})";
        }
    }

    /// <summary>
    /// Connection between two machines (used for force calculations).
    /// </summary>
    public class LayoutConnection
    {
        public string ConnectionId { get; set; }
        public string SourceMachineId { get; set; }
        public string DestinationMachineId { get; set; }

        public string MaterialType { get; set; }
        public double FlowRateKgPerHour { get; set; }

        // Ports for routing
        public Point3d SourcePort { get; set; }
        public Point3d DestinationPort { get; set; }

        // Routing preference
        public double AttractionForce { get; set; } = 1.0;  // Strength of pull

        public override string ToString()
        {
            return $"{SourceMachineId} → {DestinationMachineId} ({MaterialType})";
        }
    }

    /// <summary>
    /// Phase II output: Complete spatial layout with all placements.
    /// </summary>
    public class SpatialLayout
    {
        public string LayoutId { get; set; }
        public string ProjectName { get; set; }
        public DateTime CreatedDate { get; set; }

        // Factory envelope
        public double EnvelopeLength { get; set; }
        public double EnvelopeWidth { get; set; }
        public double EnvelopeHeight { get; set; }

        // Placements
        public Dictionary<string, MachinePlacement> Placements { get; set; } =
            new Dictionary<string, MachinePlacement>();

        // Zone structure (from recursive partitioning)
        public LayoutZone RootZone { get; set; }

        // Connections with calculated paths
        public List<LayoutConnection> Connections { get; set; } =
            new List<LayoutConnection>();

        // Layout quality metrics
        public double TotalPipeLength { get; set; }  // Calculated routing distance
        public double FloorSpaceUtilization { get; set; }  // Percentage used
        public double MaxCollisionsResolved { get; set; }  // Peak iteration count
        public bool IsCollisionFree { get; set; }

        // Restricted zones (no placement)
        public List<Modulus.Core.RestrictedZone> ForbiddenZones { get; set; } =
            new List<Modulus.Core.RestrictedZone>();

        public double GetTotalUsedArea()
        {
            double totalArea = 0;
            foreach (var placement in Placements.Values)
            {
                totalArea += placement.FootprintLength * placement.FootprintWidth;
            }
            return totalArea;
        }

        public double GetFloorspaceUtilization()
        {
            double totalArea = EnvelopeLength * EnvelopeWidth;
            double usedArea = GetTotalUsedArea();
            return (usedArea / totalArea) * 100.0;
        }
    }

    /// <summary>
    /// Force vector acting on a machine (for force-directed refinement).
    /// </summary>
    public class MachineForce
    {
        public string MachineId { get; set; }

        // Accumulated force components
        public double ForceX { get; set; }
        public double ForceY { get; set; }

        // Magnitude and direction
        public double Magnitude => Math.Sqrt(ForceX * ForceX + ForceY * ForceY);
        public double AngleRadians => Math.Atan2(ForceY, ForceX);

        public void AddForce(double dx, double dy)
        {
            ForceX += dx;
            ForceY += dy;
        }

        public void AddForce(Vector2d vector)
        {
            ForceX += vector.X;
            ForceY += vector.Y;
        }

        public void Clear()
        {
            ForceX = 0;
            ForceY = 0;
        }

        public override string ToString()
        {
            return $"[{MachineId}] Force: ({ForceX:F2}, {ForceY:F2}) = {Magnitude:F2}";
        }
    }

    /// <summary>
    /// Configuration for spatial layout algorithm.
    /// </summary>
    public class LayoutConfiguration
    {
        // Force-directed parameters
        public double SpringConstant { get; set; } = 0.5;     // Attraction strength
        public double RepulsionConstant { get; set; } = 50.0; // Repulsion strength
        public double SafetyClearance { get; set; } = 500.0;  // Minimum clearance (mm)
        public double DampingFactor { get; set; } = 0.95;     // Energy dissipation

        // Convergence criteria
        public double ForceThreshold { get; set; } = 0.01;    // Stop when forces < this
        public int MaxIterations { get; set; } = 1000;        // Max force iterations
        public int MaxCollisionPasses { get; set; } = 10;     // Max collision resolution

        // Zone partitioning
        public bool UseRecursiveBipartition { get; set; } = true;
        public double MinZoneArea { get; set; } = 2000000.0;  // Minimum zone size (mm²)

        // Optimization weights (from Phase I constraints)
        public double PipeLengthWeight { get; set; } = 0.3;
        public double MaintenanceAccessWeight { get; set; } = 0.2;
        public double EnergyEfficiencyWeight { get; set; } = 0.25;
        public double SafetyZoneWeight { get; set; } = 0.15;
        public double FloorSpaceUtilizationWeight { get; set; } = 0.1;
    }

    /// <summary>
    /// Rectangle used in Sweep-and-Prune collision detection.
    /// </summary>
    public class AABBRectangle
    {
        public string ObjectId { get; set; }
        public double MinX { get; set; }
        public double MaxX { get; set; }
        public double MinY { get; set; }
        public double MaxY { get; set; }

        public AABBRectangle(string id, double minX, double maxX, double minY, double maxY)
        {
            ObjectId = id;
            MinX = minX;
            MaxX = maxX;
            MinY = minY;
            MaxY = maxY;
        }

        public bool OverlapsWith(AABBRectangle other)
        {
            return !(MaxX < other.MinX || MinX > other.MaxX ||
                     MaxY < other.MinY || MinY > other.MaxY);
        }

        public double GetOverlapX(AABBRectangle other)
        {
            if (!OverlapsWith(other)) return 0;
            return Math.Min(MaxX, other.MaxX) - Math.Max(MinX, other.MinX);
        }

        public double GetOverlapY(AABBRectangle other)
        {
            if (!OverlapsWith(other)) return 0;
            return Math.Min(MaxY, other.MaxY) - Math.Max(MinY, other.MinY);
        }
    }

    /// <summary>
    /// Statistics and quality metrics for the layout.
    /// </summary>
    public class LayoutQualityMetrics
    {
        public string LayoutId { get; set; }

        // Spatial metrics
        public double FloorSpaceUtilization { get; set; }  // 0-100 %
        public double AverageDistanceBetweenMachines { get; set; }
        public double TotalPipeLength { get; set; }

        // Constraint satisfaction
        public int SafetyViolations { get; set; }
        public int CollisionsRemaining { get; set; }
        public bool IsFeasible { get; set; }

        // Force metrics
        public double MaximumForce { get; set; }
        public double AverageForce { get; set; }
        public int ConvergedToThreshold { get; set; }

        // Zone utilization
        public double AverageZoneUtilization { get; set; }
        public int TotalZones { get; set; }

        public override string ToString()
        {
            return $@"
                Layout Quality Metrics:
                Floorspace Utilization: {FloorSpaceUtilization:F1}%
                Total Pipe Length: {TotalPipeLength:F0} mm
                Safety Violations: {SafetyViolations}
                Collisions Remaining: {CollisionsRemaining}
                Max Force: {MaximumForce:F2}
                Avg Force: {AverageForce:F2}
                Feasible: {(IsFeasible ? "✓" : "✗")}
            ";
        }
    }
}
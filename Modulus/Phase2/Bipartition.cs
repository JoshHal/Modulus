using System;
using System.Collections.Generic;
using System.Linq;
using Autodesk.AutoCAD.Geometry;

namespace Modulus.Core.Phase2
{
    /// <summary>
    /// Algorithm 1: Recursive Bipartitioning (Slicing Tree)
    /// Divides factory envelope into zones based on machine area requirements per rank.
    /// Creates balanced rectangular regions for machine placement.
    /// </summary>
    public class RecursiveBipartitioningEngine
    {
        private readonly SpatialLayout _layout;
        private readonly ProcessTopology _topology;
        private readonly List<MachineObject> _allMachines;
        private readonly LayoutConfiguration _config;
        private int _zoneCounter = 0;

        public RecursiveBipartitioningEngine(
            SpatialLayout layout,
            ProcessTopology topology,
            List<MachineObject> machines,
            LayoutConfiguration config)
        {
            _layout = layout ?? throw new ArgumentNullException(nameof(layout));
            _topology = topology ?? throw new ArgumentNullException(nameof(topology));
            _allMachines = machines ?? throw new ArgumentNullException(nameof(machines));
            _config = config ?? throw new ArgumentNullException(nameof(config));
        }

        /// <summary>
        /// Main entry point: Partition factory envelope into zones.
        /// Calls recursive bipartitioning algorithm.
        /// </summary>
        public LayoutZone PartitionFloorplan()
        {
            // Create root zone (entire factory floor)
            var rootZone = new LayoutZone(
                GenerateZoneId(),
                0,
                _layout.EnvelopeLength,
                0,
                _layout.EnvelopeWidth
            );

            // Get list of ranks and their machines
            var machinesByRank = GroupMachinesByRank();

            // Recursively partition the floor
            RecursiveBipartition(rootZone, machinesByRank, 0);

            _layout.RootZone = rootZone;
            return rootZone;
        }

        /// <summary>
        /// Core recursive bipartitioning algorithm.
        /// Recursively splits zones along best partition line.
        /// </summary>
        private void RecursiveBipartition(
            LayoutZone zone,
            Dictionary<int, List<MachineObject>> machinesByRank,
            int rankIndex)
        {
            // Base case: zone is small enough or no machines assigned
            if (zone.TotalArea < _config.MinZoneArea ||
                zone.AssignedMachineIds.Count == 0)
            {
                zone.IsLeafZone = true;
                return;
            }

            // Get machines for this zone
            var machinesInZone = zone.AssignedMachineIds
                .Select(id => _allMachines.FirstOrDefault(m => m.MachineId == id))
                .Where(m => m != null)
                .ToList();

            if (machinesInZone.Count <= 1)
            {
                zone.IsLeafZone = true;
                return;
            }

            // Calculate total area needed
            double totalAreaNeeded = machinesInZone.Sum(m => m.FootprintLength * m.FootprintWidth);

            // If area is tight, don't subdivide further
            if (totalAreaNeeded > zone.TotalArea * 0.9)
            {
                zone.IsLeafZone = true;
                return;
            }

            // Find best partition line (horizontal or vertical)
            var partition = FindBestPartition(zone, machinesInZone);

            if (partition == null)
            {
                zone.IsLeafZone = true;
                return;
            }

            // Create left and right child zones
            zone.IsLeafZone = false;

            if (partition.IsVertical)
            {
                // Vertical cut (constant X)
                zone.LeftChild = new LayoutZone(
                    GenerateZoneId(),
                    zone.MinX,
                    partition.CutPosition,
                    zone.MinY,
                    zone.MaxY
                )
                { ParentZoneId = zone.ZoneId };

                zone.RightChild = new LayoutZone(
                    GenerateZoneId(),
                    partition.CutPosition,
                    zone.MaxX,
                    zone.MinY,
                    zone.MaxY
                )
                { ParentZoneId = zone.ZoneId };
            }
            else
            {
                // Horizontal cut (constant Y)
                zone.LeftChild = new LayoutZone(
                    GenerateZoneId(),
                    zone.MinX,
                    zone.MaxX,
                    zone.MinY,
                    partition.CutPosition
                )
                { ParentZoneId = zone.ZoneId };

                zone.RightChild = new LayoutZone(
                    GenerateZoneId(),
                    zone.MinX,
                    zone.MaxX,
                    partition.CutPosition,
                    zone.MaxY
                )
                { ParentZoneId = zone.ZoneId };
            }

            // Distribute machines to child zones
            DistributeMachinesToChildren(zone, machinesInZone);

            // Recurse on both children
            RecursiveBipartition(zone.LeftChild, machinesByRank, rankIndex + 1);
            RecursiveBipartition(zone.RightChild, machinesByRank, rankIndex + 1);
        }

        /// <summary>
        /// Find the best partition line (vertical or horizontal) for a zone.
        /// Uses heuristic: balance area and aspect ratio.
        /// </summary>
        private PartitionLine FindBestPartition(LayoutZone zone, List<MachineObject> machines)
        {
            double zoneWidth = zone.Width;
            double zoneHeight = zone.Height;

            double bestScore = double.MaxValue;
            PartitionLine bestPartition = null;

            // Try vertical cuts
            double stepX = zoneWidth / 10.0;  // Try 10 candidate positions
            for (double x = zone.MinX + stepX; x < zone.MaxX; x += stepX)
            {
                var leftZone = new LayoutZone("temp", zone.MinX, x, zone.MinY, zone.MaxY);
                var rightZone = new LayoutZone("temp", x, zone.MaxX, zone.MinY, zone.MaxY);

                // Count machines on each side (approximate)
                int leftCount = machines.Count(m => m.FootprintLength / 2.0 < (x - zone.MinX));
                int rightCount = machines.Count - leftCount;

                // Score: minimize imbalance (prefer 50-50 split)
                double imbalance = Math.Abs(leftCount - rightCount) / (double)machines.Count;
                double aspectLeft = Math.Abs(leftZone.Width - leftZone.Height);
                double aspectRight = Math.Abs(rightZone.Width - rightZone.Height);
                double score = imbalance + (aspectLeft + aspectRight) * 0.01;

                if (score < bestScore)
                {
                    bestScore = score;
                    bestPartition = new PartitionLine { IsVertical = true, CutPosition = x };
                }
            }

            // Try horizontal cuts
            double stepY = zoneHeight / 10.0;
            for (double y = zone.MinY + stepY; y < zone.MaxY; y += stepY)
            {
                var topZone = new LayoutZone("temp", zone.MinX, zone.MaxX, zone.MinY, y);
                var bottomZone = new LayoutZone("temp", zone.MinX, zone.MaxX, y, zone.MaxY);

                // Count machines on each side
                int topCount = machines.Count(m => m.FootprintWidth / 2.0 < (y - zone.MinY));
                int bottomCount = machines.Count - topCount;

                double imbalance = Math.Abs(topCount - bottomCount) / (double)machines.Count;
                double aspectTop = Math.Abs(topZone.Width - topZone.Height);
                double aspectBottom = Math.Abs(bottomZone.Width - bottomZone.Height);
                double score = imbalance + (aspectTop + aspectBottom) * 0.01;

                if (score < bestScore)
                {
                    bestScore = score;
                    bestPartition = new PartitionLine { IsVertical = false, CutPosition = y };
                }
            }

            return bestPartition;
        }

        /// <summary>
        /// Distribute machines to left and right child zones.
        /// </summary>
        private void DistributeMachinesToChildren(LayoutZone parent, List<MachineObject> machines)
        {
            foreach (var machine in machines)
            {
                bool goToLeft;

                if (parent.LeftChild.MaxX == parent.RightChild.MinX)
                {
                    // Vertical partition: compare X position
                    double partitionX = parent.LeftChild.MaxX;
                    goToLeft = machine.FootprintLength / 2.0 < partitionX;
                }
                else
                {
                    // Horizontal partition: compare Y position
                    double partitionY = parent.LeftChild.MaxY;
                    goToLeft = machine.FootprintWidth / 2.0 < partitionY;
                }

                var targetZone = goToLeft ? parent.LeftChild : parent.RightChild;
                targetZone.AssignedMachineIds.Add(machine.MachineId);
                targetZone.UsedArea += machine.FootprintLength * machine.FootprintWidth;
            }
        }

        /// <summary>
        /// Group machines by their rank (from Phase I topological analysis).
        /// </summary>
        private Dictionary<int, List<MachineObject>> GroupMachinesByRank()
        {
            var grouped = new Dictionary<int, List<MachineObject>>();

            for (int rank = 0; rank <= _topology.GetMaxRank(); rank++)
            {
                var machinesAtRank = _topology.GetMachinesAtRank(rank)
                    .Select(id => _allMachines.FirstOrDefault(m => m.MachineId == id))
                    .Where(m => m != null)
                    .ToList();

                if (machinesAtRank.Count > 0)
                    grouped[rank] = machinesAtRank;
            }

            return grouped;
        }

        /// <summary>
        /// Get all leaf zones (actual placement areas).
        /// </summary>
        public List<LayoutZone> GetLeafZones()
        {
            var leafZones = new List<LayoutZone>();
            CollectLeafZones(_layout.RootZone, leafZones);
            return leafZones;
        }

        private void CollectLeafZones(LayoutZone zone, List<LayoutZone> result)
        {
            if (zone == null) return;

            if (zone.IsLeafZone)
            {
                result.Add(zone);
            }
            else
            {
                CollectLeafZones(zone.LeftChild, result);
                CollectLeafZones(zone.RightChild, result);
            }
        }

        /// <summary>
        /// Get zone hierarchy statistics.
        /// </summary>
        public string GetPartitionStatistics()
        {
            var report = new System.Text.StringBuilder();
            report.AppendLine("=== RECURSIVE BIPARTITIONING STATISTICS ===\n");

            var leafZones = GetLeafZones();
            report.AppendLine($"Total Leaf Zones: {leafZones.Count}");
            report.AppendLine($"Total Zone Counter: {_zoneCounter}\n");

            report.AppendLine("Zone Distribution:");
            var zonesBySize = leafZones.OrderByDescending(z => z.TotalArea).ToList();
            for (int i = 0; i < Math.Min(5, zonesBySize.Count); i++)
            {
                var z = zonesBySize[i];
                report.AppendLine(
                    $"  {z.ZoneId}: {z.Width:F0}×{z.Height:F0}mm " +
                    $"({z.TotalArea / 1000000:F2}m²) " +
                    $"Machines: {z.AssignedMachineIds.Count}");
            }

            double avgArea = leafZones.Average(z => z.TotalArea);
            double maxArea = leafZones.Max(z => z.TotalArea);
            double minArea = leafZones.Min(z => z.TotalArea);

            report.AppendLine($"\nZone Area Statistics:");
            report.AppendLine($"  Average: {avgArea / 1000000:F2}m²");
            report.AppendLine($"  Maximum: {maxArea / 1000000:F2}m²");
            report.AppendLine($"  Minimum: {minArea / 1000000:F2}m²");

            return report.ToString();
        }

        private string GenerateZoneId()
        {
            return $"Zone_{_zoneCounter++}";
        }

        /// <summary>
        /// Helper class for partition information.
        /// </summary>
        private class PartitionLine
        {
            public bool IsVertical { get; set; }
            public double CutPosition { get; set; }
        }
    }

    /// <summary>
    /// Extension: Position machines within their assigned zones.
    /// Uses zone center as initial placement.
    /// </summary>
    public class ZoneBasedInitialPlacement
    {
        public static void PlaceMachinesInZones(
            SpatialLayout layout,
            ProcessTopology topology,
            List<MachineObject> machines,
            LayoutZone rootZone)
        {
            var leafZones = GetLeafZones(rootZone);

            foreach (var zone in leafZones)
            {
                var machinesInZone = zone.AssignedMachineIds
                    .Select(id => machines.FirstOrDefault(m => m.MachineId == id))
                    .Where(m => m != null)
                    .ToList();

                if (machinesInZone.Count == 0) continue;

                // Place machines in a grid within the zone
                int cols = (int)Math.Ceiling(Math.Sqrt(machinesInZone.Count));
                int rows = (int)Math.Ceiling(machinesInZone.Count / (double)cols);

                double cellWidth = zone.Width / cols;
                double cellHeight = zone.Height / rows;

                int idx = 0;
                for (int row = 0; row < rows; row++)
                {
                    for (int col = 0; col < cols; col++)
                    {
                        if (idx >= machinesInZone.Count) break;

                        var machine = machinesInZone[idx];
                        var placement = new MachinePlacement
                        {
                            MachineId = machine.MachineId,
                            MachineName = machine.MachineName,
                            FootprintLength = machine.FootprintLength,
                            FootprintWidth = machine.FootprintWidth,
                            FootprintHeight = machine.FootprintHeight,
                            Position = new Point3d(
                                zone.MinX + cellWidth * (col + 0.5),
                                zone.MinY + cellHeight * (row + 0.5),
                                machine.FootprintHeight / 2.0
                            ),
                            ZoneId = zone.ZoneId,
                            Rank = topology.MachineRanks.ContainsKey(machine.MachineId)
                                ? topology.MachineRanks[machine.MachineId]
                                : 0
                        };

                        placement.CalculateBoundingBox();
                        layout.Placements[machine.MachineId] = placement;
                        idx++;
                    }
                }
            }
        }

        private static List<LayoutZone> GetLeafZones(LayoutZone zone)
        {
            var result = new List<LayoutZone>();
            if (zone.IsLeafZone)
                result.Add(zone);
            else
            {
                if (zone.LeftChild != null)
                    result.AddRange(GetLeafZones(zone.LeftChild));
                if (zone.RightChild != null)
                    result.AddRange(GetLeafZones(zone.RightChild));
            }
            return result;
        }
    }
}
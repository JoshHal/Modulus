using System;
using System.Collections.Generic;
using System.Linq;
using QuikGraph;
using QuikGraph.Algorithms;

namespace Modulus.Core
{
    public class TopologicalSortingEngine
    {
        private RequirementManifest _manifest;
        private BidirectionalGraph<string, Edge<string>> _processDAG;
        private Dictionary<string, int> _rankAssignments;
        private List<string> _topologicalOrder;

        public TopologicalSortingEngine(RequirementManifest manifest)
        {
            _manifest = manifest ?? throw new ArgumentNullException(nameof(manifest));
            _processDAG = new BidirectionalGraph<string, Edge<string>>();
            _rankAssignments = new Dictionary<string, int>();
            _topologicalOrder = new List<string>();
        }

        public void ConstructDAG()
        {
            foreach (var machineId in _manifest.Machines.Keys)
            {
                _processDAG.AddVertex(machineId);
            }

            // Add edges from process flows
            foreach (var flow in _manifest.ProcessMatrix.Flows)
            {
                var edge = new Edge<string>(flow.SourceMachineId, flow.DestinationMachineId);
                if (!_processDAG.ContainsEdge(edge))
                {
                    _processDAG.AddEdge(edge);
                }
            }

            isAcyclic();
        }

        private void isAcyclic()
        {
            var cycleDetector = new CyclePairing<string, Edge<string>>();
            if (cycleDetector.IsCyclic(_processDAG))
            {
                throw new InvalidOperationException(
                    "Process graph contains cycles! Ensure no circular dependencies between machines.");
            }
        }

        public void AssignRanks()
        {
            if (_processDAG.VertexCount == 0)
                throw new InvalidOperationException("DAG must be constructed before rank assignment.");

            // Use topological sort to determine order
            var topOrder = new List<string>();
            if (!_processDAG.TopologicalSort(topOrder))
            {
                throw new InvalidOperationException("Topological sort failed (graph may contain cycles).");
            }

            _topologicalOrder = topOrder;
            foreach (var vertex in _processDAG.Vertices)
            {
                _rankAssignments[vertex] = 0;
            }
            foreach (var vertex in topOrder)
            {
                int maxIncomingRank = 0;

                // Get all incoming edges
                foreach (var inEdge in _processDAG.InEdges(vertex))
                {
                    if (_rankAssignments.ContainsKey(inEdge.Source))
                    {
                        maxIncomingRank = Math.Max(maxIncomingRank, _rankAssignments[inEdge.Source]);
                    }
                }

                _rankAssignments[vertex] = maxIncomingRank + 1;
            }
        }
        public ProcessTopology AnalyzeTopology()
        {
            if (_rankAssignments.Count == 0)
                throw new InvalidOperationException("Ranks must be assigned before topology analysis.");

            var topology = new ProcessTopology();

            foreach (var machine in _manifest.Machines.Values)
            {
                var inDegree = _processDAG.InDegree(machine.MachineId);
                var outDegree = _processDAG.OutDegree(machine.MachineId);
                var rank = _rankAssignments[machine.MachineId];

                // Source
                if (inDegree == 0 && outDegree > 0)
                {
                    topology.SourceMachines.Add(machine.MachineId, machine);
                    topology.MachineRanks[machine.MachineId] = 0;
                }
                // Sink
                else if (inDegree > 0 && outDegree == 0)
                {
                    topology.SinkMachines.Add(machine.MachineId, machine);
                    topology.MachineRanks[machine.MachineId] = rank;
                }
                // Intermediate
                else if (inDegree > 0 && outDegree > 0)
                {
                    topology.IntermediateMachines.Add(machine.MachineId, machine);
                    topology.MachineRanks[machine.MachineId] = rank;
                }
                // Isolated
                else
                {
                    topology.IsolatedMachines.Add(machine.MachineId, machine);
                    topology.MachineRanks[machine.MachineId] = 0;
                }
            }

            topology.ProcessFlows = _manifest.ProcessMatrix.Flows.ToList();
            topology.TopologicalOrder = new List<string>(_topologicalOrder);

            return topology;
        }
        public int GetMachineRank(string machineId)
        {
            return _rankAssignments.ContainsKey(machineId) ? _rankAssignments[machineId] : -1;
        }

        public List<string> GetDownstreamMachines(string machineId)
        {
            var downstream = new List<string>();
            if (_processDAG.TryGetOutEdges(machineId, out var edges))
            {
                downstream.AddRange(edges.Select(e => e.Target));
            }
            return downstream;
        }
        public List<string> GetUpstreamMachines(string machineId)
        {
            var upstream = new List<string>();
            if (_processDAG.TryGetInEdges(machineId, out var edges))
            {
                upstream.AddRange(edges.Select(e => e.Source));
            }
            return upstream;
        }
        public List<string> GetCriticalPath()
        {
            if (_topologicalOrder.Count == 0)
                throw new InvalidOperationException("Topological order not computed.");

            var sinkRank = _rankAssignments.Where(kvp => _processDAG.OutDegree(kvp.Key) == 0)
                                           .Max(kvp => kvp.Value);

            var criticalPath = new List<string>();
            var currentRank = sinkRank;

            while (currentRank >= 0)
            {
                var nodesAtRank = _rankAssignments.Where(kvp => kvp.Value == currentRank)
                                                   .Select(kvp => kvp.Key)
                                                   .ToList();
                
                if (nodesAtRank.Count > 0)
                {
                    criticalPath.Add(nodesAtRank.First());
                }

                currentRank--;
            }

            criticalPath.Reverse();
            return criticalPath;
        }
        public string GenerateReport()
        {
            var report = new System.Text.StringBuilder();
            report.AppendLine("=== MODULUS PHASE I: TOPOLOGICAL ANALYSIS REPORT ===\n");

            report.AppendLine("DAG Summary:");
            report.AppendLine($"  Total Machines: {_processDAG.VertexCount}");
            report.AppendLine($"  Total Connections: {_processDAG.EdgeCount}");
            report.AppendLine($"  Is Acyclic: ✓");

            report.AppendLine("\nRank Assignments:");
            foreach (var kvp in _rankAssignments.OrderBy(x => x.Value))
            {
                var machine = _manifest.GetMachine(kvp.Key);
                report.AppendLine($"  Rank {kvp.Value}: [{kvp.Key}] {machine.MachineName}");
            }

            report.AppendLine("\nTopological Order:");
            report.AppendLine($"  {string.Join(" → ", _topologicalOrder)}");

            report.AppendLine("\nMachine Dependencies:");
            foreach (var machineId in _processDAG.Vertices)
            {
                var upstream = GetUpstreamMachines(machineId);
                var downstream = GetDownstreamMachines(machineId);
                
                report.AppendLine($"\n  [{machineId}]");
                if (upstream.Count > 0)
                    report.AppendLine($"    Upstream: {string.Join(", ", upstream)}");
                if (downstream.Count > 0)
                    report.AppendLine($"    Downstream: {string.Join(", ", downstream)}");
                if (upstream.Count == 0 && downstream.Count == 0)
                    report.AppendLine($"    (Isolated)");
            }

            return report.ToString();
        }
    }

    public class ProcessTopology
    {
        public Dictionary<string, MachineObject> SourceMachines { get; set; } = new Dictionary<string, MachineObject>();
        public Dictionary<string, MachineObject> IntermediateMachines { get; set; } = new Dictionary<string, MachineObject>();
        public Dictionary<string, MachineObject> SinkMachines { get; set; } = new Dictionary<string, MachineObject>();
        public Dictionary<string, MachineObject> IsolatedMachines { get; set; } = new Dictionary<string, MachineObject>();

        public Dictionary<string, int> MachineRanks { get; set; } = new Dictionary<string, int>();
        public List<ProcessFlow> ProcessFlows { get; set; } = new List<ProcessFlow>();
        public List<string> TopologicalOrder { get; set; } = new List<string>();

        public int GetMaxRank()
        {
            return MachineRanks.Count > 0 ? MachineRanks.Values.Max() : 0;
        }

        public List<string> GetMachinesAtRank(int rank)
        {
            return MachineRanks.Where(kvp => kvp.Value == rank).Select(kvp => kvp.Key).ToList();
        }
    }
}
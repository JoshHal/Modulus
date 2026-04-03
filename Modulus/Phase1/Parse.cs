using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using Autodesk.AutoCAD.Geometry;

namespace Modulus.Core
{
    public class Parser
    {
        public static RequirementManifest LoadFromJson(string filePath)
        {
            if (!File.Exists(filePath))
                throw new FileNotFoundException($"Manifest file not found: {filePath}");

            try
            {
                string jsonContent = File.ReadAllText(filePath);
                var options = new JsonSerializerOptions
                {
                    PropertyNameCaseInsensitive = true,
                    WriteIndented = true
                };

                var manifest = JsonSerializer.Deserialize<RequirementManifest>(jsonContent, options)
                    ?? throw new InvalidOperationException("Failed to deserialize manifest.");

                manifest.Validate();
                return manifest;
            }
            catch (JsonException ex)
            {
                throw new InvalidOperationException($"Invalid JSON in manifest file: {ex.Message}", ex);
            }
        }
        public static void SaveToJson(RequirementManifest manifest, string filePath)
        {
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true,
                WriteIndented = true
            };

            string jsonContent = JsonSerializer.Serialize(manifest, options);
            File.WriteAllText(filePath, jsonContent);
        }
        public static RequirementManifest CreateDefaultManifest(
            double envelopeLength, double envelopeWidth, double envelopeHeight)
        {
            return new RequirementManifest
            {
                ManifestId = Guid.NewGuid().ToString(),
                ProjectName = "Default Factory Layout",
                CreatedDate = DateTime.Now,
                EnvelopeLength = envelopeLength,
                EnvelopeWidth = envelopeWidth,
                EnvelopeHeight = envelopeHeight,
                ProcessMatrix = new ProcessMatrix { ProcessId = "DefaultProcess" },
                Constraints = new ConstraintWeights()
            };
        }
    }
    public class ManifestBuilder
    {
        private RequirementManifest _manifest;

        public ManifestBuilder(string projectName, double length, double width, double height)
        {
            _manifest = new RequirementManifest
            {
                ManifestId = Guid.NewGuid().ToString(),
                ProjectName = projectName,
                CreatedDate = DateTime.Now,
                EnvelopeLength = length,
                EnvelopeWidth = width,
                EnvelopeHeight = height,
                ProcessMatrix = new ProcessMatrix { ProcessId = "Process_" + Guid.NewGuid().ToString() }
            };
        }

        public ManifestBuilder AddMachine(string machineId, string machineName, 
            double length, double width, double height, 
            double throughputKgHr, double powerKw)
        {
            var machine = new MachineObject
            {
                MachineId = machineId,
                MachineName = machineName,
                FootprintLength = length,
                FootprintWidth = width,
                FootprintHeight = height,
                ThroughputKgPerHour = throughputKgHr,
                PowerConsumptionKw = powerKw
            };

            _manifest.AddMachine(machine);
            return this;
        }

        public ManifestBuilder AddPort(string machineId, string portName, double x, double y, double z = 0)
        {
            var machine = _manifest.GetMachine(machineId);
            if (machine == null)
                throw new InvalidOperationException($"Machine {machineId} not found.");

            machine.Ports[portName] = new Point3d(x, y, z);
            return this;
        }

        public ManifestBuilder AddFlow(string sourceId, string destId, string sourcePort, string destPort,
            string materialType, double flowRateKgHr, MaterialPhase phase)
        {
            var flow = new ProcessFlow
            {
                FlowId = $"Flow_{Guid.NewGuid().ToString()}",
                SourceMachineId = sourceId,
                DestinationMachineId = destId,
                SourcePortName = sourcePort,
                DestinationPortName = destPort,
                MaterialType = materialType,
                FlowRateKgPerHour = flowRateKgHr,
                Phase = phase
            };

            _manifest.ProcessMatrix.AddFlow(flow);
            return this;
        }

        public ManifestBuilder SetConstraintWeights(double pipeLength = 0.3, double maintenance = 0.2,
            double energy = 0.25, double safety = 0.15, double floorSpace = 0.1)
        {
            _manifest.Constraints = new ConstraintWeights
            {
                PipeLengthWeight = pipeLength,
                MaintenanceAccessWeight = maintenance,
                EnergyEfficiencyWeight = energy,
                SafetyZoneWeight = safety,
                FloorSpaceUtilizationWeight = floorSpace
            };

            return this;
        }

        public RequirementManifest Build()
        {
            _manifest.Validate();
            return _manifest;
        }
    }

    // / <summary>
    // / Example JSON Manifest Schema (for documentation):
    // / 
    // / {
    // /   "manifestId": "manifest-001",
    // /   "projectName": "Beverage Plant Layout",
    // /   "createdDate": "2025-01-15T10:30:00Z",
    // /   "envelopeLength": 50000,
    // /   "envelopeWidth": 30000,
    // /   "envelopeHeight": 10000,
    // /   "machines": {
    // /     "MIXER_01": {
    // /       "machineId": "MIXER_01",
    // /       "machineName": "Industrial Mixer",
    // /       "throughputKgPerHour": 5000,
    // /       "powerConsumptionKw": 150,
    // /       "footprintLength": 2500,
    // /       "footprintWidth": 1800,
    // /       "footprintHeight": 2200,
    // /       "ports": {
    // /         "Inlet_1": [0, -900, -1100],
    // /         "Inlet_2": [0, 900, -1100],
    // /         "Outlet": [0, 0, 1100]
    // /       },
    // /       "type": "Intermediate",
    // /       "processingTimePerUnitSeconds": 45
    // /     }
    // /   },
    // /   "processMatrix": {
    // /     "processId": "process-001",
    // /     "flows": [
    // /       {
    // /         "flowId": "flow-001",
    // /         "sourceMachineId": "HOPPER_01",
    // /         "destinationMachineId": "MIXER_01",
    // /         "sourcePortName": "Outlet",
    // /         "destinationPortName": "Inlet_1",
    // /         "materialType": "Sugar",
    // /         "flowRateKgPerHour": 2500,
    // /         "phase": "Solid",
    // /         "requiresCooling": false,
    // /         "requiresHeating": false
    // /       }
    // /     ]
    // /   },
    // /   "constraints": {
    // /     "pipeLengthWeight": 0.3,
    // /     "maintenanceAccessWeight": 0.2,
    // /     "energyEfficiencyWeight": 0.25,
    // /     "safetyZoneWeight": 0.15,
    // /     "floorSpaceUtilizationWeight": 0.1
    // /   },
    // /   "restrictedZones": [
    // /     {
    // /       "zoneId": "zone-emergency-exit",
    // /       "minX": 48000,
    // /       "maxX": 50000,
    // /       "minY": 14000,
    // /       "maxY": 16000,
    // /       "reason": "Emergency Exit"
    // /     }
    // /   ]
    // / }
    // / </summary>
}
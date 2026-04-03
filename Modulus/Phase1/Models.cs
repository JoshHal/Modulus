using System;
using System.Collections.Generic;
using Autodesk.AutoCAD.Geometry;

namespace Modulus.Core
{
    public class MachineObject
    {
        public string MachineId { get; set; }
        public string MachineName { get; set; }

        // Functional Specifications
        public double ThroughputKgPerHour { get; set; }
        public double PowerConsumptionKw { get; set; }

        // Physical Specifications
        public double FootprintLength { get; set; }    // X dimension (mm)
        public double FootprintWidth { get; set; }     // Y dimension (mm)
        public double FootprintHeight { get; set; }    // Z dimension (mm)

        // 3D Model Information
        public string ModelFilePath { get; set; }      // Path to .dwg or block file
        public string BlockName { get; set; }          // Block name if embedded
        public double ModelScale { get; set; } = 1.0;  // Scale factor if needed

        // Port Information (inlet/outlet connections)
        public Dictionary<string, Point3d> Ports { get; set; } = new Dictionary<string, Point3d>();

        // Machine category for constraint evaluation
        public MachineType Type { get; set; }

        // Optional: Processing time per unit (seconds)
        public double ProcessingTimePerUnitSeconds { get; set; }

        /// <summary>
        /// Get the full path to the 3D model file.
        /// Supports both absolute paths and relative paths.
        /// </summary>
        public string GetModelPath(string baseDirectory = null)
        {
            if (string.IsNullOrEmpty(ModelFilePath))
                return null;

            // If absolute path, return as-is
            if (System.IO.Path.IsPathRooted(ModelFilePath))
                return ModelFilePath;

            // If relative path and base directory provided, combine
            if (!string.IsNullOrEmpty(baseDirectory))
                return System.IO.Path.Combine(baseDirectory, ModelFilePath);

            return ModelFilePath;
        }

        public override string ToString()
        {
            return string.Format("[{0}] {1} ({2}x{3}mm, {4}kg/hr)",
                MachineId, MachineName, FootprintLength, FootprintWidth, ThroughputKgPerHour);
        }
    }

    /// <summary>
    /// 3D Model file reference with metadata.
    /// </summary>
    public class ModelFileReference
    {
        public string MachineId { get; set; }
        public string FilePath { get; set; }           // Path to .dwg, .iges, .step, etc.
        public string BlockName { get; set; }          // Block name if using embedded blocks
        public ModelFileType FileType { get; set; }
        public double Scale { get; set; } = 1.0;
        public bool IsExternal { get; set; } = true;   // External file vs embedded block

        public override string ToString()
        {
            return string.Format("{0}: {1} ({2})", MachineId, FilePath, FileType);
        }
    }

    /// <summary>
    /// Supported 3D model file formats.
    /// </summary>
    public enum ModelFileType
    {
        DWG,        // AutoCAD drawing
        DWF,        // AutoCAD web format
        STEP,       // 3D CAD standard
        IGES,       // Initial Graphics Exchange Specification
        STL,        // Stereolithography (3D printing)
        SAT,        // ACIS SAT format
        Block       // Embedded AutoCAD block
    }

    /// <summary>
    /// Enhanced Requirement Manifest with 3D model information.
    /// </summary>
    public class RequirementManifest
    {
        public string ManifestId { get; set; }
        public string ProjectName { get; set; }
        public DateTime CreatedDate { get; set; }

        // Factory Floor Envelope
        public double EnvelopeLength { get; set; }  // X dimension (mm)
        public double EnvelopeWidth { get; set; }   // Y dimension (mm)
        public double EnvelopeHeight { get; set; }  // Z dimension (mm)

        // Machine inventory
        public Dictionary<string, MachineObject> Machines { get; set; } = new Dictionary<string, MachineObject>();

        // Process definition
        public ProcessMatrix ProcessMatrix { get; set; } = new ProcessMatrix();

        // Optimization preferences
        public ConstraintWeights Constraints { get; set; } = new ConstraintWeights();

        // Restricted zones (no placement)
        public List<RestrictedZone> RestrictedZones { get; set; } = new List<RestrictedZone>();

        // 3D Models Configuration
        public ModelLibrary ModelLibrary { get; set; } = new ModelLibrary();

        public void AddMachine(MachineObject machine)
        {
            if (machine == null) throw new ArgumentNullException(nameof(machine));
            Machines[machine.MachineId] = machine;
        }

        public MachineObject GetMachine(string machineId)
        {
            return Machines.ContainsKey(machineId) ? Machines[machineId] : null;
        }

        public void Validate()
        {
            if (EnvelopeLength <= 0 || EnvelopeWidth <= 0 || EnvelopeHeight <= 0)
                throw new InvalidOperationException("Envelope dimensions must be positive.");

            if (Machines.Count == 0)
                throw new InvalidOperationException("Manifest must contain at least one machine.");

            // Validate all flows reference existing machines
            foreach (var flow in ProcessMatrix.Flows)
            {
                if (!Machines.ContainsKey(flow.SourceMachineId))
                    throw new InvalidOperationException(string.Format(
                        "Flow references unknown source machine: {0}", flow.SourceMachineId));

                if (!Machines.ContainsKey(flow.DestinationMachineId))
                    throw new InvalidOperationException(string.Format(
                        "Flow references unknown destination machine: {0}", flow.DestinationMachineId));
            }

            // Validate 3D models exist (warnings only, not errors)
            foreach (var machine in Machines.Values)
            {
                if (string.IsNullOrEmpty(machine.ModelFilePath))
                {
                    // Optional: machines can render as simple boxes
                }
            }
        }
    }

    /// <summary>
    /// Library of 3D models and their configurations.
    /// Centralized management of all machine models.
    /// </summary>
    public class ModelLibrary
    {
        public string LibraryName { get; set; } = "Default Machine Library";
        public string BasePath { get; set; }     // Base directory for relative paths
        
        // Model configurations
        public Dictionary<string, ModelFileReference> Models { get; set; } = 
            new Dictionary<string, ModelFileReference>();

        // Color/material assignments
        public Dictionary<MachineType, ColorAssignment> TypeColors { get; set; } = 
            new Dictionary<MachineType, ColorAssignment>();

        public ModelLibrary()
        {
            // Initialize default colors
            TypeColors[MachineType.Source] = new ColorAssignment { ACI = 2, Name = "Yellow" };
            TypeColors[MachineType.Intermediate] = new ColorAssignment { ACI = 7, Name = "White" };
            TypeColors[MachineType.Sink] = new ColorAssignment { ACI = 4, Name = "Cyan" };
        }

        public void AddModel(string machineId, ModelFileReference reference)
        {
            Models[machineId] = reference;
        }

        public ModelFileReference GetModel(string machineId)
        {
            return Models.ContainsKey(machineId) ? Models[machineId] : null;
        }

        public string GetFullPath(string machineId)
        {
            var model = GetModel(machineId);
            if (model == null) return null;

            if (string.IsNullOrEmpty(model.FilePath))
                return null;

            // If absolute path, return as-is
            if (System.IO.Path.IsPathRooted(model.FilePath))
                return model.FilePath;

            // If relative path and base path provided, combine
            if (!string.IsNullOrEmpty(BasePath))
                return System.IO.Path.Combine(BasePath, model.FilePath);

            return model.FilePath;
        }
    }

    /// <summary>
    /// Color assignment for machine types in AutoCAD.
    /// </summary>
    public class ColorAssignment
    {
        public int ACI { get; set; }            // AutoCAD Color Index (1-256)
        public string Name { get; set; }        // Color name
        public int RGB { get; set; }            // RGB value if needed
    }

    /// <summary>
    /// Represents a restricted zone (no machine placement allowed).
    /// </summary>
    public class RestrictedZone
    {
        public string ZoneId { get; set; }
        public double MinX { get; set; }
        public double MaxX { get; set; }
        public double MinY { get; set; }
        public double MaxY { get; set; }
        public string Reason { get; set; }  // "Utility", "Emergency Exit", "Maintenance", etc.

        public bool ContainsPoint(double x, double y)
        {
            return x >= MinX && x <= MaxX && y >= MinY && y <= MaxY;
        }
    }

    /// <summary>
    /// Represents a material flow connection between two machines.
    /// </summary>
    public class ProcessFlow
    {
        public string FlowId { get; set; }
        public string SourceMachineId { get; set; }
        public string DestinationMachineId { get; set; }
        public string SourcePortName { get; set; }
        public string DestinationPortName { get; set; }

        // Material specifications
        public string MaterialType { get; set; }
        public double FlowRateKgPerHour { get; set; }

        // Flow properties
        public MaterialPhase Phase { get; set; }  // Solid, Liquid, Gas
        public bool RequiresCooling { get; set; }
        public bool RequiresHeating { get; set; }

        public override string ToString()
        {
            return string.Format("{0}->{1} ({2} @ {3}kg/hr)",
                SourceMachineId, DestinationMachineId, MaterialType, FlowRateKgPerHour);
        }
    }

    /// <summary>
    /// The Process Matrix: defines all material flows in the factory layout.
    /// </summary>
    public class ProcessMatrix
    {
        public string ProcessId { get; set; }
        public List<ProcessFlow> Flows { get; set; } = new List<ProcessFlow>();

        public void AddFlow(ProcessFlow flow)
        {
            if (flow == null) throw new ArgumentNullException(nameof(flow));
            Flows.Add(flow);
        }

        public List<ProcessFlow> GetFlowsFrom(string machineId)
        {
            return Flows.FindAll(f => f.SourceMachineId == machineId);
        }

        public List<ProcessFlow> GetFlowsTo(string machineId)
        {
            return Flows.FindAll(f => f.DestinationMachineId == machineId);
        }
    }

    /// <summary>
    /// Constraint weights for the layout optimization algorithm.
    /// Values represent importance (0.0 to 1.0, or sum to 100).
    /// </summary>
    public class ConstraintWeights
    {
        public double PipeLengthWeight { get; set; } = 0.3;
        public double MaintenanceAccessWeight { get; set; } = 0.2;
        public double EnergyEfficiencyWeight { get; set; } = 0.25;
        public double SafetyZoneWeight { get; set; } = 0.15;
        public double FloorSpaceUtilizationWeight { get; set; } = 0.1;

        public double GetNormalizedWeight(string constraintName)
        {
            double total = PipeLengthWeight + MaintenanceAccessWeight + EnergyEfficiencyWeight 
                         + SafetyZoneWeight + FloorSpaceUtilizationWeight;
            
            switch (constraintName)
            {
                case "PipeLength":
                    return PipeLengthWeight / total;
                case "MaintenanceAccess":
                    return MaintenanceAccessWeight / total;
                case "EnergyEfficiency":
                    return EnergyEfficiencyWeight / total;
                case "SafetyZone":
                    return SafetyZoneWeight / total;
                case "FloorSpaceUtilization":
                    return FloorSpaceUtilizationWeight / total;
                default:
                    return 0.0;
            }
        }
    }

    public enum MachineType
    {
        Source,         // Raw material input
        Intermediate,   // Processing/transformation
        Sink,           // Final output/packaging
        Utility         // Support (compressor, chiller, etc.)
    }

    public enum MaterialPhase
    {
        Solid,
        Liquid,
        Gas,
        Slurry
    }
}
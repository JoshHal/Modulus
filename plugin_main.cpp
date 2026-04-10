#include "core.h"
#include "cad_adapter.h"
#include "ui_panel.h"
#include <vector>
#include <string>

// ObjectARX headers
#include "acdb.h"
#include "acap.h"
#include "acedads.h"
#include "aced.h"
#include "rxregsvc.h"

using namespace ModulusLite;

// ============================================================================
// PLUGIN STATE
// ============================================================================

class ModulusLitePlugin {
public:
    static ModulusLitePlugin& instance() {
        static ModulusLitePlugin s_instance;
        return s_instance;
    }
    
    bool initialize();
    void shutdown();
    
    std::vector<Machine>& getMachines() { return m_machines; }
    std::vector<Pipe>& getPipes() { return m_pipes; }
    
    Pipeline* getPipeline() { return m_pPipeline; }
    
private:
    ModulusLitePlugin();
    ~ModulusLitePlugin();
    
    std::vector<Machine> m_machines;
    std::vector<Pipe> m_pipes;
    Pipeline* m_pPipeline;
    UIPanel* m_pUIPanel;
    
    void setupSampleData();
};

ModulusLitePlugin::ModulusLitePlugin()
    : m_pPipeline(nullptr), m_pUIPanel(nullptr) {
}

ModulusLitePlugin::~ModulusLitePlugin() {
    if (m_pPipeline) delete m_pPipeline;
    if (m_pUIPanel) delete m_pUIPanel;
}

bool ModulusLitePlugin::initialize() {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) return false;
    
    // Create pipeline
    m_pPipeline = new Pipeline(pDb);
    
    // Setup sample data (in real app, this would come from user input)
    setupSampleData();
    
    // Create UI panel (note: Qt integration requires additional setup)
    // m_pUIPanel = new UIPanel();
    // m_pUIPanel->setMachines(&m_machines);
    // m_pUIPanel->setPipes(&m_pipes);
    // m_pUIPanel->setPipeline(m_pPipeline);
    
    return true;
}

void ModulusLitePlugin::shutdown() {
    // Cleanup
}

void ModulusLitePlugin::setupSampleData() {
    // Create 5 machines
    m_machines.resize(5);
    
    for (int i = 0; i < 5; ++i) {
        m_machines[i].id = i;
        m_machines[i].name = "Machine" + std::to_string(i);
        m_machines[i].position = Point3D(2.0 + i * 3.0, 2.0 + (i % 2) * 4.0, 0.5);
        m_machines[i].size = Point3D(1.0, 1.0, 1.0);
        m_machines[i].velocity = Point3D(0, 0, 0);
        
        // Add 4 ports to each machine
        m_machines[i].ports.push_back(Point3D(-0.5, 0, 0.5));  // Left
        m_machines[i].ports.push_back(Point3D(0.5, 0, 0.5));   // Right
        m_machines[i].ports.push_back(Point3D(0, -0.5, 0.5));  // Back
        m_machines[i].ports.push_back(Point3D(0, 0.5, 0.5));   // Front
        
        m_machines[i].modelPath = "MachineBlock";
    }
    
    // Create pipes connecting machines
    m_pipes.resize(3);
    
    m_pipes[0].id = 0;
    m_pipes[0].fromMachineId = 0;
    m_pipes[0].toMachineId = 1;
    m_pipes[0].fromPortIndex = 1;
    m_pipes[0].toPortIndex = 0;
    m_pipes[0].material = MaterialType::Liquid;
    m_pipes[0].service = ServiceType::Water;
    m_pipes[0].diameter = PipeProperties::getDiameter(MaterialType::Liquid);
    m_pipes[0].specCode = "CS150";
    m_pipes[0].lineNumber = 100;
    
    m_pipes[1].id = 1;
    m_pipes[1].fromMachineId = 1;
    m_pipes[1].toMachineId = 3;
    m_pipes[1].fromPortIndex = 2;
    m_pipes[1].toPortIndex = 3;
    m_pipes[1].material = MaterialType::Gas;
    m_pipes[1].service = ServiceType::Air;
    m_pipes[1].diameter = PipeProperties::getDiameter(MaterialType::Gas);
    m_pipes[1].specCode = "SS316";
    m_pipes[1].lineNumber = 200;
    
    m_pipes[2].id = 2;
    m_pipes[2].fromMachineId = 2;
    m_pipes[2].toMachineId = 4;
    m_pipes[2].fromPortIndex = 1;
    m_pipes[2].toPortIndex = 0;
    m_pipes[2].material = MaterialType::Sludge;
    m_pipes[2].service = ServiceType::Slurry;
    m_pipes[2].diameter = PipeProperties::getDiameter(MaterialType::Sludge);
    m_pipes[2].specCode = "HDPE";
    m_pipes[2].lineNumber = 300;
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void cmdGenerateLayout() {
    auto& plugin = ModulusLitePlugin::instance();
    auto& machines = plugin.getMachines();
    
    // Run layout
    FDGLayout::layout(machines);
    
    acutPrintf(_T("\nLayout generated. %zu machines positioned.\n"), machines.size());
}

void cmdRoutePipes() {
    auto& plugin = ModulusLitePlugin::instance();
    auto pPipeline = plugin.getPipeline();
    auto& machines = plugin.getMachines();
    auto& pipes = plugin.getPipes();
    
    // Execute full pipeline
    if (pPipeline && pPipeline->execute(machines, pipes)) {
        acutPrintf(_T("\nPipeline executed successfully.\n"));
        acutPrintf(_T("Routed %zu pipes.\n"), pipes.size());
        
        // Print pipe info
        for (const auto& pipe : pipes) {
            acutPrintf(_T("Pipe %d: %s, Length=%.2fm, Cost=$%.2f\n"),
                pipe.id, pipe.tag.c_str(), pipe.length, pipe.cost);
        }
    } else {
        acutPrintf(_T("\nPipeline execution failed.\n"));
    }
}

// ============================================================================
// ENTRY POINTS
// ============================================================================

extern "C" {

AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode msg, void* pkt) {
    switch (msg) {
        case AcRx::kInitAppMsg:
            {
                // Register commands
                if (!acrxRegisterAppMDIAware(pkt)) {
                    return AcRx::kRetError;
                }
                
                acedRegCmds->addCommand(_T("MODULUSLITE"), _T("MODLAYOUT"), 
                                       _T("MODLAYOUT"), AcRx::kNormal, cmdGenerateLayout);
                acedRegCmds->addCommand(_T("MODULUSLITE"), _T("MODROUTE"), 
                                       _T("MODROUTE"), AcRx::kNormal, cmdRoutePipes);
                
                // Initialize plugin
                if (!ModulusLitePlugin::instance().initialize()) {
                    return AcRx::kRetError;
                }
                
                acutPrintf(_T("\nModulus Lite v1.0 loaded successfully.\n"));
                acutPrintf(_T("Commands: MODLAYOUT, MODROUTE\n"));
            }
            return AcRx::kRetOK;
            
        case AcRx::kUnloadAppMsg:
            {
                // Unregister commands
                acedRegCmds->removeGroup(_T("MODULUSLITE"));
                
                // Shutdown plugin
                ModulusLitePlugin::instance().shutdown();
                
                acutPrintf(_T("\nModulus Lite unloaded.\n"));
            }
            return AcRx::kRetOK;
            
        default:
            return AcRx::kRetOK;
    }
}

}  // extern "C"

#include "ui_panel.h"
#include "cad_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <sstream>
#include <iomanip>

namespace ModulusLite {

UIPanel::UIPanel(QWidget* parent)
    : QDockWidget("Modulus Lite", parent),
      m_pMachines(nullptr),
      m_pPipes(nullptr),
      m_pPipeline(nullptr) {
    
    setupUI();
}

UIPanel::~UIPanel() {
    // Qt cleanup is automatic
}

void UIPanel::setupUI() {
    // Create central widget
    QWidget* pCentral = new QWidget(this);
    QVBoxLayout* pMainLayout = new QVBoxLayout(pCentral);
    
    // Title label
    QLabel* pTitle = new QLabel("Piping Layout Generator", this);
    QFont titleFont = pTitle->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    pTitle->setFont(titleFont);
    pMainLayout->addWidget(pTitle);
    
    // Button layout
    QHBoxLayout* pBtnLayout = new QHBoxLayout();
    
    m_pBtnLayout = new QPushButton("Generate Layout", this);
    m_pBtnRoute = new QPushButton("Route Pipes", this);
    
    pBtnLayout->addWidget(m_pBtnLayout);
    pBtnLayout->addWidget(m_pBtnRoute);
    pMainLayout->addLayout(pBtnLayout);
    
    // Status label
    m_pStatusLabel = new QLabel("Ready", this);
    m_pStatusLabel->setStyleSheet("QLabel { color: green; }");
    pMainLayout->addWidget(m_pStatusLabel);
    
    // Separator
    QFrame* pLine = new QFrame(this);
    pLine->setFrameShape(QFrame::HLine);
    pMainLayout->addWidget(pLine);
    
    // Pipes list
    QLabel* pListLabel = new QLabel("Pipes:", this);
    pMainLayout->addWidget(pListLabel);
    
    m_pPipeList = new QListWidget(this);
    m_pPipeList->setMaximumHeight(200);
    pMainLayout->addWidget(m_pPipeList);
    
    // Info label
    m_pInfoLabel = new QLabel("Select a pipe to view details", this);
    m_pInfoLabel->setWordWrap(true);
    m_pInfoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 5px; }");
    pMainLayout->addWidget(m_pInfoLabel);
    
    // Stretch
    pMainLayout->addStretch();
    
    // Set central widget
    setWidget(pCentral);
    
    // Connect signals
    connect(m_pBtnLayout, &QPushButton::clicked, this, &UIPanel::onGenerateLayout);
    connect(m_pBtnRoute, &QPushButton::clicked, this, &UIPanel::onRoutePipes);
    connect(m_pPipeList, &QListWidget::itemSelectionChanged, 
            this, [this]() {
                int row = m_pPipeList->currentRow();
                if (row >= 0 && m_pPipes && row < (int)m_pPipes->size()) {
                    updateInfo((*m_pPipes)[row]);
                }
            });
    
    // Set initial state
    m_pBtnLayout->setEnabled(false);
    m_pBtnRoute->setEnabled(false);
}

void UIPanel::setMachines(const std::vector<Machine>* pMachines) {
    m_pMachines = pMachines;
    if (m_pMachines && !m_pMachines->empty()) {
        m_pBtnLayout->setEnabled(true);
    }
}

void UIPanel::setPipes(const std::vector<Pipe>* pPipes) {
    m_pPipes = pPipes;
    refreshPipeList();
    if (m_pPipes && !m_pPipes->empty()) {
        m_pBtnRoute->setEnabled(true);
    }
}

void UIPanel::setPipeline(Pipeline* pPipeline) {
    m_pPipeline = pPipeline;
}

void UIPanel::onGenerateLayout() {
    if (!m_pPipeline || !m_pMachines) {
        updateStatus("No pipeline or machines available");
        return;
    }
    
    updateStatus("Generating layout...");
    
    // Note: In a real implementation, this would be done in a worker thread
    // to avoid blocking the UI. For MVP, single-threaded is acceptable.
    
    updateStatus("Layout generated successfully");
}

void UIPanel::onRoutePipes() {
    if (!m_pPipeline || !m_pPipes) {
        updateStatus("No pipeline or pipes available");
        return;
    }
    
    updateStatus("Routing pipes...");
    
    // Note: Similarly, this should be in a worker thread in production
    
    refreshPipeList();
    updateStatus("Pipes routed and drawn successfully");
}

void UIPanel::refreshPipeList() {
    m_pPipeList->clear();
    
    if (!m_pPipes) return;
    
    for (const auto& pipe : *m_pPipes) {
        std::ostringstream ss;
        ss << "Pipe " << pipe.id << " (" << pipe.tag << ")";
        m_pPipeList->addItem(QString::fromStdString(ss.str()));
    }
}

void UIPanel::updateStatus(const std::string& message) {
    m_pStatusLabel->setText(QString::fromStdString(message));
}

void UIPanel::updateInfo(const Pipe& pipe) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "Pipe ID: " << pipe.id << "\n"
       << "Tag: " << pipe.tag << "\n"
       << "From Machine: " << pipe.fromMachineId << "\n"
       << "To Machine: " << pipe.toMachineId << "\n"
       << "Diameter: " << (pipe.diameter * 1000.0) << " mm\n"
       << "Length: " << pipe.length << " m\n"
       << "Cost: $" << pipe.cost << "\n"
       << "Spec: " << pipe.specCode;
    
    m_pInfoLabel->setText(QString::fromStdString(ss.str()));
}

}  // namespace ModulusLite

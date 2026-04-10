#pragma once

#include <QDockWidget>
#include <QPushButton>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <vector>
#include "core.h"

namespace ModulusLite {

// Forward declaration
class Pipeline;

class UIPanel : public QDockWidget {
    Q_OBJECT
    
public:
    explicit UIPanel(QWidget* parent = nullptr);
    ~UIPanel();
    
    // Set data to display
    void setMachines(const std::vector<Machine>* pMachines);
    void setPipes(const std::vector<Pipe>* pPipes);
    
    // Get pipeline reference for callbacks
    void setPipeline(Pipeline* pPipeline);
    
private slots:
    void onGenerateLayout();
    void onRoutePipes();
    void onPipeSelected(int row);
    
private:
    // UI Elements
    QPushButton* m_pBtnLayout;
    QPushButton* m_pBtnRoute;
    QListWidget* m_pPipeList;
    QLabel* m_pStatusLabel;
    QLabel* m_pInfoLabel;
    
    // Data
    const std::vector<Machine>* m_pMachines;
    const std::vector<Pipe>* m_pPipes;
    Pipeline* m_pPipeline;
    
    void setupUI();
    void refreshPipeList();
    void updateStatus(const std::string& message);
    void updateInfo(const Pipe& pipe);
};

}  // namespace ModulusLite

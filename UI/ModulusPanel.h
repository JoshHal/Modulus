#pragma once
#include <QDockWidget>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include "../ModulusController.h"

// ─── ModulusPanel ────────────────────────────────────────────────────────────
//
// Qt dockable panel.  Two buttons drive the full pipeline:
//
//   [Generate Layout]  → calls ModulusController::generateLayout()
//                        updates machine positions
//
//   [Route & Draw]     → calls ModulusController::routeAndDraw()
//                        writes DWG entities, populates pipe list
//
// The pipe list shows: Tag | Length | Cost for each routed pipe.
//
// UI thread safety:
//   Both button slots run on the Qt main thread.
//   AutoCAD DB writes (inside routeAndDraw) are therefore also on the main
//   thread — compliant with ObjectARX threading rules.
// ─────────────────────────────────────────────────────────────────────────────

class ModulusPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit ModulusPanel(QWidget* parent = nullptr);

private slots:
    void onGenerateLayout();
    void onRouteAndDraw();
    void onPipeSelected(QListWidgetItem* item);

private:
    void rebuildPipeList();
    void setStatus(const QString& msg, bool error = false);

    // Controller (owns machine + pipe data)
    ModulusController controller_;

    // Widgets
    QPushButton*   btnGenerate_;
    QPushButton*   btnRoute_;
    QListWidget*   pipeList_;
    QLabel*        statusLabel_;
    QLabel*        totalCostLabel_;
};
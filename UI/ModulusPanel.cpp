#include "ModulusPanel.h"
#include <QMessageBox>
#include <QApplication>
#include <sstream>
#include <iomanip>
#include <numeric>

// ─── Constructor ──────────────────────────────────────────────────────────────

ModulusPanel::ModulusPanel(QWidget* parent)
    : QDockWidget("Modulus Lite", parent)
{
    setMinimumWidth(260);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    // ── Central widget ─────────────────────────────────────────────────────

    QWidget*     central = new QWidget(this);
    QVBoxLayout* vlay    = new QVBoxLayout(central);
    vlay->setContentsMargins(8, 8, 8, 8);
    vlay->setSpacing(6);

    // ── Header label ──────────────────────────────────────────────────────

    QLabel* header = new QLabel("Mini Plant Layout Generator", central);
    header->setStyleSheet("font-weight: bold; font-size: 11pt; padding-bottom: 4px;");
    vlay->addWidget(header);

    // ── Button row ────────────────────────────────────────────────────────

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);

    btnGenerate_ = new QPushButton("Generate Layout", central);
    btnGenerate_->setToolTip("Run Force-Directed Graph to position machines");
    btnRoute_    = new QPushButton("Route && Draw", central);
    btnRoute_->setToolTip("Route pipes with A* and write all DWG entities");
    btnRoute_->setEnabled(false);

    btnRow->addWidget(btnGenerate_);
    btnRow->addWidget(btnRoute_);
    vlay->addLayout(btnRow);

    // ── Pipe list ─────────────────────────────────────────────────────────

    QLabel* pipeHeader = new QLabel("Routed Pipes", central);
    pipeHeader->setStyleSheet("font-weight: bold; margin-top: 4px;");
    vlay->addWidget(pipeHeader);

    pipeList_ = new QListWidget(central);
    pipeList_->setAlternatingRowColors(true);
    pipeList_->setSelectionMode(QAbstractItemView::SingleSelection);
    vlay->addWidget(pipeList_, 1);  // stretch factor 1 → fills remaining height

    // ── Total cost ────────────────────────────────────────────────────────

    totalCostLabel_ = new QLabel("Total cost: —", central);
    totalCostLabel_->setStyleSheet("font-weight: bold;");
    vlay->addWidget(totalCostLabel_);

    // ── Status bar ────────────────────────────────────────────────────────

    statusLabel_ = new QLabel("Ready. Load sample scene or add machines.", central);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet("font-size: 9pt; color: grey;");
    vlay->addWidget(statusLabel_);

    setWidget(central);

    // ── Connections ───────────────────────────────────────────────────────

    connect(btnGenerate_, &QPushButton::clicked, this, &ModulusPanel::onGenerateLayout);
    connect(btnRoute_,    &QPushButton::clicked, this, &ModulusPanel::onRouteAndDraw);
    connect(pipeList_,    &QListWidget::itemClicked,
            this,         &ModulusPanel::onPipeSelected);

    // Pre-load sample scene so the user can immediately click Generate Layout
    controller_.buildSampleScene();
    setStatus(QString("Sample scene loaded: %1 machines, %2 pipes.")
              .arg(controller_.getMachines().size())
              .arg(controller_.getPipes().size()));
}

// ─── Slots ────────────────────────────────────────────────────────────────────

void ModulusPanel::onGenerateLayout() {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    setStatus("Running FDG layout…");

    try {
        controller_.generateLayout();
        btnRoute_->setEnabled(true);
        setStatus(QString("Layout complete. %1 machines positioned. Click 'Route & Draw'.")
                  .arg(controller_.getMachines().size()));
    } catch (const std::exception& ex) {
        setStatus(QString("Layout error: %1").arg(ex.what()), true);
    }

    QApplication::restoreOverrideCursor();
}

void ModulusPanel::onRouteAndDraw() {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    setStatus("Routing pipes and writing DWG entities…");

    try {
        controller_.routeAndDraw();
        rebuildPipeList();
        setStatus("Done. AutoCAD view updated.");
    } catch (const std::exception& ex) {
        setStatus(QString("Route error: %1").arg(ex.what()), true);
    }

    QApplication::restoreOverrideCursor();
}

void ModulusPanel::onPipeSelected(QListWidgetItem* item) {
    if (!item) return;
    // Row index maps directly to pipe index
    int idx = pipeList_->row(item);
    const auto& pipes = controller_.getPipes();
    if (idx < 0 || idx >= static_cast<int>(pipes.size())) return;

    const Pipe& p = pipes[idx];
    std::ostringstream info;
    info << "Pipe " << p.id << ": " << p.tag << "\n"
         << "  Length : " << std::fixed << std::setprecision(2) << p.length << " m\n"
         << "  Cost   : $" << std::fixed << std::setprecision(2) << p.cost << "\n"
         << "  Diam   : " << static_cast<int>(p.diameter * 1000) << " mm\n"
         << "  Waypts : " << p.path.size();
    setStatus(QString::fromStdString(info.str()));
}

// ─── Private helpers ──────────────────────────────────────────────────────────

void ModulusPanel::rebuildPipeList() {
    pipeList_->clear();
    const auto& pipes = controller_.getPipes();
    double totalCost  = 0.0;

    for (const auto& p : pipes) {
        std::ostringstream row;
        row << p.tag
            << "   " << std::fixed << std::setprecision(1) << p.length << " m"
            << "   $" << std::fixed << std::setprecision(0) << p.cost;
        pipeList_->addItem(QString::fromStdString(row.str()));
        totalCost += p.cost;
    }

    std::ostringstream totalStr;
    totalStr << "Total cost: $" << std::fixed << std::setprecision(2) << totalCost;
    totalCostLabel_->setText(QString::fromStdString(totalStr.str()));
}

void ModulusPanel::setStatus(const QString& msg, bool error) {
    statusLabel_->setText(msg);
    statusLabel_->setStyleSheet(
        error ? "font-size: 9pt; color: #c0392b;"
              : "font-size: 9pt; color: grey;");
}
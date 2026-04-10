//
// ModulusLite.cpp
// ObjectARX plugin entry point (macOS / ARX Mac compatible subset)
//
// Registers:
//   • MODULUS command  — opens / brings to front the Qt panel
//   • acrxEntryPoint   — standard ARX lifecycle hooks
//
// The Qt panel is created once and kept alive for the session.
// All DWG writes happen from the Qt main thread (= AutoCAD main thread on Mac).
//

#include <rxregsvc.h>
#include <aced.h>
#include <acdb.h>
#include "UI/ModulusPanel.h"

// ─── Global panel pointer ─────────────────────────────────────────────────────

static ModulusPanel* gPanel = nullptr;

// ─── Command: MODULUS ─────────────────────────────────────────────────────────

static void cmdModulus() {
    if (!gPanel) {
        // Retrieve the AutoCAD main window as parent
        QWidget* acadWin = acedGetAcadFrame();
        gPanel = new ModulusPanel(acadWin);

        // Dock on the right side of the AutoCAD window
        if (auto* mw = qobject_cast<QMainWindow*>(acadWin)) {
            mw->addDockWidget(Qt::RightDockWidgetArea, gPanel);
        } else {
            gPanel->setFloating(true);
            gPanel->resize(280, 480);
            gPanel->show();
        }
    }
    gPanel->setVisible(true);
    gPanel->raise();
}

// ─── ARX entry point ─────────────────────────────────────────────────────────

extern "C" AcRx::AppRetCode
acrxEntryPoint(AcRx::AppMsgCode msg, void* /*pkt*/) {

    switch (msg) {

    case AcRx::kInitAppMsg:
        // Prevent ARX from unloading the plugin while it is in use
        acrxDynamicLinker->unlockApplication(nullptr);
        acrxDynamicLinker->registerAppMDIAware(nullptr);

        // Register the MODULUS command
        acedRegCmds->addCommand(
            L"MODULUS_CMDS",    // command group
            L"MODULUS",         // global name
            L"MODULUS",         // local name (same for English)
            ACRX_CMD_MODAL,     // flags: runs modally so DB is consistent
            cmdModulus);

        acutPrintf(L"\nModulus Lite loaded. Type MODULUS to open the panel.\n");
        break;

    case AcRx::kUnloadAppMsg:
        // Remove commands and destroy the panel
        acedRegCmds->removeGroup(L"MODULUS_CMDS");
        delete gPanel;
        gPanel = nullptr;
        break;

    default:
        break;
    }

    return AcRx::kRetOK;
}
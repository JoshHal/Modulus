#if defined(__APPLE__)
  #include <CoreFoundation/CoreFoundation.h>
  typedef void* HWND;
  typedef void* HDC;
  typedef void* HINSTANCE;
  typedef void* HBITMAP;
  // add others as the compiler complains
#endif

#include "aced.h"
#include "rxregsvc.h"
#include "rxvar.h"

// Forward declarations
void initApp();
void unloadApp();

// Simple command function
void HelloWorldCommand()
{
    acutPrintf(ACRX_T("\n*** Hello from MyExtension! ***\n"));
    acutPrintf(ACRX_T("This is a test ARX module.\n"));
}

// Application initialization
void initApp()
{
    // Register the hello world command
    acedRegCmds->addCommand(ACRX_T("MYEXT"), ACRX_T("HELLOWORLD"), ACRX_T("HELLOWORLD"), ACRX_CMD_TRANSPARENT, HelloWorldCommand);
    acutPrintf(ACRX_T("\nMyExtension loaded successfully!\n"));
    acutPrintf(ACRX_T("Type 'HELLOWORLD' to test the command.\n"));
}

// Application cleanup
void unloadApp()
{
    // Remove all registered commands
    acedRegCmds->removeGroup(ACRX_T("MYEXT"));
    acutPrintf(ACRX_T("\nMyExtension unloaded.\n"));
}

// ARX Entry Point - required
extern "C" __attribute__((visibility("default")))
AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode msg, void* pkt)
{
    switch(msg) {
        case AcRx::kInitAppMsg:
            initApp();
            break;
        case AcRx::kUnloadAppMsg:
            unloadApp();
            break;
        default:
            break;
    }
    return AcRx::kRetOK;
}

extern "C" __attribute__((visibility("default")))
AcRx::AppRetCode acrxUnloadModule(void)
{
    return AcRx::kRetOK;
}

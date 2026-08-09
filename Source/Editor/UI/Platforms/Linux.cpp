#include <SDL2/SDL.h>

#include <chrono>
#include <thread>

#include <signal.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Diagnostics.h>

#include <Studio/Impl.hpp>

extern "C" {
    #include "Common.h"
}

#include <UI/Graphics/Renderer.hpp>
#include <UI/System/Application.hpp>

IMenu* IMenu_Create() {
    return NULL;
}
int IMenu_AddItem(IMenu* menu, const char* title, void (*action)(), int shortcut, int enabled, int type, int altShortcut) {
    return -1;
}
int IMenu_AddSubmenu(IMenu* menu, IMenu* submenu, const char* title, int altShortcut) {
    return -1;
}
int IMenu_AddSeparator(IMenu* menu) {
    return -1;
}
void IMenu_EditItem(IMenu* menu, int index, const char* title, void (*action)(void), int shortcut, int enabled, int type) { }
void IMenu_ClearItems(IMenu* menu) { }
void IMenu_SetAppMenu(IMenu* menu) { }

void IMenu_SetAppleMenu(struct IMenu* menu) { }
void IMenu_SetWindowMenu(struct IMenu* menu) { }
void IMenu_SetHelpMenu(struct IMenu* menu) { }

void IMenu_Dispose(struct IMenu* menu) { }

void IMenu_Init() { }

namespace UI::Graphics::Renderer {
    void Sleep(double seconds) {
        Platform_Sleep(seconds);
    }
}

namespace UI::SystemDialog {
    bool StartProcess(const char* appPath, const char* cmd, const char* startDir) {
        Diagnostics::SetError("StartProcess unsupported in this system!");
        return true;
    }
}

namespace GameLinker {
    void LinkExternalGameLogic(LinkData* linkData, const char* projectFolder) {
        Diagnostics::SetError("LinkExternalGameLogic unsupported in this system!");
    }
}

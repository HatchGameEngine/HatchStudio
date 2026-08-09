#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <math.h>
#include <chrono>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Diagnostics.h>

#include <Studio/Impl.hpp>
extern "C" {
    #include "Common.h"
}

#include <UI/Graphics/Renderer.hpp>
#include <UI/System/SystemDialog.hpp>

#include <fstream>

// IMenu implementation
struct Win32_IMenuItem {
    char* Title;
    void (*Action)();
    const char* Shortcut;
    UINT Index;
};
struct Win32_IMenu {
    HMENU Handle;
    UINT Index;

    Win32_IMenu* PrevMenu;

    Win32_IMenuItem* Items;
    int Count;
    int Capacity;
};

Win32_IMenu* MenuHead = NULL;

void ShortcutToAppendString(char* out, int shortcut) {
    char* str = NULL;
    if ((shortcut & 0xFF) != 0) {
        str = (char*)malloc(24);
        if (str == NULL)
            return;

        str[0] = 0;

        strcat(str, "\t");
        if ((shortcut & SM_CONTROL) != 0)
            strcat(str, "Ctrl+");
        if ((shortcut & SM_ALT) != 0)
            strcat(str, "Alt+");
        if ((shortcut & SM_SHIFT) != 0)
            strcat(str, "Shift+");

        char shortcutString[2];
        shortcutString[0] = (shortcut & 0xFF);
        shortcutString[1] = 0;

        if (!isupper(shortcutString[0]))
            shortcutString[0] = toupper(shortcutString[0]);

        strcat(str, shortcutString);

        strcat(out, str);
        free(str);
    }
}
void PrintLastError() {
    DWORD hresult = GetLastError();
    LPSTR errorText = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, hresult,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&errorText, 0, NULL);
    if (errorText != NULL) {
        printf("err: %s", errorText);
        LocalFree(errorText);
        errorText = NULL;
    }
}

Win32_IMenuItem* Win32_IMenu_Add(Win32_IMenu* win32_menu) {
    if (win32_menu->Items == NULL)
        return NULL;

    if (win32_menu->Count + 1 >= win32_menu->Capacity) {
        win32_menu->Capacity *= 2;

        Win32_IMenuItem* newItems = (Win32_IMenuItem*)realloc(win32_menu->Items, win32_menu->Capacity * sizeof(Win32_IMenuItem));
        if (newItems == NULL)
            return NULL;

        win32_menu->Items = newItems;
    }

    return &win32_menu->Items[win32_menu->Count++];
}

IMenu* IMenu_Create() {
    IMenu* menu = (IMenu*)calloc(1, sizeof(IMenu));
    if (menu == NULL)
        return NULL;

    Win32_IMenu* win32_menu = (Win32_IMenu*)calloc(1, sizeof(Win32_IMenu));
    if (win32_menu == NULL)
        return NULL;

    win32_menu->Handle = CreateMenu();
    if (!win32_menu->Handle) {
        PrintLastError();
        return NULL;
    }

    // Allocate menu items
    win32_menu->Count = 0;
    win32_menu->Capacity = 4;
    win32_menu->Items = (Win32_IMenuItem*)calloc(win32_menu->Capacity, sizeof(Win32_IMenuItem));
    if (win32_menu->Items == NULL)
        return NULL;

    // Add to menu chain
    win32_menu->Index = MenuHead == NULL ? 0 : MenuHead->Index + 1;
    win32_menu->PrevMenu = MenuHead;
    MenuHead = win32_menu;

    menu->Data = win32_menu;
    return menu;
}
int IMenu_AddItem(IMenu* menu, const char* title, void (*action)(), int shortcut, int enabled, int type, int altShortcut) {
    Win32_IMenu* win32_menu = (Win32_IMenu*)menu->Data;

    char* itemTitle = (char*)malloc(48);
    if (itemTitle == NULL)
        return -1;

    strcpy(itemTitle, title);
    ShortcutToAppendString(itemTitle, shortcut);

    int index = win32_menu->Count;

    Win32_IMenuItem* item = Win32_IMenu_Add(win32_menu);
    item->Title = itemTitle;
    item->Action = action;
    item->Shortcut = NULL;

    UINT appearanceFlags = MF_STRING;
    if (!enabled)
        appearanceFlags |= MF_GRAYED; // Disables the menu item and grays it so that it cannot be selected.

    if (AppendMenuA(win32_menu->Handle, appearanceFlags, (UINT_PTR)(win32_menu->Index | (index << 8)), itemTitle) == 0) {
        PrintLastError();
        return -1;
    }

    MENUITEMINFOA menuInfo;
    memset(&menuInfo, 0, sizeof(menuInfo));
    menuInfo.cbSize = sizeof(menuInfo);

    switch (type) {
    case IT_RADIO_UNCHECKED:
        menuInfo.fType = MFT_RADIOCHECK;
    case IT_CHECKMARK_UNCHECKED:
        menuInfo.fMask = MIIM_FTYPE | MIIM_STATE;
        menuInfo.fState = MFS_UNCHECKED;
        if (!enabled) menuInfo.fState |= MFS_DISABLED;
        if (SetMenuItemInfoA(win32_menu->Handle, index, true, &menuInfo) == 0) {
            PrintLastError();
            return -1;
        }
        break;
    case IT_RADIO_CHECKED:
        menuInfo.fType = MFT_RADIOCHECK;
    case IT_CHECKMARK_CHECKED:
        menuInfo.fMask = MIIM_FTYPE | MIIM_STATE;
        menuInfo.fState = MFS_CHECKED;
        if (!enabled) menuInfo.fState |= MFS_DISABLED;
        if (SetMenuItemInfoA(win32_menu->Handle, index, true, &menuInfo) == 0) {
            PrintLastError();
            return -1;
        }
        break;
    }

    return index;
}
int IMenu_AddSubmenu(IMenu* menu, IMenu* submenu, const char* title, int altShortcut) {
    Win32_IMenu* win32_menu = (Win32_IMenu*)menu->Data;
    Win32_IMenu* win32_submenu = (Win32_IMenu*)submenu->Data;

    int index = win32_menu->Count;

    Win32_IMenuItem* item = Win32_IMenu_Add(win32_menu);
    item->Title = NULL;
    item->Action = NULL;
    item->Shortcut = NULL;

    if (AppendMenuA(win32_menu->Handle, MF_POPUP, (UINT_PTR)win32_submenu->Handle, title) == 0) {
        PrintLastError();
        return -1;
    }
    return index;
}
int IMenu_AddSeparator(IMenu* menu) {
    Win32_IMenu* win32_menu = (Win32_IMenu*)menu->Data;

    int index = win32_menu->Count;

    Win32_IMenuItem* item = Win32_IMenu_Add(win32_menu);
    item->Title = NULL;
    item->Action = NULL;
    item->Shortcut = NULL;

    UINT uint;
    if (AppendMenuA(win32_menu->Handle, MF_SEPARATOR, (UINT_PTR)&uint, NULL) == 0) {
        PrintLastError();
        return -1;
    }
    return index;
}
void IMenu_EditItem(IMenu* menu, int index, const char* title, void (*action)(void), int shortcut, int enabled, int type) {
    Win32_IMenu* win32_menu = (Win32_IMenu*)menu->Data;
    if (win32_menu->Items == NULL ||
        index >= win32_menu->Count ||
        index < 0)
        return;

    char* itemTitle = (char*)malloc(48);
    if (itemTitle == NULL)
        return;

    strcpy(itemTitle, title);
    ShortcutToAppendString(itemTitle, shortcut);

    Win32_IMenuItem* item = &win32_menu->Items[index];
    if (item->Title) free(item->Title);
    item->Title = itemTitle;
    item->Action = action;
    item->Shortcut = NULL;

    UINT appearanceFlags = MF_BYCOMMAND | MF_STRING;
    if (!enabled)
        appearanceFlags |= MF_GRAYED; // Disables the menu item and grays it so that it cannot be selected.

    if (ModifyMenuA(win32_menu->Handle, win32_menu->Index | (index << 8),
            appearanceFlags, (UINT_PTR)(win32_menu->Index | (index << 8)), itemTitle) == 0) {
        PrintLastError();
        return;
    }

    MENUITEMINFOA menuInfo;
    memset(&menuInfo, 0, sizeof(menuInfo));
    menuInfo.cbSize = sizeof(menuInfo);

    switch (type) {
    case IT_RADIO_UNCHECKED:
        menuInfo.fType = MFT_RADIOCHECK;
    case IT_CHECKMARK_UNCHECKED:
        menuInfo.fMask = MIIM_FTYPE | MIIM_STATE;
        menuInfo.fState = MFS_UNCHECKED;
        if (!enabled) menuInfo.fState |= MFS_DISABLED;
        if (SetMenuItemInfoA(win32_menu->Handle, index, true, &menuInfo) == 0) {
            PrintLastError();
            return;
        }
        break;
    case IT_RADIO_CHECKED:
        menuInfo.fType = MFT_RADIOCHECK;
    case IT_CHECKMARK_CHECKED:
        menuInfo.fMask = MIIM_FTYPE | MIIM_STATE;
        menuInfo.fState = MFS_CHECKED;
        if (!enabled) menuInfo.fState |= MFS_DISABLED;
        if (SetMenuItemInfoA(win32_menu->Handle, index, true, &menuInfo) == 0) {
            PrintLastError();
            return;
        }
        break;
    }
}
void IMenu_ClearItems(IMenu* menu) {
    Win32_IMenu* win32_menu = (Win32_IMenu*)menu->Data;
    if (win32_menu->Items == NULL)
        return;

    for (int i = win32_menu->Count - 1; i >= 0; i--) {
        Win32_IMenuItem* item = &win32_menu->Items[i];
        if (item->Title != NULL)
            free(item->Title);

        if (RemoveMenu(win32_menu->Handle, i, MF_BYPOSITION) == 0) {
            printf("RemoveMenu ");
            PrintLastError();
            return;
        }
    }

    win32_menu->Count = 0;
}
void IMenu_SetAppMenu(IMenu* menu) {
    // Get WIN32 Window Handle
    SDL_SysWMinfo sysInfo;
    SDL_VERSION(&sysInfo.version);
    SDL_GetWindowWMInfo(UI::Graphics::Renderer::Window, &sysInfo);

    HWND hwnd = sysInfo.info.win.window;
    if (SetMenu(hwnd, ((Win32_IMenu*)menu->Data)->Handle) == 0) {
        PrintLastError();
    }
}

void IMenu_SetAppleMenu(struct IMenu* menu) { }
void IMenu_SetWindowMenu(struct IMenu* menu) { }
void IMenu_SetHelpMenu(struct IMenu* menu) { }

void IMenu_Dispose(struct IMenu* menu) {
    Win32_IMenu* win32_menu = (Win32_IMenu*)menu->Data;

    // Remove from menu chain
    if (MenuHead != win32_menu) {
        // Get the node right ahead of the chosen node
        Win32_IMenu* ahead_menu = MenuHead;
        while (ahead_menu->PrevMenu != win32_menu) {
            ahead_menu = ahead_menu->PrevMenu;
        }

        ahead_menu->PrevMenu = win32_menu->PrevMenu;
    }
    else {
        MenuHead = win32_menu->PrevMenu;
    }

    // Free menu items
    if (win32_menu->Items != NULL)
        free(win32_menu->Items);

    // Destroy Win32 menu
    if (DestroyMenu(win32_menu->Handle) == 0) {
        PrintLastError();
    }

    // Free Win32 IMenu struct
    free(win32_menu);

    // Free IMenu
    free(menu);
}

void WindowsMessageHook(void* userdata, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COMMAND) {
        int menuID = wParam & 0xFF;
        int itemID = wParam >> 8;

        Win32_IMenu* win32_menu = MenuHead;
        while (win32_menu) {
            if (win32_menu->Index == menuID) {
                Win32_IMenuItem* item = &win32_menu->Items[itemID];
                if (item->Action)
                    item->Action();
                break;
            }
            win32_menu = win32_menu->PrevMenu;
        }
    }
}

void IMenu_Init() {
    SDL_SetWindowsMessageHook((SDL_WindowsMessageHook)WindowsMessageHook, NULL);
}

// Platform-specific menubar
namespace UI::Graphics::Renderer {
    void Sleep(double seconds) {
		// https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
        using namespace std::chrono;

        static HANDLE timer = CreateWaitableTimer(NULL, FALSE, NULL);
        static double estimate = 5e-3;
        static double mean = 5e-3;
        static double m2 = 0;
        static int64_t count = 1;

        while (seconds - estimate > 1e-7) {
            double toWait = seconds - estimate;
            LARGE_INTEGER due;
            due.QuadPart = -int64_t(toWait * 1e7);
            auto start = high_resolution_clock::now();
            SetWaitableTimerEx(timer, &due, 0, NULL, NULL, NULL, 0);
            WaitForSingleObject(timer, INFINITE);
            auto end = high_resolution_clock::now();

            double observed = (end - start).count() / 1e9;
            seconds -= observed;

            ++count;
            double error = observed - toWait;
            double delta = error - mean;
            mean += delta / count;
            m2 += delta * (error - mean);
            double stddev = sqrt(m2 / (count - 1));
            estimate = mean + stddev;
        }

        // spin lock
        auto start = high_resolution_clock::now();
        while ((high_resolution_clock::now() - start).count() / 1e9 < seconds);
    }
}

namespace UI::SystemDialog {
    bool StartProcess(const char* appPath, const char* cmd, const char* startDir) {
        // additional information
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;

        // set the size of the structures 
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        // start the program up
        CreateProcessA(appPath,   // the path
            (char*)cmd,        // Command line
            NULL,           // Process handle not inheritable
            NULL,           // Thread handle not inheritable
            FALSE,          // Set handle inheritance to FALSE
            0,              // No creation flags
            NULL,           // Use parent's environment block
            startDir,           // Use parent's starting directory 
            &si,            // Pointer to STARTUPINFO structure
            &pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
        );

        // WaitForSingleObject(pi.hProcess, INFINITE);

        // Close process and thread handles. 
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
}

namespace GameLinker {
    void LinkExternalGameLogic(LinkData* linkData, const char* projectFolder) {
        #define DL_EXT ".dll"

        char binaryPath[1024];
        const char* fileNameDLL = "Binaries/Game" DL_EXT;

        // Update the editor DLL
		sprintf(binaryPath, "%s/%s", projectFolder, "Binaries/Game" DL_EXT);
        std::ifstream src(binaryPath, std::ios::binary);

		sprintf(binaryPath, "%s/%s", projectFolder, "Binaries/GameEditor" DL_EXT);
		std::ofstream dst(binaryPath, std::ios::binary);
		dst << src.rdbuf();

        src.close();
        dst.close();

        // Load that DLL instead
        fileNameDLL = "Binaries/GameEditor" DL_EXT;

        GameLogicSharedObject = SDL_LoadObject(binaryPath);
        if (GameLogicSharedObject) {
            void (*linkGameLogic)(LinkData*) = (void (*)(LinkData*))SDL_LoadFunction(GameLogicSharedObject, "LinkGameLogic");
            if (linkGameLogic) {
                linkGameLogic(linkData);
            }
            else
                Diagnostics::SetError("Could not find \"%s\" in %s! (%s)", "LinkGameLogic", fileNameDLL, SDL_GetError());
        }
        else {
            Diagnostics::SetError("Could not find %s! (%s)", fileNameDLL, SDL_GetError());
        }
    }
}

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <Hatch/Primitives.h>

#if defined(_MACOS) || defined(_WINDOWS)
#define USE_NATIVE_MENU
#endif

enum ShortcutModifier {
    SM_NONE = 0x00,
    SM_CONTROL = 0x100,
    SM_SHIFT = 0x200,
    SM_ALT = 0x400,
	SM_OPTION = 0x400,
    SM_COMMAND = 0x800,
};

enum ItemType {
    IT_TEXT,
    IT_CHECKMARK_UNCHECKED,
    IT_RADIO_UNCHECKED,
    IT_CHECKMARK_CHECKED,
    IT_RADIO_CHECKED,
    IT_SUBMENU,
    IT_SEPARATOR
};

#ifndef USE_NATIVE_MENU
struct IMenuItem {
    String Text;
    String ShortcutText;
    void (*Action)();
    void* Submenu;
    int Shortcut;
    int AltShortcut;
    bool Enabled;
    int Type;
    int Index;
};
#endif

struct IMenu {
    void* Data;

#ifndef USE_NATIVE_MENU
    IMenuItem** Items;
    int Capacity;
    int Count;
#endif
};

extern void IMenu_Init(void);

extern struct IMenu* IMenu_Create(void);

// Adds an item to the menu, returns the index
extern int IMenu_AddItem(struct IMenu* menu, const char* title, void (*action)(), int shortcut, int enabled, int type, int altShortcut);
extern int IMenu_AddSubmenu(struct IMenu* menu, struct IMenu* submenu, const char* title, int altShortcut);
extern int IMenu_AddSeparator(struct IMenu* menu);
extern void IMenu_EditItem(struct IMenu* menu, int index, const char* title, void (*action)(void), int shortcut, int enabled, int type);
extern void IMenu_ClearItems(struct IMenu* menu);

extern void IMenu_SetAppMenu(struct IMenu* menu);

extern void IMenu_SetAppleMenu(struct IMenu* menu); // Give it an empty menu and on MacOS it will fill as needed
extern void IMenu_SetWindowMenu(struct IMenu* menu); // Give it a menu and on MacOS it will fill as needed, placing the right menuitems in the right spots
extern void IMenu_SetHelpMenu(struct IMenu* menu); // Give it a menu and on MacOS it will fill as needed, placing the right menuitems in the right spots

extern void IMenu_Dispose(struct IMenu* menu);

extern void Platform_Sleep(double seconds);

#ifdef __cplusplus
}
#endif

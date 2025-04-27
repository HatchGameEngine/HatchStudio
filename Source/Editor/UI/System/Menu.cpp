#include <UI/System/Menu.hpp>
#include <UI/Platforms/Common.h>

namespace UI {
    bool Initialized = false;
    void EnsureInit() {
        if (Initialized) return;

        IMenu_Init();
        Initialized = true;
    }

    Menu::Menu() {
        EnsureInit();

        menu = IMenu_Create();
        if (menu == nullptr)
            throw "Could not create Menu object.";
    }
    Menu::~Menu() {
        EnsureInit();

        IMenu_Dispose((IMenu*)menu);
    }

    int Menu::AddItem(const char* text, void (*action)(), int shortcut, int enabled, ItemType type) {
        EnsureInit();
        return IMenu_AddItem((IMenu*)menu, text, action, shortcut, enabled, (int)type);
    }
    int Menu::AddSubmenu(const char* text, Menu* submenu) {
        EnsureInit();
        return IMenu_AddSubmenu((IMenu*)menu, (IMenu*)submenu->menu, text);
    }
    int Menu::AddSeparator() {
        EnsureInit();
        return IMenu_AddSeparator((IMenu*)menu);
    }
    void Menu::EditItem(int index, const char* text, void (*action)(), int shortcut, int enabled, ItemType type) {
        EnsureInit();
        IMenu_EditItem((IMenu*)menu, index, text, action, shortcut, enabled, (int)type);
    }
    void Menu::ClearItems() {
        EnsureInit();
        IMenu_ClearItems((IMenu*)menu);
    }

    void Menu::SetMainMenu(Menu* menu) {
        EnsureInit();
        IMenu_SetAppMenu((IMenu*)menu->menu);
    }
#ifdef _MACOS
    void Menu::SetAppleMenu(Menu* menu) {
        EnsureInit();
        IMenu_SetAppleMenu((IMenu*)menu->menu);
    }
    void Menu::SetWindowMenu(Menu* menu) {
        EnsureInit();
        IMenu_SetWindowMenu((IMenu*)menu->menu);
    }
    void Menu::SetHelpMenu(Menu* menu) {
        EnsureInit();
        IMenu_SetHelpMenu((IMenu*)menu->menu);
    }
#endif
}

#include <UI/System/Menu.hpp>
#include <UI/Platforms/Common.h>
#include <Hatch/Strings.h>

#include <cctype>

namespace UI {
#ifdef USE_NATIVE_MENU
    bool Initialized = false;

    void EnsureInit() {
        if (Initialized) return;

        IMenu_Init();
        Initialized = true;
    }
#endif

    Menu::Menu() {
#ifdef USE_NATIVE_MENU
        EnsureInit();

        menu = IMenu_Create();
        if (menu == nullptr)
            throw "Could not create Menu object.";
#else
        IMenu* menuPtr = new IMenu;

        // Allocate menu items
        menuPtr->Count = 0;
        menuPtr->Capacity = 4;
        menuPtr->Items = (IMenuItem**)calloc(menuPtr->Capacity, sizeof(IMenuItem*));
        if (menuPtr->Items == NULL) {
            delete menuPtr;
            throw "Could not create Menu object.";
        }

        menu = menuPtr;
#endif
    }
    Menu::~Menu() {
#ifdef USE_NATIVE_MENU
        EnsureInit();
        IMenu_Dispose((IMenu*)menu);
#else
        if (menu) {
            IMenu* menuPtr = ((IMenu*)menu);
            for (size_t i = 0; i < menuPtr->Count; i++) {
                free(menuPtr->Items[i]);
            }
            free(menuPtr->Items);
            delete menuPtr;
        }
#endif
    }

#ifndef USE_NATIVE_MENU
    void* Menu::AddItemInternal() {
        IMenu* menuPtr = (IMenu*)menu;
        if (menuPtr == NULL || menuPtr->Items == NULL)
            return NULL;

        if (menuPtr->Count + 1 >= menuPtr->Capacity) {
            menuPtr->Capacity *= 2;

            IMenuItem** newItems = (IMenuItem**)realloc(menuPtr->Items, menuPtr->Capacity * sizeof(IMenuItem*));
            if (newItems == NULL)
                return NULL;

            menuPtr->Items = newItems;
        }

        IMenuItem* item = (IMenuItem*)calloc(1, sizeof(IMenuItem));
        if (item == NULL) {
            return NULL;
        }

        item->Enabled = true;
        item->Type = IT_TEXT;
        item->Index = menuPtr->Count;

        menuPtr->Items[menuPtr->Count] = item;
        menuPtr->Count++;

        return item;
    }

    void GetShortcutText(String* string, int shortcut) {
        char str[24];
        memset(str, 0, sizeof str);

        if ((shortcut & 0xFF) != 0) {
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
        }

        Strings::FromCString(string, str, 0);
    }
#endif

    int Menu::AddItem(const char* text, void (*action)(), int shortcut, int enabled, ItemType type, int altShortcut) {
#ifdef USE_NATIVE_MENU
        EnsureInit();
        return IMenu_AddItem((IMenu*)menu, text, action, shortcut, enabled, (int)type, altShortcut);
#else
        IMenuItem* item = (IMenuItem*)AddItemInternal();
        if (!item) {
            return -1;
        }

        Strings::FromCString(&item->Text, text, 0);
        GetShortcutText(&item->ShortcutText, shortcut);
        item->Action = action;
        item->Shortcut = shortcut;
        item->AltShortcut = altShortcut;
        item->Enabled = enabled;
        item->Type = (int)type;

        return item->Index;
#endif
    }
    int Menu::AddSubmenu(const char* text, Menu* submenu, int altShortcut) {
#ifdef USE_NATIVE_MENU
        EnsureInit();
        return IMenu_AddSubmenu((IMenu*)menu, (IMenu*)submenu->menu, text, altShortcut);
#else
        IMenuItem* item = (IMenuItem*)AddItemInternal();
        if (!item) {
            return -1;
        }

        Strings::FromCString(&item->Text, text, 0);
        Strings::FromCString(&item->ShortcutText, ">", 0);
        item->Submenu = submenu;
        item->Type = IT_SUBMENU;
        item->AltShortcut = altShortcut;

        return item->Index;
#endif
    }
    int Menu::AddSeparator() {
#ifdef USE_NATIVE_MENU
        EnsureInit();
        return IMenu_AddSeparator((IMenu*)menu);
#else
        IMenuItem* item = (IMenuItem*)AddItemInternal();
        if (!item) {
            return -1;
        }

        item->Type = IT_SEPARATOR;

        return item->Index;
#endif
    }
    void* Menu::GetItem(int index) {
#ifndef USE_NATIVE_MENU
        IMenu* menuPtr = (IMenu*)menu;
        if (menuPtr == NULL || menuPtr->Items == NULL || index >= menuPtr->Count || index < 0)
            return NULL;

        return menuPtr->Items[index];
#else
        return NULL;
#endif
    }
    int Menu::NumItems() {
#ifndef USE_NATIVE_MENU
        IMenu* menuPtr = (IMenu*)menu;
        if (menuPtr == NULL)
            return 0;

        return menuPtr->Count;
#else
        return 0;
#endif
    }
    void Menu::EditItem(int index, const char* text, void (*action)(), int shortcut, int enabled, ItemType type) {
#ifdef USE_NATIVE_MENU
        EnsureInit();
        IMenu_EditItem((IMenu*)menu, index, text, action, shortcut, enabled, (int)type);
#else
        IMenu* menuPtr = (IMenu*)menu;
        if (menuPtr == NULL || menuPtr->Items == NULL || index >= menuPtr->Count || index < 0)
            return;

        IMenuItem* item = menuPtr->Items[index];
        Strings::FromCString(&item->Text, text, 0);
        GetShortcutText(&item->ShortcutText, shortcut);
        item->Action = action;
        item->Shortcut = shortcut;
        item->Enabled = enabled;
        item->Type = (int)type;
#endif
    }
    void Menu::ClearItems() {
#ifdef USE_NATIVE_MENU
        EnsureInit();
        IMenu_ClearItems((IMenu*)menu);
#else
        IMenu* menuPtr = ((IMenu*)menu);
        for (size_t i = 0; i < menuPtr->Count; i++) {
            free(menuPtr->Items[i]);
        }
        menuPtr->Count = 0;
#endif
    }

#ifdef USE_NATIVE_MENU
    void Menu::SetNativeMainMenu(Menu* menu) {
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
#endif
}

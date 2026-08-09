#pragma once

namespace UI {
    class Menu {
    public:
        enum ShortcutModifier {
            SM_NONE = 0x00,
            SM_CONTROL = 0x100,
            SM_SHIFT = 0x200,
            SM_ALT = 0x400,
        	SM_OPTION = 0x400,
            SM_COMMAND = 0x800,
        };

        enum class ItemType {
            IT_TEXT,
            IT_CHECKMARK_UNCHECKED,
            IT_RADIO_UNCHECKED,
            IT_CHECKMARK_CHECKED,
            IT_RADIO_CHECKED,
            IT_SUBMENU,
            IT_SEPARATOR
        };

        Menu();
        ~Menu();

#ifndef USE_NATIVE_MENU
        void* AddItemInternal();
#endif

        int AddItem(const char* text, void (*action)(), int shortcut, int enabled, ItemType type = ItemType::IT_TEXT, int altShortcut = 0);
        int AddSubmenu(const char* text, Menu* submenu, int altShortcut = 0);
        int AddSeparator();
        void EditItem(int index, const char* text, void (*action)(), int shortcut, int enabled, ItemType type = ItemType::IT_TEXT);
        void* GetItem(int index);
        int NumItems();
        void ClearItems();

#ifdef USE_NATIVE_MENU
        static void SetNativeMainMenu(Menu* menu);
	#ifdef _MACOS
        static void SetAppleMenu(Menu* menu);
        static void SetWindowMenu(Menu* menu);
        static void SetHelpMenu(Menu* menu);
	#endif
#endif

    protected:
        void* menu;
    };
}

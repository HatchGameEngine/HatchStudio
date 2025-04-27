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
        };

        Menu();
        ~Menu();

        int AddItem(const char* text, void (*action)(), int shortcut, int enabled, ItemType type = ItemType::IT_TEXT);
        int AddSubmenu(const char* text, Menu* submenu);
        int AddSeparator();
        void EditItem(int index, const char* text, void (*action)(), int shortcut, int enabled, ItemType type = ItemType::IT_TEXT);
        void ClearItems();

        static void SetMainMenu(Menu* menu);
	#ifdef _MACOS
        static void SetAppleMenu(Menu* menu);
        static void SetWindowMenu(Menu* menu);
        static void SetHelpMenu(Menu* menu);
	#endif

    protected:
        void* menu;
    };
}

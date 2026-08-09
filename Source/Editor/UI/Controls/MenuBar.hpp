#pragma once

#include "Control.hpp"

struct MenuBarItemPosition {
    int X, Y, W, H;
};

/********************
* Enums / Constants *
********************/

/******************
* Event Arg Types *
******************/

/***********
* Controls *
***********/

struct MenuBar : Control {
public:
    // Properties
    DEFINE_PROPERTY_BASICF(int, SelectedIndex, MenuBar);
    DEFINE_PROPERTY_BASICF(int, HighlightedIndex, MenuBar);

    void *MenuPtr;

    std::vector<MenuBarItemPosition> ItemPositions;

    Control* Dropdown = NULL;
    bool FocusShortcut = false;

    Color HighlightColor;
    Color HighlightOutlineColor;

    int ItemHeight = 32;

    MenuBar() : Control() {
        BackColor = Color(0x434856, 0xFF);
        ForeColor = Color(0xFFFFFF, 0xFF);
        HighlightColor = Color(0x007FFF, 0xFF);
        HighlightOutlineColor = Color(0x005FBF, 0xFF);

        Dock = DOCK_TOP;

		SelectedIndex = -1;
        HighlightedIndex = -1;
    }
    ~MenuBar() {
        CloseDropdown();
    }

    void SetMenu(void *menuPtr);

    void CalculateItemPositions();

    void Select(int index);
    void SelectNext();
    void SelectPrevious();
    void HighlightSelection(int index);

    int GetSelectionUnderCursor(int relX, int relY);

    void OpenDropdown(int index);
    void CloseDropdown();

    void ConfirmSelection();
    void Close();

    void ChangeToAltFocus();
    void StopFocus();

    bool HandleShortcuts(SDL_Event* e, void* menuPtr);
    bool HandleAltShortcuts(SDL_Event* e, void* menuPtr);

    // Events
    void OnMouseMove(MouseEventArgs* e);
    void OnMouseDown(MouseEventArgs* e);
    void OnMouseLeave(MouseEventArgs* e);

    virtual void HandleSDLEvent(SDL_Event* e);
    virtual void Update();
    virtual void Render();

protected:
    // Setters
    virtual void set_Size(::Size value) {
        Control::set_Size(value);

        ResizeChildren();
    }

private:
    bool AltFocus = false;
    Uint32 AltTimerStart = 0;
    Uint32 AltWaitDuration = 300;
};

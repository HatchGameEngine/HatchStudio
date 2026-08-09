#pragma once

#include "Control.hpp"

struct DropDownItemPosition {
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

struct DropDown : Control {
public:
    // Properties
    DEFINE_PROPERTY_BASICF(int, HighlightedIndex, DropDown);

    void *MenuPtr;

    std::vector<DropDownItemPosition> ItemPositions;

    Control* MenuBarPtr = NULL;
    Control* Child = NULL;
    Control* ParentDropdown = NULL;

    bool FocusShortcut = false;

    Color OutlineColor;
    Color HighlightColor;
    Color HighlightOutlineColor;
    Color SeparatorColor;

    DropDown() : Control() {
        Init();
    }
    ~DropDown() {}

    void Init();

    void SetMenu(void *menuPtr);

    void CalculateItemPositions();

    void Select(int index);
    void Close();

    void HighlightSelection(int index);
    void HighlightFirstAvailable();
    void HighlightNext();
    void HighlightPrevious();

    int GetSelectionUnderCursor(int relX, int relY);
    void ConfirmSelection();

    bool HandleAltShortcuts(SDL_Event* e, void* menuPtr);

    void OpenChild(int index);
    void CloseChild();

    bool IsMouseOnTopOfChild(SDL_Event* e);
    bool IsMouseOnTopOfSelf(SDL_Event* e);

    // Events
    void OnMouseMove(MouseEventArgs* e);
    void OnMouseDown(MouseEventArgs* e);
    void OnMouseUp(MouseEventArgs* e);
    void OnMouseLeave(MouseEventArgs* e);

    virtual void HandleSDLEvent(SDL_Event* e);
    virtual void Render();

protected:
    SDL_Texture* ShapeRadioUnchecked;
    SDL_Texture* ShapeRadioChecked;

    // Setters
    virtual void set_Size(::Size value) {
        Control::set_Size(value);

        ResizeChildren();
    }
};

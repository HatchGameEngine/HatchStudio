#pragma once

#include "Control.hpp"
#include "Form.hpp"

/********************
* Enums / Constants *
********************/

/******************
* Event Arg Types *
******************/

/***********
* Controls *
***********/
struct ComboBoxItemCollection : List<CString> {
    virtual bool IsFixedSize() const noexcept { return false; };
    virtual bool IsReadOnly() const noexcept { return false; };
};

struct ComboBox;

struct ComboBoxDropDown : Form {
public:
    ComboBox* ParentComboBox = NULL;
    ComboBoxDropDown(ComboBox* parentComboBox);

    void HandleSDLEvent(SDL_Event* e);
    void Render();

    int HoverIndex = -1;
};

struct ComboBox : Control {
    friend class ComboBoxDropDown;

    const int ARROW_SPACE_WIDTH = 20;

public:
    ComboBoxItemCollection Items;

    bool AutoEllipsis = false; // Gets or sets a value indicating whether the ellipsis character (...) appears at the right edge of the control, denoting that the control text extends beyond the specified length of the control.
    int TextAlign = TEXT_ALIGN_LEFT;
    Color FocusColor;
    Color HoverColor;
    Color PressedColor;

    int SelectedIndex = -1;
    bool Pressing = false;
    String Text = { };

    int ItemHeight = 20;

    DEFINE_SIMPLE_EVENT(SelectedIndexChanged);

    ComboBox();

    void OnMouseClick(MouseEventArgs* e);
    void OnMouseDown(MouseEventArgs* e);
    void OnMouseUp(MouseEventArgs* e);
    void OnMouseMove(MouseEventArgs* e);
    void OnMouseEnter(MouseEventArgs* e);
    void OnMouseLeave(MouseEventArgs* e);

    void OnKeyDown(KeyEventArgs* e);
    void OnKeyUp(KeyEventArgs* e);

    void Render();

    void Select(int index);
    void OpenDialog();
    void CloseDialog();

protected:
    SDL_Texture* ShapeTriangleFill;
    ComboBoxDropDown* DropDownControl = NULL;

private:
    String BufferText = { };
    bool Opened = false;
};

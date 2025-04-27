#pragma once

#include "Control.hpp"
#include "Form.hpp"

/********************
* Enums / Constants *
********************/

enum class Appearance {
    Normal,
    Button,
};

enum class CheckState {
    Unchecked,
    Checked,
    Indeterminate,
};

/******************
* Event Arg Types *
******************/

/***********
* Controls *
***********/

struct ButtonBase : Control {
public:
    bool AutoEllipsis = false; // Gets or sets a value indicating whether the ellipsis character (...) appears at the right edge of the control, denoting that the control text extends beyond the specified length of the control.
    SDL_Texture* Image = NULL;
    String Text = { };
    int TextAlign = TEXT_ALIGN_CENTER;

    Color FocusColor;
    Color HoverColor;
    Color PressedColor;
    String* TextPtr = NULL;

    bool Pressing = false;

    ButtonBase();
    ButtonBase(CString string);
    ButtonBase(String* string);

    void SetText(CString title);
    void SetText(String* title);

    ::Size get_Size();

    void OnMouseDown(MouseEventArgs* e);
    void OnMouseUp(MouseEventArgs* e);
    void OnMouseMove(MouseEventArgs* e);
    void OnMouseEnter(MouseEventArgs* e);
    void OnMouseLeave(MouseEventArgs* e);

    void OnKeyDown(KeyEventArgs* e);
    void OnKeyUp(KeyEventArgs* e);
};

struct Button : ButtonBase {
    DialogResult Result = DialogResult::None;

    int BorderRadius = 4;

    Button() : ButtonBase() { Init(); }
    Button(CString string) : ButtonBase(string) { Init(); }
    Button(String* string) : ButtonBase(string) { Init(); }

    void DrawButtonShape(SDL_Rect* bounds, Color color);
    void Render();

    void set_Size(::Size size);

protected:
    SDL_Texture* FillShape;
    void Init();
};

struct RadioButton : ButtonBase {
    Appearance Appearance = Appearance::Normal; // Gets or sets the value that determines the appearance of a CheckBox control.
    bool AutoCheck = false; // Gets or set a value indicating whether the Checked or CheckState values and the CheckBox's appearance are automatically changed when the CheckBox is clicked.
    int CheckAlign = TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE;
    bool Checked = false; // Gets or sets a value indicating whether the control is checked.

    RadioButton** SelectionGroup = NULL;

    // Events
    DEFINE_SIMPLE_EVENT(CheckedChanged);

    RadioButton() : ButtonBase() { Init(); }
    RadioButton(CString string) : ButtonBase(string) { Init(); }
    RadioButton(String* string) : ButtonBase(string) { Init(); }

    void Check() {
        bool checked = Checked;
        Checked = false;

        if (SelectionGroup == NULL)
            return;

        if (*SelectionGroup)
            (*SelectionGroup)->Checked = false;
        *SelectionGroup = this;

        Checked = true;

        if (checked != Checked)
            OnCheckedChanged(NULL);
    }

    void OnMouseClick(MouseEventArgs* e) {
        ButtonBase::OnMouseClick(e);
        Check();
    }

    ::Size get_Size();

    void Render();

protected:
    SDL_Texture* OuterRadius;
    SDL_Texture* OuterRadiusBorder;
    SDL_Texture* InnerRadius;
    void Init();
};

struct CheckBox : ButtonBase {
    Appearance Appearance = Appearance::Normal; // Gets or sets the value that determines the appearance of a CheckBox control.
    bool AutoCheck = false; // Gets or set a value indicating whether the Checked or CheckState values and the CheckBox's appearance are automatically changed when the CheckBox is clicked.
    int CheckAlign = TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE;
    CheckState CheckState = CheckState::Unchecked;
    bool ThreeState = false;

    // Events
    DEFINE_SIMPLE_EVENT(CheckedChanged);

    CheckBox() : ButtonBase() { Init(); }
    CheckBox(CString string) : ButtonBase(string) { Init(); }
    CheckBox(String* string) : ButtonBase(string) { Init(); }

    void OnMouseClick(MouseEventArgs* e) {
        ButtonBase::OnMouseClick(e);

        if (ThreeState) {
            if (CheckState != CheckState::Unchecked)
                CheckState = CheckState::Unchecked;
            else
                CheckState = CheckState::Checked;
        }
        else {
            if (CheckState != CheckState::Unchecked)
                CheckState = CheckState::Unchecked;
            else
                CheckState = CheckState::Checked;
        }
        OnCheckedChanged(NULL);
    }

    ::Size get_Size();

    bool GetChecked() { return CheckState != CheckState::Unchecked; }  // true if the CheckBox is in the checked state; otherwise, false. The default value is false. Note: If the ThreeState property is set to true, the Checked property will return true for either a Checked or Indeterminate CheckState.

    void Render();

protected:
    SDL_Texture* RadiusImage;
    SDL_Texture* CheckIcon;
    void Init();
};

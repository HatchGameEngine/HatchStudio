#pragma once

#include "Textbox.hpp"
#include "Button.hpp"

/********************
* Enums / Constants *
********************/

/******************
* Event Arg Types *
******************/

/***********
* Controls *
***********/

// NumericUpDownAccelerationCollection : List<NumericUpDownAcceleration>

struct NumericUpDown : TextboxBase {
public:
    // NumericUpDownAccelerationCollection Accelerations; // Gets a collection of sorted acceleration objects for the NumericUpDown control.
    DEFINE_PROPERTY_NOSETF(int, DecimalPlaces, NumericUpDown); // Gets or sets the number of decimal places to display in the spin box (also known as an up-down control). This property doesn't affect the Value property.
    DEFINE_PROPERTY_NOSETF(bool, Hexadecimal, NumericUpDown); // Gets or sets a value indicating whether the spin box (also known as an up-down control) should display the value it contains in hexadecimal format.
    double Increment = 1.0; // Gets or sets the value to increment or decrement the spin box (also known as an up-down control) when the up or down buttons are clicked.
    double Maximum = 100.0;
    double Minimum = 0.0;
    // bool ThousandsSeparator = false; // Gets or sets a value indicating whether a thousands separator is displayed in the spin box (also known as an up-down control) when appropriate.
    DEFINE_PROPERTY_NOSETF(double, Value, NumericUpDown);

    // When committing a value, we can have the NumericUpDown optionally write to a pointer
    void* ValuePtr = NULL;
    bool IsInteger = false;
    size_t NumberTypeSize = sizeof(float);

    // Events
    DEFINE_SIMPLE_EVENT(ValueChanged);

    NumericUpDown(double value = 0.0f);
    ~NumericUpDown();

    void ReadPointer();
    void WritePointer();

    ::Size get_Size();
    void set_Size(::Size size);
    void set_Value(double value);

    void set_Hexadecimal(bool value);
    void set_DecimalPlaces(int value);

    void UpdateText();
    void UpdateLayout();
    void HandleSDLEvent(SDL_Event* e) {
        TextboxBase::HandleSDLEvent(e);

        buttonUp->HandleSDLEvent(e);
        buttonDown->HandleSDLEvent(e);
    }

    // void OnClick(EventArgs* e);
    // void OnDoubleClick(EventArgs* e);
    //
    // void OnMouseDown(MouseEventArgs* e);
    // void OnMouseClick(MouseEventArgs* e);
    // void OnMouseUp(MouseEventArgs* e);
    // void OnMouseDoubleClick(MouseEventArgs* e);
    // void OnMouseMove(MouseEventArgs* e);
    // void OnMouseEnter(MouseEventArgs* e);
    // void OnMouseLeave(MouseEventArgs* e);
    // void OnMouseWheel(MouseEventArgs* e);
    //
    // void OnTextInputted(TextEventArgs* e);
    // void OnTextEdited(TextEventArgs* e);
    //
    // void OnKeyDown(KeyEventArgs* e);
    // void OnKeyUp(KeyEventArgs* e);
    //
    void OnFocusLost(EventArgs* e);

    void Render();

private:
    Button* buttonUp;
    Button* buttonDown;
    SDL_Texture* ShapeTriangleFill;

    void buttonUp_onClick(void* sender, EventArgs* e) { set_Value(internal_Value + Increment); }
    void buttonDown_onClick(void* sender, EventArgs* e) { set_Value(internal_Value - Increment); }
};

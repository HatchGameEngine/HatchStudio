#pragma once

#include "Control.hpp"

/********************
* Enums / Constants *
********************/

/******************
* Event Arg Types *
******************/

/***********
* Controls *
***********/

struct Label : Control {
public:
    String Text = { };

    Label();
    Label(CString text);
    Label(String* text);

    void SetText(CString text);
    void SetText(String* text);

    ::Size get_Size();

    void Render();
};

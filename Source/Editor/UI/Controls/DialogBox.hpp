#pragma once

#include "Form.hpp"
#include "Label.hpp"
#include "Button.hpp"
#include "ScrollBar.hpp"

/********************
* Enums / Constants *
********************/

/******************
* Event Arg Types *
******************/

/***********
* Controls *
***********/

struct DialogBox : Form {
    DialogBox(int w, int h, const char* title, const char* text);
    ~DialogBox();

private:
    Label* labelText;
    Button* buttonYes;
    Button* buttonNo;

    FlowLayoutPanel* mainPanel;
};

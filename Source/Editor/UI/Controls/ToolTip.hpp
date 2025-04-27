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
struct ToolTip : Form {
public:
    String Text = { };
    ToolTip(String* text);
    ToolTip(const char* text);
    void HandleSDLEvent(SDL_Event* e);
    void Render();
private:
    void SetText(String* text);
    void SetText(const char* text);
};

#pragma once

#include "Control.hpp"
#include "Form.hpp"

/********************
* Enums / Constants *
********************/

/******************
* Event Arg Types *
******************/

struct DialValueChangedArgs : EventArgs {
    double Value;
};
#define DEFINE_DIALVALUECHANGED_EVENT(name) Event<DialValueChangedArgs> on##name; virtual void On##name(DialValueChangedArgs* e) { on##name.Raise(this, e); }

struct DialTurnedArgs : EventArgs {
    double Value;
};
#define DEFINE_DIALTURNED_EVENT(name) Event<DialTurnedArgs> on##name; virtual void On##name(DialTurnedArgs* e) { on##name.Raise(this, e); }

/***********
* Controls *
***********/

struct RadialKnob : Control {
public:
    Color FocusColor;
    Color HoverColor;
    Color PressedColor;
    String* TextPtr = NULL;

    bool Pressing = false;

    double Angle = 0.0;
    double Bias = 0.0;
    DEFINE_DIALTURNED_EVENT(DialTurn);
    DEFINE_DIALVALUECHANGED_EVENT(ValueChanged);

    double MaxAngle = 360.0;

    bool SnapAngle = true;
    int SnapDivisors = 32;

    RadialKnob();

    void set_Size(::Size size);

    void OnMouseDown(MouseEventArgs* e);
    void OnMouseUp(MouseEventArgs* e);
    void OnMouseMove(MouseEventArgs* e);
    void OnMouseEnter(MouseEventArgs* e);
    void OnMouseLeave(MouseEventArgs* e);

    void OnKeyDown(KeyEventArgs* e);
    void OnKeyUp(KeyEventArgs* e);

    void Render();

protected:
    SDL_Texture* ShapeCircleFill;
    SDL_Texture* ShapeCircleStroke;
    SDL_Texture* ShapeTriangleFill;

private:
    MouseEventArgs dragStartEvent;
};

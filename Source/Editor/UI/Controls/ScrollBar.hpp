#pragma once

#include "Control.hpp"

/********************
* Enums / Constants *
********************/

enum class ScrollOrientation {
    HorizontalScroll, // The horizontal scroll bar.
    VerticalScroll, // The vertical scroll bar.
};
enum class ScrollEventType {
    SmallDecrement, // The scroll box was moved a small distance. The user clicked the left(horizontal) or top(vertical) scroll arrow, or pressed the UP ARROW key.
    SmallIncrement, // The scroll box was moved a small distance. The user clicked the right(horizontal) or bottom(vertical) scroll arrow, or pressed the DOWN ARROW key.
    LargeDecrement, // The scroll box moved a large distance. The user clicked the scroll bar to the left(horizontal) or above(vertical) the scroll box, or pressed the PAGE UP key.
    LargeIncrement, // The scroll box moved a large distance. The user clicked the scroll bar to the right(horizontal) or below(vertical) the scroll box, or pressed the PAGE DOWN key.
    ThumbPosition, // The scroll box was moved.
    ThumbTrack, // The scroll box is currently being moved.
    First, // The scroll box was moved to the Minimum position.
    Last, // The scroll box was moved to the Maximum position.
    EndScroll, // The scroll box has stopped moving.
};

/******************
* Event Arg Types *
******************/

struct ScrollEventArgs : EventArgs {
    int NewValue;
    int OldValue;
    ScrollOrientation Orientation;
    ScrollEventType Type;
};
#define DEFINE_SCROLL_EVENT(name) Event<ScrollEventArgs> on##name; virtual void On##name(ScrollEventArgs* e) { on##name.Raise(this, e); }

/***********
* Controls *
***********/

struct ScrollBar : Control {
public:
    // Fields
    bool DoEasing = false;

    // Properties
    DEFINE_PROPERTY_NOSETF(int, Minimum, ScrollBar);
    DEFINE_PROPERTY_NOSETF(int, Maximum, ScrollBar);
    DEFINE_PROPERTY_BASICF(int, SmallChange, ScrollBar);
    DEFINE_PROPERTY_BASICF(int, LargeChange, ScrollBar);
    DEFINE_PROPERTY_NOFUNC(int, Value, ScrollBar);

    // Events
    DEFINE_MOUSE_EVENT(MouseWheel);
    DEFINE_SCROLL_EVENT(Scroll);
    DEFINE_SIMPLE_EVENT(ValueChanged);

    ScrollBar() : Control() {
        Value = 0;
        Minimum = 0;
        Maximum = 100;
        SmallChange = 1;
        LargeChange = 10;
    }

    virtual void Update() {
        if (DoEasing)
            easing_Value += (internal_Value - easing_Value) / 2;
    }

protected:
    virtual SDL_Rect GetThumbBounds() = 0;
    bool MouseHovering = false;
    bool DragStarted = false;
    SDL_Point DragCursorStart = { 0, 0 };
    int ValueStart = 0;

private:
    int easing_Value = 0;

    // Setters
    void set_Value(int value) {
        internal_Value = M_CLAMP(value, Minimum, Maximum);
    }
    void set_Minimum(int value) {
        internal_Minimum = value;
        set_Value(internal_Value); // Update Value to new bounds
    }
    void set_Maximum(int value) {
        internal_Maximum = value;
        set_Value(internal_Value); // Update Value to new bounds
    }

    // Getters
    int get_Value() {
        if (DoEasing)
            return easing_Value;

        return internal_Value;
    }
};
struct HScrollBar : ScrollBar {
    HScrollBar() : ScrollBar() {
        Size = { 0, 16 };
    }

    virtual SDL_Rect GetThumbBounds();
    virtual void OnMouseDown(MouseEventArgs* e);
    virtual void OnMouseMove(MouseEventArgs* e);
    virtual void OnMouseLeave(MouseEventArgs* e);
    virtual void OnMouseUp(MouseEventArgs* e);
    virtual void Render();
};
struct VScrollBar : ScrollBar {
    VScrollBar() : ScrollBar() {
        Size = { 16, 0 };
    }

    virtual SDL_Rect GetThumbBounds();
    virtual void OnMouseDown(MouseEventArgs* e);
    virtual void OnMouseMove(MouseEventArgs* e);
    virtual void OnMouseLeave(MouseEventArgs* e);
    virtual void OnMouseUp(MouseEventArgs* e);
    virtual void Render();
};
struct ScrollableControl : Control {
    bool AutoScroll;
    SDL_Rect DisplayBounds;
    SDL_Rect ContentBounds;
    bool DoHScroll;
    bool DoVScroll;
    bool HideEmptyHScroll;
    bool HideEmptyVScroll;
    HScrollBar* HScrollControl;
    VScrollBar* VScrollControl;

    ScrollableControl() : Control() {
        AutoScroll = true;
        DisplayBounds = { 0, 0, 100, 100 };
        ContentBounds = { 0, 0, 100, 100 };

        DoHScroll = false;
        DoVScroll = false;
        HideEmptyHScroll = true;
        HideEmptyVScroll = true;
        HScrollControl = new HScrollBar();
        VScrollControl = new VScrollBar();

        EventArgs eH;
        HScrollControl->Parent = this;
        HScrollControl->OnParentChanged(&eH);

        EventArgs eV;
        VScrollControl->Parent = this;
        VScrollControl->OnParentChanged(&eV);
    }
	~ScrollableControl() {
        delete HScrollControl;
        delete VScrollControl;
    }

    virtual void OnMouseMove(MouseEventArgs* e) {
        if (MouseOver)
            SDL_SetCursor(Cursor);
    }

    virtual void OnMouseWheel(MouseEventArgs* e) {
        if (DoVScroll)
            VScrollControl->Value = VScrollControl->Value - VScrollControl->SmallChange * e->Delta;
        else if (DoHScroll)
            HScrollControl->Value = HScrollControl->Value - HScrollControl->SmallChange * e->Delta;
    }
    virtual void HandleSDLEvent(SDL_Event* e) {
        auto temp = ScrollLocation;
        ScrollLocation = { 0, 0 };

        HScrollControl->HandleSDLEvent(e);
        VScrollControl->HandleSDLEvent(e);

        ScrollLocation = temp;

        Control::HandleSDLEvent(e);
    }
    virtual void ResizeChildren() {
        // Bounds: the size of the container, as we want it set
        // ContentBounds: the size of the content inside the container
        auto Bounds = GetScreenRect();
        auto contentBounds = GetContentSize();

        ContentBounds.w = contentBounds.W;
        ContentBounds.h = contentBounds.H;
        DisplayBounds.w = Bounds.w;
        DisplayBounds.h = Bounds.h;

        bool showHScrollBar = DoHScroll && (!HideEmptyHScroll || DisplayBounds.w < contentBounds.W);
        bool showVScrollBar = DoVScroll && (!HideEmptyVScroll || DisplayBounds.h < contentBounds.H);
        ::Size hScrollBarSize = HScrollControl->Size;
        ::Size vScrollBarSize = VScrollControl->Size;

        if (showHScrollBar)
            DisplayBounds.h -= hScrollBarSize.H;
        if (showVScrollBar)
            DisplayBounds.w -= vScrollBarSize.W;

        HScrollControl->Location = { 0, DisplayBounds.h };
        HScrollControl->Size = { DisplayBounds.w, hScrollBarSize.H };
        HScrollControl->Minimum = 0;
        HScrollControl->Maximum = ContentBounds.w - DisplayBounds.w;

        VScrollControl->Location = { DisplayBounds.w, 0 };
        VScrollControl->Size = { vScrollBarSize.W, DisplayBounds.h };
        VScrollControl->Minimum = 0;
        VScrollControl->Maximum = ContentBounds.h - DisplayBounds.h;

        Control::ResizeChildren();
    }
    virtual void Update() {
        auto temp = ScrollLocation;
        ScrollLocation = { 0, 0 };

        bool showHScrollBar = DoHScroll && (!HideEmptyHScroll || DisplayBounds.w < ContentBounds.w);
        bool showVScrollBar = DoVScroll && (!HideEmptyVScroll || DisplayBounds.h < ContentBounds.h);
        if (showHScrollBar)
            HScrollControl->Update();
        if (showVScrollBar)
            VScrollControl->Update();

        ScrollLocation = temp;

        Control::Update();
    }
    virtual void Render() {
        Control::Render();

        auto temp = ScrollLocation;
        ScrollLocation = { 0, 0 };

        bool showHScrollBar = DoHScroll && (!HideEmptyHScroll || DisplayBounds.w < ContentBounds.w);
        bool showVScrollBar = DoVScroll && (!HideEmptyVScroll || DisplayBounds.h < ContentBounds.h);
        if (showHScrollBar)
            HScrollControl->Render();
        if (showVScrollBar)
            VScrollControl->Render();

        ScrollLocation = temp;
    }
};

struct Panel : ScrollableControl {

};

enum class FlowDirection {
    LEFT_TO_RIGHT,
    RIGHT_TO_LEFT,
    TOP_TO_BOTTOM,
    BOTTOM_TO_TOP,
};

struct FlowLayoutPanel : Panel {
    FlowDirection FlowDirection = FlowDirection::TOP_TO_BOTTOM;
    bool WrapContents = true;
    bool FillLines = true;

    void UpdateLayout_LeftToRight(::Size validSize) {
        ::Size lineSize = { 0, 0 };
        ::Position lineStartPos = { Padding.Left, Padding.Top };
        int lineStartIndex = 0;
        int lineEndIndex = 0;

        for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
            auto Child = Controls.Items[i];
            ::Size ChildSize = Child->Size;

            if (Child->Dock == DOCK_LEFT ||
                Child->Dock == DOCK_FILL ||
                Child->Dock == DOCK_RIGHT ||
                Child->Anchor == (ANCHOR_TOP | ANCHOR_BOTTOM)) {
                lineSize.W += Child->Margin.Left + ChildSize.W + Child->Margin.Right;
            }
            else {
                lineSize.W += Child->Margin.Left + ChildSize.W + Child->Margin.Right;
                lineSize.H = M_MAX(lineSize.H, Child->Margin.Top + ChildSize.H + Child->Margin.Bottom);
            }

            if (Child->LineBreak) {
                lineEndIndex = i;
                goto EndOfLine;
            }

            // If row size is past the valid width, this means that the current control "breaks"
            if (WrapContents && lineSize.W >= validSize.W) {
                lineEndIndex = i - 1;
                goto EndOfLine;
            }
            if (i == iSz - 1) {
                lineEndIndex = i;
                goto EndOfLine;
            }

            continue;

        EndOfLine:
            if (FillLines) {
                // lineSize = validSize;
            }

            Position nextChildPos = lineStartPos;
            for (int r = lineStartIndex; r <= lineEndIndex; r++) {
                auto Child = Controls.Items[r];
                ::Size ChildSize = Child->Size;

                if (Child->Dock != DOCK_NONE && Child->Anchor == ANCHOR_NONE) {
                    // orientation-specific
                    switch (Child->Dock) {
                    case DOCK_LEFT:
                    case DOCK_RIGHT:
                    case DOCK_FILL:
                        Child->Size = { ChildSize.W, lineSize.H - Child->Margin.Vertical() };
                        Child->Location = { nextChildPos.X + Child->Margin.Left, nextChildPos.Y + Child->Margin.Top };
                        break;
                    case DOCK_TOP:
                        Child->Size = ChildSize;
                        Child->Location = { nextChildPos.X + Child->Margin.Left, nextChildPos.Y + Child->Margin.Top };
                        break;
                    case DOCK_BOTTOM:
                        Child->Size = ChildSize;
                        Child->Location = { nextChildPos.X + Child->Margin.Left, nextChildPos.Y + lineSize.H - Child->Margin.Bottom - ChildSize.H };
                        break;
                    }
                }
                else if (Child->Dock == DOCK_NONE) {
                    ::Size newSize;
                    ::Position newLocation;
                    ::Size childSpace = { ChildSize.W + Child->Margin.Horizontal(), ChildSize.H + Child->Margin.Vertical() };
                    ::Size cellSize = { childSpace.W, lineSize.H }; // orientation-specific

                    // Horizontal layout
                    switch (Child->Anchor & (ANCHOR_LEFT | ANCHOR_RIGHT)) {
                    case ANCHOR_LEFT:
                        newSize.W = ChildSize.W;
                        newLocation.X = Child->Margin.Left;
                        break;
                    case ANCHOR_RIGHT:
                        newSize.W = ChildSize.W;
                        newLocation.X = cellSize.W - ChildSize.W - Child->Margin.Right;
                        break;
                    case ANCHOR_LEFT | ANCHOR_RIGHT:
                        newSize.W = ChildSize.W - Child->Margin.Horizontal();
                        newLocation.X = Child->Margin.Left;
                        break;
                    case ANCHOR_NONE:
                        newSize.W = ChildSize.W;
                        newLocation.X = (cellSize.W - childSpace.W) / 2 + Child->Margin.Left;
                        break;
                    }

                    // Vertical layout
                    switch (Child->Anchor & (ANCHOR_TOP | ANCHOR_BOTTOM)) {
                    case ANCHOR_TOP:
                        newSize.H = ChildSize.H;
                        newLocation.Y = Child->Margin.Top;
                        break;
                    case ANCHOR_BOTTOM:
                        newSize.H = ChildSize.H;
                        newLocation.Y = cellSize.H - ChildSize.H - Child->Margin.Bottom;
                        break;
                    case ANCHOR_TOP | ANCHOR_BOTTOM:
                        newSize.H = ChildSize.H - Child->Margin.Vertical();
                        newLocation.Y = Child->Margin.Top;
                        break;
                    case ANCHOR_NONE:
                        newSize.H = ChildSize.H;
                        newLocation.Y = (cellSize.H - childSpace.H) / 2 + Child->Margin.Top;
                        break;
                    }

                    Child->Size = newSize;
                    Child->Location = newLocation + nextChildPos;
                }
                nextChildPos.X += Child->Margin.Left + ChildSize.W + Child->Margin.Right;
            }
            lineStartIndex = i + 1;
            lineStartPos.Y += lineSize.H;
            lineSize = { 0, 0 };
            continue;
        }
    }
    void UpdateLayout_TopToBottom(::Size validSize) {
        ::Size lineSize = { 0, 0 };
        ::Position lineStartPos = { Padding.Left, Padding.Top };
        int lineStartIndex = 0;
        int lineEndIndex = 0;

        for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
            auto Child = Controls.Items[i];
            ::Size ChildSize = Child->Size;

            if (Child->Dock == DOCK_TOP ||
                Child->Dock == DOCK_FILL ||
                Child->Dock == DOCK_BOTTOM ||
                Child->Anchor == (ANCHOR_LEFT | ANCHOR_RIGHT)) {
                lineSize.H += Child->Margin.Top + ChildSize.H + Child->Margin.Bottom;
            }
            else {
                lineSize.H += Child->Margin.Top + ChildSize.H + Child->Margin.Bottom;
                lineSize.W = M_MAX(lineSize.W, Child->Margin.Left + ChildSize.W + Child->Margin.Right);
            }

            if (Child->LineBreak) {
                lineEndIndex = i;
                goto EndOfLine;
            }

            // If row size is past the valid width, this means that the current control "breaks"
            if (WrapContents && lineSize.H >= validSize.H) {
                lineEndIndex = i - 1;
                goto EndOfLine;
            }
            if (i == iSz - 1) {
                lineEndIndex = i;
                goto EndOfLine;
            }

            continue;

        EndOfLine:
            if (FillLines) {
                lineSize = validSize;
            }

            Position nextChildPos = lineStartPos;
            for (int r = lineStartIndex; r <= lineEndIndex; r++) {
                auto Child = Controls.Items[r];
                ::Size ChildSize = Child->Size;

                if (Child->Dock != DOCK_NONE && Child->Anchor == ANCHOR_NONE) {
                    // orientation-specific
                    switch (Child->Dock) {
                    case DOCK_TOP:
                    case DOCK_BOTTOM:
                    case DOCK_FILL:
                        Child->Size = { lineSize.W - Child->Margin.Horizontal(), ChildSize.H };
                        Child->Location = { nextChildPos.X + Child->Margin.Left, nextChildPos.Y + Child->Margin.Top };
                        break;
                    case DOCK_LEFT:
                        Child->Size = ChildSize;
                        Child->Location = { nextChildPos.X + Child->Margin.Left, nextChildPos.Y + Child->Margin.Top };
                        break;
                    case DOCK_RIGHT:
                        Child->Size = ChildSize;
                        Child->Location = { nextChildPos.X + lineSize.W - Child->Margin.Right - ChildSize.W, nextChildPos.Y + Child->Margin.Top };
                        break;
                    }
                }
                else if (Child->Dock == DOCK_NONE) {
                    ::Size newSize;
                    ::Position newLocation;
                    ::Size childSpace = { ChildSize.W + Child->Margin.Horizontal(), ChildSize.H + Child->Margin.Vertical() };
                    ::Size cellSize = { lineSize.W, childSpace.H }; // orientation-specific

                    // Horizontal layout
                    switch (Child->Anchor & (ANCHOR_LEFT | ANCHOR_RIGHT)) {
                    case ANCHOR_LEFT:
                        newSize.W = ChildSize.W;
                        newLocation.X = Child->Margin.Left;
                        break;
                    case ANCHOR_RIGHT:
                        newSize.W = ChildSize.W;
                        newLocation.X = cellSize.W - ChildSize.W - Child->Margin.Right;
                        break;
                    case ANCHOR_LEFT | ANCHOR_RIGHT:
                        newSize.W = ChildSize.W - Child->Margin.Horizontal();
                        newLocation.X = Child->Margin.Left;
                        break;
                    case ANCHOR_NONE:
                        newSize.W = ChildSize.W;
                        newLocation.X = (cellSize.W - childSpace.W) / 2 + Child->Margin.Left;
                        break;
                    }

                    // Vertical layout
                    switch (Child->Anchor & (ANCHOR_TOP | ANCHOR_BOTTOM)) {
                    case ANCHOR_TOP:
                        newSize.H = ChildSize.H;
                        newLocation.Y = Child->Margin.Top;
                        break;
                    case ANCHOR_BOTTOM:
                        newSize.H = ChildSize.H;
                        newLocation.Y = cellSize.H - ChildSize.H - Child->Margin.Bottom;
                        break;
                    case ANCHOR_TOP | ANCHOR_BOTTOM:
                        newSize.H = ChildSize.H - Child->Margin.Vertical();
                        newLocation.Y = Child->Margin.Top;
                        break;
                    case ANCHOR_NONE:
                        newSize.H = ChildSize.H;
                        newLocation.Y = (cellSize.H - childSpace.H) / 2 + Child->Margin.Top;
                        break;
                    }

                    Child->Size = newSize;
                    Child->Location = newLocation + nextChildPos;
                }
                nextChildPos.Y += Child->Margin.Top + ChildSize.H + Child->Margin.Bottom;
            }
            lineStartIndex = i + 1;
            lineStartPos.X += lineSize.W;
            lineSize = { 0, 0 };
            continue;
        }
    }

    virtual void UpdateLayout() {
        ::Size validSize = internal_Size - ::Size { Padding.Horizontal(), Padding.Vertical() };

        switch (FlowDirection) {

        case FlowDirection::LEFT_TO_RIGHT:
            UpdateLayout_LeftToRight(validSize);
            break;

        case FlowDirection::TOP_TO_BOTTOM:
            UpdateLayout_TopToBottom(validSize);
            break;

        default:
            break;
        }


    }
};

struct TableLayoutPanel : Panel {

};

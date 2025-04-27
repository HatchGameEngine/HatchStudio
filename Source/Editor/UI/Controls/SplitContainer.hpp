#pragma once

#include "ScrollBar.hpp"

/********************
* Enums / Constants *
********************/

enum class SplitOrientation {
    Horizontal, // The horizontal split.
    Vertical, // The vertical split.
};
enum class SplitPanelFix {
    None,
    Panel1,
    Panel2,
};

/******************
* Event Arg Types *
******************/

struct SplitterEventArgs : EventArgs {
    int X;
    int Y;
    int SplitX;
    int SplitY;
};
struct SplitterCancelEventArgs : EventArgs {
    int MouseCursorX;
    int MouseCursorY;
    int SplitX;
    int SplitY;
};
#define DEFINE_SPLITTER_EVENT(name) Event<SplitterEventArgs> on##name; virtual void On##name(SplitterEventArgs* e) { on##name.Raise(this, e); }
#define DEFINE_SPLITTER_CANCEL_EVENT(name) Event<SplitterCancelEventArgs> on##name; virtual void On##name(SplitterCancelEventArgs* e) { on##name.Raise(this, e); }

/***********
* Controls *
***********/

struct SplitterPanel : Panel {
public:
    // Properties

    // Events

    SplitterPanel() : Panel() {

    }
};

struct SplitContainer : Panel {
    // Properties
    DEFINE_PROPERTY_NOSETF(bool, Panel1Collapsed, SplitContainer);
    DEFINE_PROPERTY_NOSETF(bool, Panel2Collapsed, SplitContainer);
    DEFINE_PROPERTY_NOSETF(int, Panel1MinSize, SplitContainer);
    DEFINE_PROPERTY_NOSETF(int, Panel2MinSize, SplitContainer);
    DEFINE_PROPERTY_NOSETF(int, SplitterDistance, SplitContainer); // Gets or sets the location of the splitter, in pixels, from the left or top edge of the SplitContainer.
    DEFINE_PROPERTY_NOSETF(int, SplitterIncrement, SplitContainer); // Gets or sets a value representing the increment of splitter movement in pixels.
    DEFINE_PROPERTY_NOSETF(int, SplitterWidth, SplitContainer); // Gets or sets the width of the splitter in pixels.

public:
    // Fields
    SplitterPanel* Panel1 = NULL;
    SplitterPanel* Panel2 = NULL;
    SplitOrientation Orientation = SplitOrientation::Horizontal;
    bool IsSplitterFixed = false;
    SplitPanelFix FixedPanel = SplitPanelFix::None;

    // Events
    DEFINE_SPLITTER_EVENT(SplitterMoved);
    DEFINE_SPLITTER_EVENT(SplitterMoving);

    SplitContainer() : Panel() {
        Panel1 = new SplitterPanel();
        Panel1->Parent = this;
        Panel2 = new SplitterPanel();
        Panel2->Parent = this;

        internal_Panel1Collapsed = false;
        internal_Panel2Collapsed = false;
        internal_Panel1MinSize = 25;
        internal_Panel2MinSize = 25;
        internal_SplitterDistance = 100;
        internal_SplitterIncrement = 1;
        internal_SplitterWidth = 4;

        CursorDefault = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        CursorSizeNS = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
        CursorSizeWE = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    }
	~SplitContainer() {
        delete Panel1;
        delete Panel2;
    }

    virtual SDL_Rect GetThumbBounds() {
        Position screenPos = GetPositionInWindowCoords();
        if (Orientation == SplitOrientation::Horizontal)
            return { screenPos.X + internal_SplitterDistance, screenPos.Y, internal_SplitterWidth, internal_Size.H };
        else
            return { screenPos.X, screenPos.Y + internal_SplitterDistance, internal_Size.W, internal_SplitterWidth };
    }
    virtual void OnMouseDown(MouseEventArgs* e) {
        if (IsSplitterFixed)
            return;

        SDL_Point mousePos { e->X, e->Y };
        SDL_Rect thumbArea = GetThumbBounds();

        if (SDL_PointInRect(&mousePos, &thumbArea)) {
            if (CaptureMouse()) {
                DragCursorStart = mousePos;
                DragStarted = true;
                ValueStart = internal_SplitterDistance;
            }
        }
    }
    virtual void OnMouseMove(MouseEventArgs* e) {
        if (IsSplitterFixed)
            return;

        SDL_Point mousePos { e->X, e->Y };
        SDL_Rect thumbArea = GetThumbBounds();

        auto setCursor = Orientation == SplitOrientation::Horizontal ? CursorSizeWE : CursorSizeNS;

        if (SDL_PointInRect(&mousePos, &thumbArea)) {
            SDL_SetCursor(setCursor);
        }

        if (DragStarted) {
            int dragDelta;
            if (Orientation == SplitOrientation::Horizontal)
                dragDelta = (e->X - DragCursorStart.x);
            else
                dragDelta = (e->Y - DragCursorStart.y);
            int valueDelta = dragDelta;

            SplitterEventArgs e;
            SplitterDistance = ValueStart + valueDelta;
            OnSplitterMoving(&e);

            SDL_SetCursor(setCursor);
        }
    }
    virtual void OnMouseLeave(MouseEventArgs* e) {
        if (IsSplitterFixed)
            return;
    }
    virtual void OnMouseUp(MouseEventArgs* e) {
        if (IsSplitterFixed)
            return;

        if (DragStarted) {
            UncaptureMouse();
        }
        DragStarted = false;
    }

    virtual void ResizeChildren() {
        Panel::ResizeChildren();

        if (Orientation == SplitOrientation::Horizontal)
            internal_SplitterDistance = M_CLAMP(internal_SplitterDistance, Panel1MinSize, Size.Get().W - Panel2MinSize - SplitterWidth);
        else
            internal_SplitterDistance = M_CLAMP(internal_SplitterDistance, Panel1MinSize, Size.Get().H - Panel2MinSize - SplitterWidth);

        ::Size containerSize = internal_Size;

        if (Orientation == SplitOrientation::Horizontal) {
            if (Panel1) {
                Panel1->Size = { internal_SplitterDistance, containerSize.H };
                Panel1->Location = { 0, 0 };

                Panel1->ResizeChildren();
            }
            if (Panel2) {
                Panel2->Size = { containerSize.W - (internal_SplitterDistance + internal_SplitterWidth), containerSize.H };
                Panel2->Location = { internal_SplitterDistance + internal_SplitterWidth, 0 };

                Panel2->ResizeChildren();
            }
        }
        else {
            if (Panel1) {
                Panel1->Size = { containerSize.W, internal_SplitterDistance };
                Panel1->Location = { 0, 0 };

                Panel1->ResizeChildren();
            }
            if (Panel2) {
                Panel2->Size = { containerSize.W, containerSize.H - (internal_SplitterDistance + internal_SplitterWidth) };
                Panel2->Location = { 0, internal_SplitterDistance + internal_SplitterWidth };

                Panel2->ResizeChildren();
            }
        }
    }
    virtual void HandleSDLEvent(SDL_Event* e) {
        if (Panel1)
            Panel1->HandleSDLEvent(e);
        if (Panel2)
            Panel2->HandleSDLEvent(e);

        Panel::HandleSDLEvent(e);
    }
    virtual void Update() {
        if (Panel1)
            Panel1->Update();
        if (Panel2)
            Panel2->Update();

        Panel::Update();
    }
    virtual void Render() {
        Panel::Render();

        if (Panel1)
            Panel1->Render();
        if (Panel2)
            Panel2->Render();

        SDL_Rect thumb = GetThumbBounds();
        if (DragStarted) {
            UI::Graphics::Renderer::DrawRect(&thumb, Color(0x000000, 0x10));
        }
    }

protected:
    // Setters
    virtual void set_Size(::Size value) {
        auto oldSize = internal_Size;

        Panel::set_Size(value);

        if (Orientation == SplitOrientation::Horizontal) {
            switch (FixedPanel) {
            case SplitPanelFix::None:
                internal_SplitterDistance = (internal_SplitterDistance * value.W / oldSize.W);
                break;
            case SplitPanelFix::Panel1:
                // Do nothing.
                break;
            case SplitPanelFix::Panel2:
                internal_SplitterDistance = value.W - (oldSize.W - internal_SplitterDistance);
                break;
            }
        }
        else {
            switch (FixedPanel) {
            case SplitPanelFix::None:
                internal_SplitterDistance = (internal_SplitterDistance * value.H / oldSize.H);
                break;
            case SplitPanelFix::Panel1:
                // Do nothing.
                break;
            case SplitPanelFix::Panel2:
                internal_SplitterDistance = value.H - (oldSize.H - internal_SplitterDistance);
                break;
            }
        }

        ResizeChildren();
    }

    void set_Panel1Collapsed(bool value) {
        internal_Panel1Collapsed = value;
    }
    void set_Panel2Collapsed(bool value) {
        internal_Panel2Collapsed = value;
    }
    void set_Panel1MinSize(int value) {
        internal_Panel1MinSize = M_MAX(value, 0);
    }
    void set_Panel2MinSize(int value) {
        internal_Panel2MinSize = M_MAX(value, 0);
    }
    void set_SplitterDistance(int value) {
        internal_SplitterDistance = value;

        ResizeChildren();
    }
    void set_SplitterIncrement(int value) {
        internal_SplitterIncrement = value;
    }
    void set_SplitterWidth(int value) {
        internal_SplitterWidth = value;

        ResizeChildren();
    }

private:
    int ValueStart = 0;
    bool DragStarted = false;
    SDL_Point DragCursorStart;

    SDL_Cursor* CursorDefault = NULL;
    SDL_Cursor* CursorSizeWE = NULL;
    SDL_Cursor* CursorSizeNS = NULL;
};

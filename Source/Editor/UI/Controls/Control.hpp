#pragma once

#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <UI/Components/Collections.hpp>

#include <vector>
#include <stack>
#include <functional>

template<typename T, typename C>
class Property {
public:
    using SetterType = void (C::*)(T);
    using GetterType = T (C::*)();

    Property(C* theObject, SetterType theSetter, GetterType theGetter) :
        itsObject(theObject),
        itsSetter(theSetter),
        itsGetter(theGetter) {
    }

    inline operator T() {
        return (itsObject->*itsGetter)();
    }
    inline T Get() {
        return (itsObject->*itsGetter)();
    }

    inline C& operator = (T value) {
        (itsObject->*itsSetter)(value);
        return *itsObject;
    }
    inline void Set(T value) {
        (itsObject->*itsSetter)(value);
    }

protected:
    C* const itsObject;
    SetterType const itsSetter;
    GetterType const itsGetter;
};

#define DEFINE_PROPERTY_BASICF(type, name, className) \
protected: \
    type internal_##name; \
    virtual void set_##name(type value) { \
        internal_##name = value; \
    } \
    virtual type get_##name() { \
        return internal_##name; \
    } \
public: \
    Property<type, className> name { this, &className::set_##name, &className::get_##name };

#define DEFINE_PROPERTY_NOSETF(type, name, className) \
protected: \
    type internal_##name; \
    virtual type get_##name() { \
        return internal_##name; \
    } \
public: \
    Property<type, className> name { this, &className::set_##name, &className::get_##name };

#define DEFINE_PROPERTY_NOGETF(type, name, className) \
protected: \
    type internal_##name; \
    virtual void set_##name(type value) { \
        internal_##name = value; \
    } \
public: \
    Property<type, className> name { this, &className::set_##name, &className::get_##name };

#define DEFINE_PROPERTY_NOFUNC(type, name, className) \
protected: \
    type internal_##name; \
public: \
    Property<type, className> name { this, &className::set_##name, &className::get_##name };

struct Spacing {
public:
    int Left, Top, Right, Bottom;

    Spacing() {
        Left = Top = Right = Bottom = 0;
    }
    Spacing(int value) {
        Left = Top = Right = Bottom = value;
    }

    int Horizontal() const { return Left + Right; }
    int Vertical() const { return Top + Bottom; }
};
struct Position {
    int X;
    int Y;

    Position operator + (Position b) {
        return { this->X + b.X, this->Y + b.Y };
    }
    Position operator - (Position b) {
        return { this->X - b.X, this->Y - b.Y };
    }
    Position& operator += (Position b) {
        this->X += b.X;
        this->Y += b.Y;
        return *this;
    }
    Position& operator -= (Position b) {
        this->X -= b.X;
        this->Y -= b.Y;
        return *this;
    }
};
struct Size {
    int W;
    int H;

    Size operator + (Size b) {
        return { this->W + b.W, this->H + b.H };
    }
    Size operator - (Size b) {
        return { this->W - b.W, this->H - b.H };
    }
    void operator += (Size b) {
        this->W += b.W;
        this->H += b.H;
    }
    void operator -= (Size b) {
        this->W -= b.W;
        this->H -= b.H;
    }
};

struct Control;
struct EventArgs {

};
typedef void(*BaseEventHandler)(void* sender, EventArgs* args);

template <class EventArgsType>
struct Event {
    typedef std::function<void(void* sender, EventArgsType* args)> CustomEventHandler;

    struct EventHandler {
        CustomEventHandler Function;
        EventHandler() { }
        EventHandler(CustomEventHandler func) {
            Function = func;
        }
    };
    std::vector<EventHandler*> Subscribers;

    // Adds a new request.
    void operator += (const CustomEventHandler D) {
        Subscribers.push_back(new EventHandler(D));
    }
    void operator += (EventHandler* ev) {
        // Prevent duplicates
        for (size_t i = 0, iSz = Subscribers.size(); i < iSz; i++) {
            if (Subscribers[i] == ev)
                return;
        }
        Subscribers.push_back(ev);
    }
    void operator -= (EventHandler* ev) {
        for (size_t i = 0, iSz = Subscribers.size(); i < iSz; i++) {
            Subscribers.erase(Subscribers.begin() + i);
            break;
        }
    }

    // Raises event.
    template <typename M>
    void Raise(M* sender, EventArgsType* args) {
        for (size_t i = 0, iSz = Subscribers.size(); i < iSz; i++) {
            Subscribers[i]->Function((void*)sender, args);
        }
    }
};

enum DockStyle {
    DOCK_NONE,
    DOCK_TOP,
    DOCK_LEFT,
    DOCK_RIGHT,
    DOCK_BOTTOM,
    DOCK_FILL,
};
enum AnchorStyle {
    ANCHOR_NONE = 0,
    ANCHOR_TOP = 1,
    ANCHOR_LEFT = 2,
    ANCHOR_RIGHT = 4,
    ANCHOR_BOTTOM = 8,
};
enum AutoSizeMode {
    AUTOSIZEMODE_GROWONLY = 0,
    AUTOSIZEMODE_GROWANDSHRINK = 1,
};
enum class Orientation {
    Horizontal, // The control or element is oriented horizontally.
    Vertical, // The control or element is oriented vertically.
};

struct ControlCollection {
    Control* Owner;
    std::vector<Control*> Items;
    void Add(Control* control);
    void Clear();
    int  IndexOf(Control* control);
    bool Contains(Control* control);
    Control* Last();
    void RemoveAt(int index);
    void Remove(Control* control);
    int  Count();

    void Sort();
};

struct MouseEventArgs : EventArgs {
    int Button;
    int Clicks;
    int Delta;
    int X;
    int Y;
    int Modifier;
};
struct KeyEventArgs : EventArgs {
    int Pressed;
    int Repeat;
    SDL_Scancode Scancode;
    SDL_Keycode Keycode;
    int Modifier;
};
struct TextEventArgs : EventArgs {
    char Text[32];
    int Start;
    int Length;
};

#define DEFINE_SIMPLE_EVENT(name) Event<EventArgs> on##name; virtual void On##name(EventArgs* e) { if (CanRaiseEvents) on##name.Raise(this, e); }
#define DEFINE_MOUSE_EVENT(name) Event<MouseEventArgs> on##name; virtual void On##name(MouseEventArgs* e) { if (CanRaiseEvents) on##name.Raise(this, e); }
#define DEFINE_KEY_EVENT(name) Event<KeyEventArgs> on##name; virtual void On##name(KeyEventArgs* e) { if (CanRaiseEvents) on##name.Raise(this, e); }
#define DEFINE_TEXT_EVENT(name) Event<TextEventArgs> on##name; virtual void On##name(TextEventArgs* e) { if (CanRaiseEvents) on##name.Raise(this, e); }

void CreateShapeTexture_EllipseStroke(SDL_Texture** texture, int width, int height);
void CreateShapeTexture_EllipseFill(SDL_Texture** texture, int width, int height);
void CreateShapeTexture_Radio(SDL_Texture** texture, int width, int height);
void CreateShapeTexture_RoundRectFill(SDL_Texture** texture, int width, int height, int c0, int c1, int c2, int c3);
void CreateShapeTexture_TriangleStroke(SDL_Texture** texture, int width, int height);
void CreateShapeTexture_TriangleFill(SDL_Texture** texture, int width, int height);

struct Control {
protected:
    bool MouseOver = false;
public:
    // Appearance
    Color BackColor = Color(0xCCCCCC, 0xFF);
    SDL_Cursor* Cursor = NULL;
    Color ForeColor = Color(0xFFFFFF, 0xFF);
    bool RightToLeft = false;
    int ZIndex = 0;
    bool DoZSorting = false;

    // Behavior
    bool AllowDrop = false;
    bool CanFocus = false;
    bool CanRaiseEvents = true;
    bool Enabled = false; // Gets or sets a value indicating whether the control can respond to user interaction.
    bool Focused = false; // Gets a value indicating whether the control has input focus.
    int TabIndex; // Gets or sets the tab order of the control within its container.
    bool TabStop = false; // Gets or sets a value indicating whether the user can give the focus to this control using the TAB key.
    bool Visible = false; // Gets or sets a value indicating whether the control and all its child controls are displayed.
    String ToolTipText = { };

    // Data
    ControlCollection Controls; // Gets the collection of controls contained within the control.

    // Layout
    int Anchor = ANCHOR_NONE;
    bool AutoSize = false;
    int AutoSizeMode = AUTOSIZEMODE_GROWONLY;
    int Dock = DOCK_FILL; // Gets or sets which control borders are docked to its parent control and determines how a control is resized with its parent.
    Position Location; // Gets or sets the location of the control including its nonclient elements, in pixels, relative to the parent control.
    Spacing Margin = 1; // Gets or sets the space between controls.
    ::Size MaximumSize = { 0, 0 };
    ::Size MinimumSize = { 0, 0 };
    Spacing Padding = 0; // Gets or sets padding within the control.
    Position ScrollLocation = { 0, 0 };
    DEFINE_PROPERTY_NOFUNC(::Size, Size, Control); // Gets or sets the size of the control.

    bool LineBreak = false;

    Control* Parent = NULL;
    bool ClickStart = false;

    static Control* MouseCaptured;
    static Control* FocusCaptured;
    static SDL_Cursor* CursorToSet;

    // Events
    DEFINE_SIMPLE_EVENT(ParentChanged);
    DEFINE_SIMPLE_EVENT(Click);
    DEFINE_SIMPLE_EVENT(DoubleClick);
    DEFINE_SIMPLE_EVENT(FocusLost);

    DEFINE_MOUSE_EVENT(MouseDown);
    DEFINE_MOUSE_EVENT(MouseClick);
    DEFINE_MOUSE_EVENT(MouseUp);
    DEFINE_MOUSE_EVENT(MouseDoubleClick);
    DEFINE_MOUSE_EVENT(MouseMove);
    DEFINE_MOUSE_EVENT(MouseEnter);
    DEFINE_MOUSE_EVENT(MouseLeave);
    DEFINE_MOUSE_EVENT(MouseWheel);

    DEFINE_TEXT_EVENT(TextEdited);
    DEFINE_TEXT_EVENT(TextInputted);

    DEFINE_KEY_EVENT(KeyDown);
    DEFINE_KEY_EVENT(KeyUp);

    Control();
    virtual ~Control() {
        if (FocusCaptured == this) {
            // FocusCaptured->OnFocusLost(NULL);
            FocusCaptured = NULL;
        }

        SDL_FreeCursor(Cursor);
    }

    Position GetPositionInWindowCoords() {
        ::Size size = Size;
        Position position = Location;
        Control* parent = Parent;
        Control* child = this;
        while (parent) {
            if (child->Anchor == ANCHOR_NONE)
                position.X = parent->Location.X + position.X - parent->ScrollLocation.X;
            else if ((child->Anchor & ANCHOR_RIGHT) == ANCHOR_RIGHT)
                position.X = parent->Location.X + parent->Size.Get().W - position.X - size.W - parent->ScrollLocation.X;
            position.Y += parent->Location.Y - parent->ScrollLocation.Y;

            child = parent;
            parent = parent->Parent;
        }
        return position;
    }
    virtual ::Size GetContentSize() {
        ::Size contentSize = { 0, 0 };

        if (Controls.Count()) {
            for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
                auto Child = Controls.Items[i];
                ::Size ChildSize = Child->Size;

                contentSize.W = M_MAX(contentSize.W, Child->Location.X + ChildSize.W);
                contentSize.H = M_MAX(contentSize.H, Child->Location.Y + ChildSize.H);
            }
        }

        return contentSize;
    }
    SDL_Rect GetScreenRect() {
        ::Size screenSize = Size;
        Position screenPos = GetPositionInWindowCoords();

        SDL_Rect rect;
        rect.x = screenPos.X;
        rect.y = screenPos.Y;
        rect.w = screenSize.W;
        rect.h = screenSize.H;
        return rect;
    }

    static bool PositionInBounds(::Position point, ::Position boundPos, ::Size boundSize) {
        return point.X >= boundPos.X && point.X < boundPos.X + boundSize.W &&
            point.Y >= boundPos.Y && point.Y < boundPos.Y + boundSize.H;
    }

    virtual void UpdateLayout() {
        // Dock, Anchor, and position Children, but only when not AutoSizing
        Position head = { Padding.Left, Padding.Top };
        ::Size size = internal_Size - ::Size { Padding.Horizontal(), Padding.Vertical() };

        for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
            auto Child = Controls.Items[i];
            ::Size ChildSize = Child->Size;

            if (Child->Dock != DOCK_NONE && Child->Anchor == ANCHOR_NONE) {
                switch (Child->Dock) {
                case DOCK_TOP:
                    Child->Location.X = head.X;
                    Child->Location.Y = head.Y;
                    Child->Size = { size.W, ChildSize.H };

                    head.Y += ChildSize.H;
                    size.H -= ChildSize.H;
                    break;
                case DOCK_LEFT:
                    Child->Location.X = head.X;
                    Child->Location.Y = head.Y;
                    Child->Size = { ChildSize.W, size.H };

                    head.X += ChildSize.W;
                    size.W -= ChildSize.W;
                    break;
                case DOCK_BOTTOM:
                    Child->Location.X = head.X;
                    Child->Location.Y = head.Y + size.H - ChildSize.H;
                    Child->Size = { size.W, ChildSize.H };

                    size.H -= ChildSize.H;
                    break;
                case DOCK_RIGHT:
                    Child->Location.X = head.X + size.W - ChildSize.W;
                    Child->Location.Y = head.Y;
                    Child->Size = { ChildSize.W, size.H };

                    size.W -= ChildSize.W;
                    break;
                case DOCK_FILL:
                    break;
                }
            }
            else if (Child->Dock == DOCK_NONE && Child->Anchor != ANCHOR_NONE) {
                /*
                if (Child->Anchor & ANCHOR_LEFT)
                    Child->Location.X = Padding.Left;
                if (Child->Anchor & ANCHOR_TOP)
                    Child->Location.Y = Padding.Top;
                if (Child->Anchor & ANCHOR_RIGHT)
                    Child->Location.X = (value.W - Padding.Right) - ChildSize.W;
                if (Child->Anchor & ANCHOR_BOTTOM)
                    Child->Location.Y = (value.H - Padding.Bottom) - ChildSize.H;
                */
            }
        }

        for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
            auto Child = Controls.Items[i];

            // This needs to be done after the other DOCK types, so that this just takes the remaining space despite Control order
            if (Child->Dock == DOCK_FILL && Child->Anchor == ANCHOR_NONE) {
                Child->Location = head;
                Child->Size = size;

                size = { 0, 0 };
            }
        }
    }

    virtual void set_Size(::Size value) {
        if (AutoSize)
            return;

        internal_Size = value;
        UpdateLayout();
    }
    virtual ::Size get_Size() {
        ::Size outputSize = internal_Size;

        if (AutoSize) {
            ::Size contentSize = GetContentSize();

            if (AutoSizeMode == AUTOSIZEMODE_GROWONLY) {
                outputSize.W = M_MAX(outputSize.W, contentSize.W);
                outputSize.H = M_MAX(outputSize.H, contentSize.H);
            }
            else if (AutoSizeMode == AUTOSIZEMODE_GROWANDSHRINK) {
                outputSize = contentSize;
            }

            outputSize.W += Padding.Horizontal();
            outputSize.H += Padding.Vertical();
        }

        return outputSize;
    }

    virtual void ResizeChildren() {
        for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
            auto Child = Controls.Items[i];
            Child->ResizeChildren();
        }
    }
    virtual void HandleSDLEvent(SDL_Event* e) {
        if (!Enabled)
            return;

        SDL_Point mousePos;
        bool mouseOver;
        SDL_Rect boundsInWindow = GetScreenRect();

        switch (e->type) {
        case SDL_MOUSEBUTTONDOWN:
            mousePos = { e->button.x, e->button.y };
            if (SDL_PointInRect(&mousePos, &boundsInWindow) || MouseCaptured) {
                MouseEventArgs ev;
                ev.Button = SDL_BUTTON(e->button.button);
                ev.Clicks = e->button.clicks;
                ev.Delta = 0;
                ev.X = mousePos.x;
                ev.Y = mousePos.y;
                ev.Modifier = SDL_GetModState();

                if (!MouseCaptured || MouseCaptured == this) {
                    if (CanFocus) {
                        if (FocusCaptured != NULL && FocusCaptured != this) {
                            FocusCaptured->OnFocusLost(NULL);
                        }

                        FocusCaptured = this;
                    }

                    ClickStart = true;
                    OnMouseDown(&ev);
                }

                for (int i = 0; i < Controls.Count(); i++) {
                    Controls.Items[i]->HandleSDLEvent(e);
                }
            }
            break;
        case SDL_MOUSEMOTION:
            mousePos = { e->motion.x, e->motion.y };
            mouseOver = SDL_PointInRect(&mousePos, &boundsInWindow);
            if (mouseOver) {
                if (!MouseOver) {
                    MouseEventArgs ev;
                    ev.Button = SDL_GetMouseState(NULL, NULL);
                    ev.Clicks = 0;
                    ev.Delta = 0;
                    ev.X = mousePos.x;
                    ev.Y = mousePos.y;
                    ev.Modifier = SDL_GetModState();

                    OnMouseEnter(&ev);

                    ToolTipTimerStart = SDL_GetTicks();
                }
            }
            else if (MouseOver) {
                MouseEventArgs ev;
                ev.Button = SDL_GetMouseState(NULL, NULL);
                ev.Clicks = 0;
                ev.Delta = 0;
                ev.X = mousePos.x;
                ev.Y = mousePos.y;
                ev.Modifier = SDL_GetModState();

                OnMouseLeave(&ev);
            }
            MouseOver = mouseOver;
            if (!MouseOver)
                ToolTipTimerStart = 0;

            if (mouseOver || MouseCaptured) {
                MouseEventArgs ev;
                ev.Button = SDL_GetMouseState(NULL, NULL);
                ev.Clicks = 0;
                ev.Delta = 0;
                ev.X = mousePos.x;
                ev.Y = mousePos.y;
                ev.Modifier = SDL_GetModState();

                if (!MouseCaptured || MouseCaptured == this) {
                    OnMouseMove(&ev);
                }
            }

            for (int i = 0; i < Controls.Count(); i++) {
                Controls.Items[i]->HandleSDLEvent(e);
            }
            break;
        case SDL_MOUSEBUTTONUP:
            mousePos = { e->button.x, e->button.y };
            mouseOver = SDL_PointInRect(&mousePos, &boundsInWindow);
            if (mouseOver || MouseCaptured) {
                MouseEventArgs ev;
                ev.Button = SDL_BUTTON(e->button.button);
                ev.Clicks = e->button.clicks;
                ev.Delta = 0;
                ev.X = mousePos.x;
                ev.Y = mousePos.y;
                ev.Modifier = SDL_GetModState();

                if (!MouseCaptured || MouseCaptured == this) {
                    if (ClickStart && mouseOver) {
                        OnClick(NULL);

                        OnMouseClick(&ev);

                        ClickStart = false;
                    }
                    OnMouseUp(&ev);
                }

                for (int i = 0; i < Controls.Count(); i++) {
                    Controls.Items[i]->HandleSDLEvent(e);
                }
            }
            break;
        case SDL_MOUSEWHEEL:
            SDL_GetMouseState(&mousePos.x, &mousePos.y);
            if (SDL_PointInRect(&mousePos, &boundsInWindow) || MouseCaptured) {
                MouseEventArgs ev;
                ev.Button = 0;
                ev.Clicks = 0;
                ev.Delta = e->wheel.y;
                ev.X = mousePos.x;
                ev.Y = mousePos.y;
                ev.Modifier = SDL_GetModState();

                if (!MouseCaptured || MouseCaptured == this) {
                    OnMouseWheel(&ev);
                }

                for (int i = 0; i < Controls.Count(); i++) {
                    Controls.Items[i]->HandleSDLEvent(e);
                }
            }
            break;
        case SDL_KEYDOWN:
            if (FocusCaptured == this || MouseCaptured) {
                KeyEventArgs ev;
                ev.Pressed = e->key.state == SDL_PRESSED;
                ev.Repeat = e->key.repeat;
                ev.Scancode = e->key.keysym.scancode;
                ev.Keycode = e->key.keysym.sym;
                ev.Modifier = e->key.keysym.mod;

                OnKeyDown(&ev);
            }

            for (int i = 0; i < Controls.Count(); i++) {
                Controls.Items[i]->HandleSDLEvent(e);
            }
            break;
        case SDL_KEYUP:
            if (FocusCaptured == this || MouseCaptured) {
                KeyEventArgs ev;
                ev.Pressed = e->key.state == SDL_PRESSED;
                ev.Repeat = e->key.repeat;
                ev.Scancode = e->key.keysym.scancode;
                ev.Keycode = e->key.keysym.sym;
                ev.Modifier = e->key.keysym.mod;

                OnKeyUp(&ev);
            }

            for (int i = 0; i < Controls.Count(); i++) {
                Controls.Items[i]->HandleSDLEvent(e);
            }
            break;
        case SDL_TEXTINPUT:
            if (FocusCaptured == this) {
                TextEventArgs ev;
                ev.Start = 0;
                ev.Length = (int)strlen(e->edit.text);
                memcpy(ev.Text, e->edit.text, 32);

                OnTextInputted(&ev);
            }

            for (int i = 0; i < Controls.Count(); i++) {
                Controls.Items[i]->HandleSDLEvent(e);
            }
            break;
        case SDL_TEXTEDITING:
            if (FocusCaptured == this) {
                TextEventArgs ev;
                ev.Start = e->edit.start;
                ev.Length = e->edit.length;
                memcpy(ev.Text, e->edit.text, 32);

                OnTextEdited(&ev);
            }

            for (int i = 0; i < Controls.Count(); i++) {
                Controls.Items[i]->HandleSDLEvent(e);
            }
            break;
        }
    }
    virtual void Update() {
        if (ToolTipTimerStart != 0 && SDL_GetTicks() - ToolTipTimerStart > ToolTipWaitDuration) {
            ShowToolTip();
        }

        for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
            auto Child = Controls.Items[i];
            Child->Update();
        }
    }
    virtual void Render();

    void ShowToolTip();
    void SetToolTipText(const char* text);

    void SetCursor(SDL_Cursor* cursor) {
        CursorToSet = cursor;
    }
    bool CaptureMouse() {
        if (SDL_CaptureMouse(SDL_TRUE) < 0) {
            fprintf(stderr, "SDL_CaptureMouse failed: %s\n", SDL_GetError());
            return false;
        }

        MouseCaptured = this;
        return true;
    }
    void UncaptureMouse() {
        if (SDL_CaptureMouse(SDL_FALSE) < 0) {
            fprintf(stderr, "SDL_CaptureMouse failed: %s\n", SDL_GetError());
        }

        MouseCaptured = NULL;
    }

    void ClipStart(SDL_Rect* buffer, SDL_Rect* clip);
    void ClipEnd(SDL_Rect* buffer);
private:
    Uint32 ToolTipTimerStart = 0;
    Uint32 ToolTipWaitDuration = 1000;
};

struct Stream;

struct Command {
    int SiblingID = -1;
    bool IsDataChange = false;

    virtual void Do() = 0;
    virtual void Undo() = 0;
    virtual void Read(Stream* stream) = 0;
    virtual void Write(Stream* stream) = 0;
    virtual Uint32 GetID() = 0;
	virtual ~Command() { };
};

struct UndoRedoStack {
    std::stack<Command*> _undo;
    std::stack<Command*> _redo;

    void Reset() {
        while (_undo.size() > 0) {
            delete _undo.top();
            _undo.pop();
        }

        while (_redo.size() > 0) {
            delete _redo.top();
            _redo.pop();
        }
    }

    UndoRedoStack() {
        Reset();
    }
    ~UndoRedoStack() {
        Reset();
    }

    int UndoCount() {
        return (int)_undo.size();
    }
    int RedoCount() {
        return (int)_redo.size();
    }

    void Do(Command* cmd, int siblingID) {
        cmd->Do();
        cmd->SiblingID = siblingID;
        _undo.push(cmd);

        while (_redo.size() > 0) {
            delete _redo.top();
            _redo.pop();
        }
    }

    void Undo() {
        if (_undo.size() > 0) {
            Command* cmd = _undo.top();
            _undo.pop();

            cmd->Undo();
            _redo.push(cmd);

            // Undo next sibling as well.
            int siblingID = cmd->SiblingID;
            if (siblingID != -1 && _undo.size() > 0) {
                Command* nextCmd = _undo.top();
                if (nextCmd->SiblingID == siblingID) {
                    Undo();
                }
            }
        }
    }
    void Redo() {
        if (_redo.size() > 0) {
            Command* cmd = _redo.top();
            _redo.pop();

            cmd->Do();
            _undo.push(cmd);

            // Redo next sibling as well.
            int siblingID = cmd->SiblingID;
            if (siblingID != -1 && _redo.size() > 0) {
                Command* nextCmd = _redo.top();
                if (nextCmd->SiblingID == siblingID) {
                    Redo();
                }
            }
        }
    }
};

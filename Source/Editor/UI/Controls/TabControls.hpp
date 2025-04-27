#pragma once

#include "ScrollBar.hpp"

/********************
* Enums / Constants *
********************/

enum class TabAlignment {
    Top,
    Bottom,
    Left,
    Right,
};

/******************
* Event Arg Types *
******************/

/***********
* Controls *
***********/

struct TabControl;

struct TabPage : Panel {
public:
    // Properties
    String Title;

    // Events

    TabPage();
    TabPage(CString title);

    void SetTitle(CString title);
    void SetTitle(String* title);
};

struct TabPageCollection : List<TabPage*> {
    TabControl* Owner;

    virtual void Add(TabPage* item);
    virtual void Insert(int index, TabPage* item);
    virtual void RemoveAt(int index);
    virtual bool IsFixedSize() const noexcept { return false; };
    virtual bool IsReadOnly() const noexcept { return false; };
};

struct TabControl : Control {
public:
    // Behavior
    TabAlignment Alignment = TabAlignment::Bottom;
    TabPageCollection TabPages;
    Position TabRackScroll = { 0, 0 };
    int SelectedIndex = 0;

    // Events
    DEFINE_SIMPLE_EVENT(Selected);

    int MinSize = 20;
    int MaxSize = 100;

    TabControl() : Control() {
        TabPages.Owner = this;

        BackColor = Color(0x000000, 0);
    }

    ::Size GetDefaultTabSize() {
        if (TabPages.Count() > 0)
            return { M_CLAMP(internal_Size.W / TabPages.Count(), MinSize, MaxSize), 30 };

        return { MaxSize, 30 };
    }

    void GetTabRackLayout(Position* position, ::Size* size);
    TabPage* GetCurrentTabPage();

    virtual void Select(int index);

    virtual void OnTabMouseDown(MouseEventArgs* e, int tabIndex);
    virtual void OnTabMouseMove(MouseEventArgs* e, int tabIndex);
    virtual void OnTabMouseUp(MouseEventArgs* e, int tabIndex);

    virtual void OnMouseWheel(MouseEventArgs* e);
    virtual void OnMouseDown(MouseEventArgs* e);
    virtual void OnMouseMove(MouseEventArgs* e);
    virtual void OnMouseLeave(MouseEventArgs* e);
    virtual void OnMouseUp(MouseEventArgs* e);

    virtual void Update();
    virtual void ResizeChildren();
    virtual void HandleSDLEvent(SDL_Event* e);
    virtual void Render();

protected:
    // Setters
    virtual void set_Size(::Size value) {
        Control::set_Size(value);

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

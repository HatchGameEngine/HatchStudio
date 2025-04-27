#pragma once

#include "Control.hpp"

// Structures
struct ToolStripItem;

struct ToolStripItemCollection : List<ToolStripItem*> {
    bool IsFixedSize() const noexcept {
        return false;
    }
    bool IsReadOnly() const noexcept {
        return false;
    }
};

// Events and EventArgs
struct ToolStripItemEventArgs : EventArgs {
    ToolStripItem* Item;
};
#define DEFINE_TOOLSTRIP_EVENT(name) Event<ToolStripItemEventArgs> on##name; virtual void On##name(ToolStripItemEventArgs* e) { on##name.Raise(this, e); }

// Controls
struct ToolStripItem : Control {
public:
    ToolStripItem() : Control() {
        Margin = 1;
        BackColor = Color(0x000000, 0x00);
    }
};

struct ToolStripSeparator : ToolStripItem {
public:
    ToolStripSeparator() : ToolStripItem() {
        Margin = 4;
        BackColor = Color(0xFFFFFF, 0x30);
        Size = { 2, 0 };
    }

    void Render();
};

struct ToolStripButton : ToolStripItem {
public:
    // Properties
    SDL_Texture* Icon = NULL;
    ::Size IconSize;

    String Text;

    bool CanSelect = false;
    bool Checked = false;
    bool CheckOnClick = false;

    Color BorderColor;

    ToolStripButton();

    ::Size get_Size();

    void SetText(CString title);
    void SetText(String* title);

    void OnMouseEnter(MouseEventArgs* e) {
        Hovering = true;
        ToolStripItem::OnMouseEnter(e);
    }
    void OnMouseMove(MouseEventArgs* e) {
        Hovering = true;
        ToolStripItem::OnMouseMove(e);
    }
    void OnMouseLeave(MouseEventArgs* e) {
        Hovering = false;
        ToolStripItem::OnMouseEnter(e);
    }

    void OnMouseDown(MouseEventArgs* e) {
        if (CheckOnClick)
            Checked = !Checked;
        ToolStripItem::OnMouseDown(e);
    }

    void Action_Base() {
        BackColor = Color(0x000000, 0x00);
        BorderColor = Color(0x000000, 0x00);
        ForeColor = Color(0xCCCCCC, 0xFF);
    }
    void Action_Hover() {
        BackColor = Color(0xFFFFFF, 0x10);
        BorderColor = Color(0xFFFFFF, 0x10);
        ForeColor = Color(0xFFFFFF, 0xFF);
    }
    void Action_Checked() {
        BackColor = Color(0x000000, 0x30);
        BorderColor = Color(0xFFFFFF, 0x30);
        ForeColor = Color(0x007FFF, 0xFF);
    }

    void Render();
private:
    bool Hovering = false;
};

struct ToolStrip : Control {
public:
    // Properties
    Orientation Orientation = Orientation::Horizontal;

    // Events
    DEFINE_TOOLSTRIP_EVENT(ItemAdded);
    DEFINE_TOOLSTRIP_EVENT(ItemRemoved);
    DEFINE_TOOLSTRIP_EVENT(ItemClicked);

    ToolStrip() : Control() {
        Size = { 200, 24 };
    }

    void UpdateLayout() {
        int Alignment = DOCK_LEFT;

        Position head = { Padding.Left, Padding.Top };

        ::Size size = internal_Size;
        size.W -= Padding.Horizontal();
        size.H -= Padding.Vertical();

        for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
            auto Child = Controls.Items[i];
            ::Size ChildSize = Child->Size;
            ::Size ChildSizeNew = ChildSize;

            switch (Alignment) {
            case DOCK_LEFT:
            case DOCK_RIGHT:
                ChildSizeNew.H = size.H;
                break;
            case DOCK_TOP:
            case DOCK_BOTTOM:
                ChildSizeNew.W = size.W;
                break;
            }

            switch (Alignment) {
            case DOCK_TOP:
                Child->Location.X = head.X;
                Child->Location.Y = head.Y + Child->Margin.Top;
                ChildSizeNew.W = size.W;

                head.Y += Child->Margin.Top + ChildSizeNew.H + Child->Margin.Bottom;
                size.H -= Child->Margin.Top + ChildSizeNew.H + Child->Margin.Bottom;
                break;
            case DOCK_LEFT:
                Child->Location.X = head.X + Child->Margin.Left;
                Child->Location.Y = head.Y;
                ChildSizeNew.H = size.H;

                head.X += Child->Margin.Left + ChildSizeNew.W + Child->Margin.Right;
                size.W -= Child->Margin.Left + ChildSizeNew.W + Child->Margin.Right;
                break;
            case DOCK_BOTTOM:
                Child->Location.X = head.X;
                Child->Location.Y = head.Y + size.H - ChildSizeNew.H;
                ChildSizeNew.W = size.W;

                size.H -= ChildSizeNew.H + Child->Margin.Top;
                break;
            case DOCK_RIGHT:
                Child->Location.X = head.X + size.W - ChildSizeNew.W;
                Child->Location.Y = head.Y;
                ChildSizeNew.H = size.H;

                size.W -= ChildSizeNew.W + Child->Margin.Left;
                break;
            }

            // If any of the size components have changed, update it
            if (ChildSizeNew.W != ChildSize.W || ChildSizeNew.H != ChildSize.H) {
                Child->Size = ChildSizeNew;
            }
        }
    }

    virtual void Add(ToolStripItem* item) {
        Controls.Add(item);
    }
protected:

private:
    // Setters
};

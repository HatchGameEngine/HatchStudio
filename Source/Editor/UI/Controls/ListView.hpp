#pragma once

#include "ScrollBar.hpp"

/********************
* Enums / Constants *
********************/

enum class ListViewLayout {
    List,
    Details,
};

/******************
* Event Arg Types *
******************/

/***********
* Controls *
***********/

struct ListView;

struct ColumnHeader {
public:
    // Properties
    String Text;
    int Width = 100;
    int DisplayWidth = 100;
    int DataIndex = 0;

    ColumnHeader(CString text, int width, int index);
    ColumnHeader(String* text, int width, int index);
    void SetText(CString text);
    void SetText(String* text);
};

struct ColumnHeaderCollection : List<ColumnHeader*> {
    virtual bool IsFixedSize() const noexcept { return false; };
    virtual bool IsReadOnly() const noexcept { return false; };
};

struct ListViewSubItem {
public:
    // Properties
    String Text;

    ListViewSubItem(CString text);
    ListViewSubItem(String* text);
    void SetText(CString text);
    void SetText(String* text);
};

struct ListViewSubItemCollection : List<ListViewSubItem*> {
    virtual bool IsFixedSize() const noexcept { return false; };
    virtual bool IsReadOnly() const noexcept { return false; };
};

struct ListViewItem {
public:
    // Properties
    String Text;
    ListViewSubItemCollection SubItems;

    ListViewItem(CString text);
    ListViewItem(String* text);
    void SetText(CString text);
    void SetText(String* text);
};

struct ListViewItemCollection : List<ListViewItem*> {
    ListView* Parent = NULL;
    void Add(ListViewItem* item);
    virtual bool IsFixedSize() const noexcept { return false; };
    virtual bool IsReadOnly() const noexcept { return false; };
};

struct ListView : Panel {
public:
    // Properties
    DEFINE_PROPERTY_BASICF(int, SelectedIndex, ListView);
    DEFINE_SIMPLE_EVENT(SelectedIndexChanged);
public:
    ListViewItemCollection Items;
    ColumnHeaderCollection Columns;

    // Events
    // DEFINE_SPLITTER_EVENT(SelectedIndexChanged);
    Color Highlight;
    int ItemSize = 20;
    int HeaderSize = 25;
    ListViewLayout LayoutType = ListViewLayout::Details;

    ListView() : Panel() {
        BackColor = Color(0x1C1E24, 0xFF);
        ForeColor = Color(0xFFFFFF, 0xFF);
        Highlight = Color(0x007FFF, 0xFF);

        DoHScroll = false;
        DoVScroll = true;

        HideEmptyVScroll = true;

		SelectedIndex = -1;

        // HScrollControl->DoEasing = true;
        // VScrollControl->DoEasing = true;

        Items.Parent = this;
    }
    ~ListView() {
        for (int i = 0; i < Columns.Count(); i++)
            delete Columns[i];

        for (int i = 0; i < Items.Count(); i++) {
            for (int s = 0; s < Items[i]->SubItems.Count(); s++) {
                delete Items[i]->SubItems[s];
            }
            delete Items[i];
        }
    }

    void Select(int index);

    void OnMouseDown(MouseEventArgs* e);

    virtual ::Size GetContentSize();
    virtual void HandleSDLEvent(SDL_Event* e);
    virtual void ResizeChildren();
    virtual void Render();

protected:
    // Setters
    virtual void set_Size(::Size value) {
        Control::set_Size(value);

        ResizeChildren();
    }

private:
};

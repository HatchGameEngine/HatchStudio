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

struct TextboxBase : Control {
public:
    bool AcceptsTab = false; // Gets or sets a value indicating whether pressing the TAB key in a multiline text box control types a TAB character in the control instead of moving the focus to the next control in the tab order.
    bool CanEnableIME = false; // Gets a value indicating whether the ImeMode property can be set to an active value, to enable IME support.      false if the ReadOnly property is true or if this TextBoxBase class is set to use a password mask character; otherwise, true.
    bool CanUndo = true;
    bool HideSelection = true; // Gets or sets a value indicating whether the selected text in the text box control remains highlighted when the control loses focus.
    int MaxLength = 32767;
    bool Modified = false; // Gets or sets a value that indicates that the text box control has been modified by the user since the control was created or its contents were last set.
    bool Multiline = false;
    int PreferredHeight = 0; // The size returned by this property is based on the font height and border style of the text box.
    bool ReadOnly = false; //
    String SelectedText;
    int SelectionCursor = 0;
    int SelectionLength = 0;
    int SelectionStart = 0;
    bool ShortcutsEnabled = true; // https://docs.microsoft.com/en-us/dotnet/api/system.windows.forms.textboxbase.shortcutsenabled?view=netframework-4.8
    String Text = { };
    bool WordWrap = true;

    Color Highlight;
    String* TextPtr = NULL;

    // Events
    DEFINE_SIMPLE_EVENT(TextChanged);
    DEFINE_SIMPLE_EVENT(TextCommitted);

    TextboxBase();
    TextboxBase(CString string);
    TextboxBase(String* string);

    ::Size get_Size();
    int MouseToTextCursorPosition(Position* mousePos);
    void TextCursorToLocation(int cursorPos, float* drawPosX);

    void InsertText(int position, CString string, size_t length);
    void RemoveText(int position, size_t length);

    void SetCursorPosition(int position);
    void HighlightBetweenCursorPositions();
    void ScrollToCursor();

    void OnClick(EventArgs* e);
    void OnDoubleClick(EventArgs* e);

    void OnMouseDown(MouseEventArgs* e);
    void OnMouseClick(MouseEventArgs* e);
    void OnMouseUp(MouseEventArgs* e);
    void OnMouseDoubleClick(MouseEventArgs* e);
    void OnMouseMove(MouseEventArgs* e);
    void OnMouseEnter(MouseEventArgs* e);
    void OnMouseLeave(MouseEventArgs* e);
    void OnMouseWheel(MouseEventArgs* e);

    void OnTextInputted(TextEventArgs* e);
    void OnTextEdited(TextEventArgs* e);

    void OnKeyDown(KeyEventArgs* e);
    void OnKeyUp(KeyEventArgs* e);

    void OnFocusLost(EventArgs* e);

    void Render();

private:
    int LastSelectionCursor = 0;
};

#pragma once

#include "Control.hpp"

/********************
* Enums / Constants *
********************/

struct Shortcut {
    int ActivatorKey = 0;
    int ActivatorModifier = 0;
    Control* Owner = NULL;
    bool NeedsFocus = true;
    std::function<void()> Action;
};
enum ShortcutModifiers {
    SMOD_NONE = 0,

    SMOD_CTRL = 1,
    SMOD_SHIFT = 2,
    SMOD_ALT = 4,

    SMOD_CTRL_SHIFT = 3,
    SMOD_CTRL_ALT = 5,
    SMOD_SHIFT_ALT = 6,

    SMOD_CTRL_SHIFT_ALT = 7,
};

/******************
* Event Arg Types *
******************/

/***********
* Controls *
***********/

enum class DialogResult {
    None,
    OK,
    Cancel,
    Abort,
    Retry,
    Ignore,
    Yes,
    No,
};
enum class CloseReason {
    None,
    WindowsShutDown,
    UserClosing,
    TaskManagerClosing,
    FormOwnerClosing,
    ApplicationExitCall,
};
typedef std::function<void(DialogResult)> DialogCallback;

struct FormClosedEventArgs : EventArgs {
    CloseReason Reason;
};
struct FormClosingEventArgs : EventArgs {
    bool Cancel;
    CloseReason Reason;
};

#define DEFINE_EVENT(name, type) Event<type> on##name; virtual void On##name(type* e) { if (CanRaiseEvents) on##name.Raise(this, e); }

struct Form : Control {
    Uint32 ID;
    bool SignalClose = false;

    DialogCallback CloseCallback;
    DialogResult Result = DialogResult::None;

    String Title;
    bool MainForm = false;

    bool IsDialog = false;

    List<Shortcut*> Shortcuts;

    DEFINE_EVENT(Resized, EventArgs);
    DEFINE_EVENT(Closed, FormClosedEventArgs);
    DEFINE_EVENT(Closing, FormClosingEventArgs);

    Form(int w, int h, const char* title, Uint32 flag = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

    // Events
    virtual void HandleSDLEvent(SDL_Event* e);

    // Uniques
    void SetTitle(CString title);
    void SetTitle(String* title);

    void RenderClear();
    void RenderPresent();

    virtual void Load();
    void Close();

    void Show();
    void ShowDialog(Form* parent, DialogCallback callback);

    void AdjustSize(Control* panel);

    void CheckShortcuts(SDL_Keycode key, SDL_Keymod mod);
    Shortcut* RegisterShortcut(int modifiers, int keycode, Control* owner, bool needsFocus, std::function<void()> action);
};

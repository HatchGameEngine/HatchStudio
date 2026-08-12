#include "Form.hpp"
#include <UI/Graphics/Renderer.hpp>

#include <Hatch/Strings.h>

Form::Form(int w, int h, const char* title, Uint32 flag) : Control() {
    Form* form = this;

    form->CloseCallback = NULL;

    form->Size = { w, h };

    CanFocus = false;
}

// Events
void Form::HandleSDLEvent(SDL_Event* e) {
    switch (e->type) {
    case SDL_KEYDOWN:
        CheckShortcuts(e->key.keysym.sym, (SDL_Keymod)e->key.keysym.mod);
        break;
    }

    for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
        Controls.Items[i]->HandleSDLEvent(e);
    }
}

// Uniques
void Form::SetTitle(CString title) {
    Strings::FromCString(&Title, title, 0);

    if (MainForm) {
        char windowTitle[1024];
        Strings::ToCString(windowTitle, &Title);

        SDL_SetWindowTitle(UI::Graphics::Renderer::Window, windowTitle);
    }
}
void Form::SetTitle(String* title) {
    Strings::Copy(&Title, title);

    if (MainForm) {
        char windowTitle[1024];
        Strings::ToCString(windowTitle, &Title);

        SDL_SetWindowTitle(UI::Graphics::Renderer::Window, windowTitle);
    }
}

void Form::RenderClear() {

    //ViewOutput* viewOutput = &Graphics::ViewOutputs[0];
    //if (viewOutput->Active) {
    //    View* view = &Graphics::Views[viewOutput->ViewIndex];
    //    // Determine output size
    //    switch (viewOutput->ScaleType) {
    //    case VOSCALE_NONE:
    //        viewOutput->Width = view->Width;
    //        viewOutput->Height = view->Height;
    //        break;
    //    case VOSCALE_FIT_TO_SCREEN:
    //        if ((RendererW << 16) / view->Width > (RendererH << 16) / view->Height) {
    //            viewOutput->Width = view->Width * RendererH / view->Height;
    //            viewOutput->Height = RendererH;
    //        }
    //        else {
    //            viewOutput->Width = RendererW;
    //            viewOutput->Height = view->Height * RendererW / view->Width;
    //        }
    //        break;
    //    case VOSCALE_COVER_TO_SCREEN:
    //        if ((RendererW << 16) / view->Width > (RendererH << 16) / view->Height) {
    //            viewOutput->Width = RendererW;
    //            viewOutput->Height = view->Height * RendererW / view->Width;
    //        }
    //        else {
    //            viewOutput->Width = view->Width * RendererH / view->Height;
    //            viewOutput->Height = RendererH;
    //        }
    //        break;
    //    case VOSCALE_STRETCH_TO_SCREEN:
    //    case VOSCALE_RESIZE_TO_SCREEN:
    //        viewOutput->Width = RendererW;
    //        viewOutput->Height = RendererH;
    //        break;
    //    }

    //    // Determine output position
    //    if (viewOutput->ScaleType >= VOSCALE_NONE) {
    //        viewOutput->X = (RendererW - viewOutput->Width) / 2;
    //        viewOutput->Y = (RendererH - viewOutput->Height) / 2;
    //    }
    //}
}
void Form::RenderPresent() {
}

void Form::Load() {
    Strings::Init(&Title, 8);
}
void Form::Close() {
    FormClosingEventArgs fe;
    fe.Cancel = false;
    fe.Reason = CloseReason::UserClosing;
    OnClosing(&fe);

    if (!fe.Cancel) {
        if (CloseCallback != NULL)
            CloseCallback(Result);

        FormClosedEventArgs e;
        e.Reason = CloseReason::UserClosing;
        OnClosed(&e);

        SignalClose = true;
    }
}

void Form::Show() {

}
void Form::ShowDialog(Form* parent, DialogCallback callback) {
    CloseCallback = callback;
    IsDialog = true;
}

void Form::AdjustSize(Control* panel) {
    UpdateLayout();

    Size = {
        panel->Controls.Last()->Location.X + panel->Controls.Last()->Size.Get().W + panel->Padding.Right,
        panel->Controls.Last()->Location.Y + panel->Controls.Last()->Size.Get().H + panel->Padding.Bottom
    };
}

void Form::CheckShortcuts(SDL_Keycode key, SDL_Keymod mod) {
    int* modifier = (int*)&mod;
    *modifier &= ~(KMOD_CAPS | KMOD_NUM);

    for (int i = 0; i < Shortcuts.Count(); i++) {
        auto shortcut = Shortcuts[i];
        if (shortcut->ActivatorKey == key &&
            ((shortcut->ActivatorModifier == KMOD_NONE && mod == 0) || (mod & shortcut->ActivatorModifier) != 0)) {
            bool isFocusCapturedControlIsAncestorOfOwner = false;
            if (shortcut->NeedsFocus && shortcut->Owner) {
                if (FocusCaptured != NULL) {
                    Control* parentWalker = FocusCaptured;
                    do {
                        if (parentWalker == shortcut->Owner) {
                            isFocusCapturedControlIsAncestorOfOwner = true;
                            break;
                        }
                        parentWalker = parentWalker->Parent;
                    }
                    while (parentWalker != NULL);
                }
            }

            if (!shortcut->NeedsFocus || isFocusCapturedControlIsAncestorOfOwner) {
                shortcut->Action();
            }
        }
    }
}
Shortcut* Form::RegisterShortcut(int modifiers, int keycode, Control* owner, bool needsFocus, std::function<void()> action) {
    if (!action)
        return NULL;

    Shortcut* shortcut = new Shortcut;
    shortcut->ActivatorKey = keycode;
    shortcut->ActivatorModifier = modifiers;
    shortcut->Owner = owner;
    shortcut->NeedsFocus = needsFocus;
    shortcut->Action = action;
    // To check if the Owner has focus:
    // Go to the FocusCaptured control
    // walk up the Parent until NULL
    // if at any point we've reached Owner, then this returns true
    // if not, return false
    Shortcuts.Add(shortcut);
	return shortcut;
}

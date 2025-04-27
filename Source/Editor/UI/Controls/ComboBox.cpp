#include "ComboBox.hpp"
#include <UI/Graphics/Renderer.hpp>
#include <UI/System/Application.hpp>

#include <Hatch/Memory.h>
#include <Hatch/Strings.h>

ComboBoxDropDown::ComboBoxDropDown(ComboBox* parentComboBox) : Form(12, 12, "") {
	ParentComboBox = parentComboBox;

    auto parentBounds = ParentComboBox->GetScreenRect();
    Location = { parentBounds.x, parentBounds.y + parentBounds.h };
    Size = { parentBounds.w, ParentComboBox->ItemHeight * ParentComboBox->Items.Count() };
    BackColor = ParentComboBox->BackColor;
    ForeColor = ParentComboBox->ForeColor;
}
void ComboBoxDropDown::HandleSDLEvent(SDL_Event* e) {
    bool mouseOver;
    SDL_Point mousePos;
    auto bounds = GetScreenRect();
    auto parentBounds = ParentComboBox->GetScreenRect();

    Location = { parentBounds.x, parentBounds.y + parentBounds.h };

    switch (e->type) {
    case SDL_MOUSEBUTTONDOWN:
        mousePos = { e->button.x, e->button.y };
        mouseOver = SDL_PointInRect(&mousePos, &bounds);
        if (!mouseOver) {
            ParentComboBox->CloseDialog();
            return;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        mousePos = { e->button.x, e->button.y };
        mouseOver = SDL_PointInRect(&mousePos, &bounds);
        if (mouseOver) {
            ParentComboBox->Select((mousePos.y - bounds.y) / ParentComboBox->ItemHeight);
            ParentComboBox->CloseDialog();
            return;
        }
        break;
    case SDL_MOUSEMOTION:
        mousePos = { e->motion.x, e->motion.y };
        mouseOver = SDL_PointInRect(&mousePos, &bounds);
        if (mouseOver) {
            HoverIndex = (mousePos.y - bounds.y) / ParentComboBox->ItemHeight;
            return;
        }
        break;
    }
}
void ComboBoxDropDown::Render() {
    auto bounds = GetScreenRect();
    auto parBounds = ParentComboBox->GetScreenRect();
    UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

    Color backColor = BackColor;
    Color foreColor = ForeColor;
    
    int textStartX;
    int textWidth = parBounds.w - ParentComboBox->ARROW_SPACE_WIDTH - ParentComboBox->Padding.Horizontal();

    switch (ParentComboBox->TextAlign & 15) {
    case TEXT_ALIGN_LEFT:
        textStartX = parBounds.x + ParentComboBox->Padding.Left;
        break;
    case TEXT_ALIGN_CENTER:
    default:
        textStartX = parBounds.x + (ParentComboBox->Padding.Left + (parBounds.w - ParentComboBox->ARROW_SPACE_WIDTH - ParentComboBox->Padding.Right)) / 2;
        break;
    case TEXT_ALIGN_RIGHT:
        textStartX = parBounds.x + (parBounds.w - ParentComboBox->ARROW_SPACE_WIDTH - ParentComboBox->Padding.Right);
        break;
    }

    for (int i = 0; i < ParentComboBox->Items.Count(); i++) {
        if (HoverIndex == i)
            UI::Graphics::Renderer::DrawRect(bounds.x, bounds.y + i * ParentComboBox->ItemHeight, bounds.w, ParentComboBox->ItemHeight, ParentComboBox->HoverColor);
        else
            UI::Graphics::Renderer::DrawRect(bounds.x, bounds.y + i * ParentComboBox->ItemHeight, bounds.w, ParentComboBox->ItemHeight, ParentComboBox->BackColor);


        // Strings::FromCString(&BufferText, Items[i], 0);
        String* string = &ParentComboBox->BufferText;
        CString srcCString = ParentComboBox->Items[i];
        size_t srcCStringLen = strlen(srcCString);
        if (srcCStringLen > string->Capacity) {
            Memory::Realloc(&string->Text, srcCStringLen * sizeof(*string->Text), Memory::MEMPOOL_STRING);
            string->Capacity = srcCStringLen;
        }
        string->Length = srcCStringLen;
        for (int i = 0; i < string->Length; i++) {
            string->Text[i] = srcCString[i];
        }

        if (ParentComboBox->AutoEllipsis)
            UI::Graphics::Renderer::DrawFontEllipsis(&ParentComboBox->BufferText, Typeface,
                textStartX, bounds.y + i * ParentComboBox->ItemHeight + ParentComboBox->ItemHeight / 2, textWidth, ParentComboBox->TextAlign | TEXT_VALIGN_MIDDLE, foreColor);
        else
            UI::Graphics::Renderer::DrawFont(&ParentComboBox->BufferText, Typeface,
                textStartX, bounds.y + i * ParentComboBox->ItemHeight + ParentComboBox->ItemHeight / 2, ParentComboBox->TextAlign | TEXT_VALIGN_MIDDLE, foreColor);
    }

    UI::Graphics::Renderer::StrokeRect(&bounds, Color(0x000000, 0x80));
}

ComboBox::ComboBox() : Control() {
    BackColor = Color(0x1C1E24, 0xFF);
    ForeColor = Color(0xFFFFFF, 0xFF);
    FocusColor = Color(0x007FFF, 0xFF);
    HoverColor = Color(0x3A3E4B, 0xFF);
    PressedColor = Color(0x000000, 0x00);

    CreateShapeTexture_TriangleFill(&ShapeTriangleFill, 10, 10);

    Strings::Init(&Text, 1);
    Strings::Init(&BufferText, 1);

    Dock = DOCK_NONE;

    Padding = 4;

    CanFocus = true;
}

void ComboBox::OpenDialog() {
	// Opened = true;
	// HoverIndex = -1;
	DropDownControl = new ComboBoxDropDown(this);
	UI::System::Application::Show(DropDownControl);
}
void ComboBox::CloseDialog() {
    if (MouseCaptured == this) {
        UncaptureMouse();
    }
    // Opened = false;
	DropDownControl->Close();
	DropDownControl = NULL;
}

void ComboBox::OnMouseClick(MouseEventArgs* e) {
    OpenDialog();
}
void ComboBox::OnMouseDown(MouseEventArgs* e) {
    
}
void ComboBox::OnMouseUp(MouseEventArgs* e) {
    /*auto bounds = GetScreenRect();
    SDL_Point mousePos = { e->X, e->Y };
    bool mouseOver = SDL_PointInRect(&mousePos, &bounds);

    if (!mouseOver) {
        if (e->X < bounds.x ||
            e->X >= bounds.x + bounds.w ||
            e->Y < bounds.y ||
            e->Y >= bounds.y + bounds.h + Items.Count() * ItemHeight)
            return;

        Select((e->Y - (bounds.y + bounds.h)) / ItemHeight);
        CloseDialog();
    }*/
}
void ComboBox::OnMouseMove(MouseEventArgs* e) {
    //auto bounds = GetScreenRect();
    //SDL_Point mousePos = { e->X, e->Y };
    //bool mouseOver = SDL_PointInRect(&mousePos, &bounds);

    //if (!mouseOver) {
    //    if (e->X >= bounds.x && e->X < bounds.x + bounds.w) {
    //        // HoverIndex = (e->Y - (bounds.y + bounds.h)) / ItemHeight;
    //    }
    //}
}
void ComboBox::OnMouseEnter(MouseEventArgs* e) { }
void ComboBox::OnMouseLeave(MouseEventArgs* e) { }

void ComboBox::OnKeyDown(KeyEventArgs* e) {
    if (FocusCaptured == this) {
        if (e->Keycode == SDLK_UP) {
            if (SelectedIndex > 0)
                Select(SelectedIndex - 1);
        }
        else if (e->Keycode == SDLK_DOWN) {
            if (SelectedIndex < Items.Count() - 1)
                Select(SelectedIndex + 1);
        }
		else if (e->Keycode == SDLK_RETURN) {
			OpenDialog();
		}
    }
}
void ComboBox::OnKeyUp(KeyEventArgs* e) { }

void ComboBox::Render() {
    auto bounds = GetScreenRect();
    UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

    Color backColor = BackColor;
    Color foreColor = ForeColor;
    if (!Enabled) {
        backColor.A /= 2;
        foreColor.A /= 2;
    }

    if (Pressing)
        UI::Graphics::Renderer::DrawRect(&bounds, PressedColor);
    else if (MouseOver)
        UI::Graphics::Renderer::DrawRect(&bounds, HoverColor);
    else
        UI::Graphics::Renderer::DrawRect(&bounds, backColor);

    if (FocusCaptured == this)
        UI::Graphics::Renderer::StrokeRect(&bounds, FocusColor);

    UI::Graphics::Renderer::DrawTexture(ShapeTriangleFill, NULL,
        bounds.x + bounds.w - ARROW_SPACE_WIDTH / 2 - Padding.Right - 5, bounds.y + bounds.h / 2 - 5,
        10, 10, Color(0xFFFFFF, 0xFF), 90.0, NULL);

    int textStartX;
    int textStartY = bounds.y + bounds.h / 2;
    int textWidth = bounds.w - ARROW_SPACE_WIDTH - Padding.Horizontal();

    switch (TextAlign & 15) {
    case TEXT_ALIGN_LEFT:
        textStartX = bounds.x + Padding.Left;
        break;
    case TEXT_ALIGN_CENTER:
    default:
        textStartX = bounds.x + (Padding.Left + (bounds.w - ARROW_SPACE_WIDTH - Padding.Right)) / 2;
        break;
    case TEXT_ALIGN_RIGHT:
        textStartX = bounds.x + (bounds.w - ARROW_SPACE_WIDTH - Padding.Right);
        break;
    }

    if (SelectedIndex >= 0) {
        if (AutoEllipsis)
            UI::Graphics::Renderer::DrawFontEllipsis(&Text, Typeface,
                textStartX, textStartY, textWidth, TextAlign | TEXT_VALIGN_MIDDLE, foreColor);
        else
            UI::Graphics::Renderer::DrawFont(&Text, Typeface,
                textStartX, textStartY, TextAlign | TEXT_VALIGN_MIDDLE, foreColor);
    }
}

void ComboBox::Select(int index) {
    if (index >= 0)
        Strings::FromCString(&Text, Items[index], 0);

    if (SelectedIndex != index) {
        SelectedIndex = index;
        OnSelectedIndexChanged(NULL);
    }
}

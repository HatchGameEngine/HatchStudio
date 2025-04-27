#include "NumericUpDownBox.hpp"
#include <UI/Graphics/Renderer.hpp>

#include <Hatch/Strings.h>

NumericUpDown::NumericUpDown(double value) : TextboxBase() {
	internal_DecimalPlaces = 0;
	internal_Hexadecimal = false;
	
    Value = value;

    CanFocus = true;

    buttonUp = new Button();
    buttonDown = new Button();

    buttonUp->BackColor =
    buttonDown->BackColor = Color(0x000000, 0x7F);

    buttonUp->Parent =
    buttonDown->Parent = this;

    buttonUp->BorderRadius = 0;
    buttonDown->BorderRadius = 0;

    buttonUp->onClick += std::bind(&NumericUpDown::buttonUp_onClick, this, std::placeholders::_1, std::placeholders::_2);
    buttonDown->onClick += std::bind(&NumericUpDown::buttonDown_onClick, this, std::placeholders::_1, std::placeholders::_2);

	CreateShapeTexture_TriangleFill(&ShapeTriangleFill, 6, 10);
}
NumericUpDown::~NumericUpDown() {
    delete buttonUp;
    delete buttonDown;
}

void NumericUpDown::ReadPointer() {
    
}
void NumericUpDown::WritePointer() {
    
}

::Size NumericUpDown::get_Size() {
    return internal_Size;
}
void NumericUpDown::set_Size(::Size size) {
    TextboxBase::set_Size(size - ::Size { 20, 0 });
}
void NumericUpDown::set_Value(double value) {
    if (value < Minimum)
        value = Minimum;
    if (value > Maximum)
        value = Maximum;

    if (internal_Value != value) {
        internal_Value = value;

        WritePointer();

        OnValueChanged(NULL);
    }

    UpdateText();
}

void NumericUpDown::set_Hexadecimal(bool value) {
    internal_Hexadecimal = value;
    UpdateText();
}
void NumericUpDown::set_DecimalPlaces(int value) {
    internal_DecimalPlaces = value;
    UpdateText();
}

void NumericUpDown::UpdateText() {
    char textBuffer[256];
    if (internal_Hexadecimal)
        sprintf(textBuffer, "%X", (int)internal_Value);
    else
        sprintf(textBuffer, "%.*f", internal_DecimalPlaces, internal_Value);
    Strings::FromCString(&Text, textBuffer, 0);
}
void NumericUpDown::UpdateLayout() {
    Position head = { 1, 0 };
    ::Size size = internal_Size - ::Size { 0, 0 };

    buttonUp->Location = { head.X + size.W, head.Y };
    buttonUp->Size = { 20, size.H / 2 };
    buttonDown->Location = { head.X + size.W, head.Y + size.H / 2 };
    buttonDown->Size = { 20, size.H - size.H / 2 };
}

// void NumericUpDown::OnClick(EventArgs* e) { }
// void NumericUpDown::OnDoubleClick(EventArgs* e) { }
//
// void NumericUpDown::OnMouseDown(MouseEventArgs* e) { }
// void NumericUpDown::OnMouseClick(MouseEventArgs* e) { }
// void NumericUpDown::OnMouseUp(MouseEventArgs* e) { }
// void NumericUpDown::OnMouseDoubleClick(MouseEventArgs* e) { }
// void NumericUpDown::OnMouseMove(MouseEventArgs* e) { }
// void NumericUpDown::OnMouseEnter(MouseEventArgs* e) { }
// void NumericUpDown::OnMouseLeave(MouseEventArgs* e) { }
// void NumericUpDown::OnMouseWheel(MouseEventArgs* e) { }
//
// void NumericUpDown::OnTextInputted(TextEventArgs* e) { }
// void NumericUpDown::OnTextEdited(TextEventArgs* e) { }
//
// void NumericUpDown::OnKeyDown(KeyEventArgs* e) { }
// void NumericUpDown::OnKeyUp(KeyEventArgs* e) { }

void NumericUpDown::OnFocusLost(EventArgs* e) {
    TextboxBase::OnFocusLost(e);

    int varsFilled;
    char textBuffer[256];

    int valueI;
    float valueF;

    Strings::ToCString(textBuffer, &Text);
    if (Hexadecimal) {
        varsFilled = sscanf(textBuffer, "%X", &valueI);
        if (varsFilled > 0) {
            set_Value(valueI);
        }
    }
    else {
        varsFilled = sscanf(textBuffer, "%f", &valueF);
        if (varsFilled > 0) {
            set_Value(valueF);
        }
    }

    if (varsFilled == 0)
        UpdateText();
}

void NumericUpDown::Render() {
    auto bounds = GetScreenRect();
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	SDL_Rect buffer;
	ClipStart(&buffer, &bounds);

	int textStartX = bounds.x + Padding.Left - ScrollLocation.X;
	int textStartY = bounds.y + bounds.h / 2;

	UI::Graphics::Renderer::DrawRect(&bounds, BackColor);
	if (FocusCaptured == this)
		UI::Graphics::Renderer::StrokeRect(&bounds, Highlight);

	UI::Graphics::Renderer::DrawFont(TextPtr, Typeface,
		textStartX, textStartY, TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE, ForeColor);

	// Selection
	Color selection = Color(Highlight.R, Highlight.G, Highlight.B, 0x7F);
	float cursorHeight = Typeface->Ascent - Typeface->Descent;
	float cursorX, highlightStartX, highlightEndX;

	if (SelectionLength) {
		TextCursorToLocation(SelectionStart, &highlightStartX);
		TextCursorToLocation(SelectionStart + SelectionLength, &highlightEndX);

		UI::Graphics::Renderer::DrawRect(
			textStartX + highlightStartX, textStartY - cursorHeight / 2,
			highlightEndX - highlightStartX, cursorHeight,
			selection);
	}

	// Draw Cursor
	if (FocusCaptured == this) {
		TextCursorToLocation(SelectionCursor, &cursorX);

		UI::Graphics::Renderer::DrawRect(
			textStartX + cursorX - 1, textStartY - cursorHeight / 2,
			2, cursorHeight,
			Highlight);
	}

	ClipEnd(&buffer);

    buttonUp->Render();
    buttonDown->Render();

    bounds = buttonUp->GetScreenRect();
    UI::Graphics::Renderer::DrawTexture(ShapeTriangleFill, NULL,
        bounds.x + bounds.w / 2 - 4, bounds.y + bounds.h / 2 - 5,
        6, 10, buttonUp->ForeColor, 90.0, NULL, FLIPXY_X);

    bounds = buttonDown->GetScreenRect();
    UI::Graphics::Renderer::DrawTexture(ShapeTriangleFill, NULL,
        bounds.x + bounds.w / 2 - 4, bounds.y + bounds.h / 2 - 5,
        6, 10, buttonDown->ForeColor, 90.0, NULL, FLIPXY_NONE);
}

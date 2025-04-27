#include "Label.hpp"
#include <UI/Graphics/Renderer.hpp>

#include <Hatch/Strings.h>

Label::Label() : Control() {
	BackColor = Color(0x000000, 0x00);
	ForeColor = Color(0xFFFFFF, 0xFF);

	Padding = 0;
	Dock = DOCK_NONE;

	Strings::Init(&Text, 8);
    CanFocus = false;

	Size = { 100, 20 };
}
Label::Label(CString text) : Label() {
	Strings::FromCString(&Text, text, 0);
}
Label::Label(String* text) : Label() {
	Strings::Copy(&Text, text);
}

void Label::SetText(CString text) {
	Strings::FromCString(&Text, text, 0);
}
void Label::SetText(String* text) {
	Strings::Copy(&Text, text);
}

::Size Label::get_Size() {
	::Size outputSize = internal_Size;
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	UI::Graphics::Renderer::MeasureFont(&Text, Typeface, &outputSize.W, &outputSize.H);

	outputSize.H = Typeface->Ascent - Typeface->Descent;

	outputSize.W += Padding.Horizontal();
	outputSize.H += Padding.Vertical();

	return outputSize;
}

void Label::Render() {
	auto bounds = GetScreenRect();
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	UI::Graphics::Renderer::DrawRect(&bounds, BackColor);

	UI::Graphics::Renderer::DrawFont(&Text, Typeface,
        bounds.x, bounds.y + bounds.h / 2, TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE, ForeColor);
}

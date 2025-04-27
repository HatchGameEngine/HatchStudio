#include "ToolTip.hpp"
#include <UI/Graphics/Renderer.hpp>
#include <UI/System/Application.hpp>

#include <Hatch/Memory.h>
#include <Hatch/Strings.h>

ToolTip::ToolTip(String* text) : Form(12, 12, "") {
    Padding = 4;
	SetText(text);

    BackColor = Color(0xFFFFFF, 0xFF);
    ForeColor = Color(0x000000, 0xFF);

    SDL_GetMouseState(&Location.X, &Location.Y);
    Location.Y += 20;
}
ToolTip::ToolTip(const char* text) : Form(12, 12, "") {
    Padding = 4;
	SetText(text);

    BackColor = Color(0xFFFFFF, 0xFF);
    ForeColor = Color(0x000000, 0xFF);

    SDL_GetMouseState(&Location.X, &Location.Y);
    Location.Y += 20;
}
void ToolTip::HandleSDLEvent(SDL_Event* e) {
    bool mouseOver;
    SDL_Point mousePos;
    SDL_Rect bounds;

    switch (e->type) {
    case SDL_MOUSEBUTTONDOWN:
        Close();
        return;
    case SDL_MOUSEMOTION:
        bounds = GetScreenRect();
        bounds.x -= 4;
        bounds.y -= 4;
        bounds.w += 8;
        bounds.h += 8;
        mousePos = { e->motion.x, e->motion.y };
        mouseOver = SDL_PointInRect(&mousePos, &bounds);
        if (!mouseOver) {
            Close();
            return;
        }
        break;
    }
}
void ToolTip::Render() {
    auto bounds = GetScreenRect();
    UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

    UI::Graphics::Renderer::DrawRect(&bounds, BackColor);
    UI::Graphics::Renderer::DrawFont(&Text, Typeface,
        bounds.x + Padding.Left, bounds.y + bounds.h / 2, TEXT_ALIGN_LEFT | TEXT_VALIGN_MIDDLE, ForeColor);
}

void ToolTip::SetText(String* text) {
    Strings::Copy(&Text, text);

    ::Size contentSize;
    UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];
	UI::Graphics::Renderer::MeasureFont(&Text, Typeface, &contentSize.W, &contentSize.H);
    Size = { contentSize.W + Padding.Horizontal(), contentSize.H + Padding.Vertical() };
}
void ToolTip::SetText(const char* text) {
    Strings::FromCString(&Text, text, 0);

    ::Size contentSize;
    UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];
	UI::Graphics::Renderer::MeasureFont(&Text, Typeface, &contentSize.W, &contentSize.H);
    Size = { contentSize.W + Padding.Horizontal(), contentSize.H + Padding.Vertical() };
}

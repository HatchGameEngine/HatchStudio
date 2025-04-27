#include "ToolStrip.hpp"
#include <UI/Graphics/Font.hpp>
#include <UI/Graphics/Renderer.hpp>

#include <Hatch/Memory.h>
#include <Hatch/Strings.h>

void ToolStripSeparator::Render() {
    auto screenRect = GetScreenRect();

	UI::Graphics::Renderer::DrawRect(&screenRect, BackColor);
}

ToolStripButton::ToolStripButton() : ToolStripItem() {
    Icon = NULL;
    IconSize = { 20, 20 };

    Strings::Init(&Text, 1);

    Padding = 4;

    BorderColor = Color(0x000000, 0x00);
    Action_Base();
}
void ToolStripButton::SetText(CString title) {
	Strings::FromCString(&Text, title, 0);
}
void ToolStripButton::SetText(String* title) {
	Strings::Copy(&Text, title);
}
::Size ToolStripButton::get_Size() {
    ::Size subSize;
	::Size outputSize = internal_Size;

    if (Icon) {
        subSize = IconSize;
        subSize.W += Padding.Horizontal();
        subSize.H += Padding.Vertical();

        outputSize.W = subSize.W;
    }
    else if (Text.Length > 0) {
        ::Size contentSize;
		UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

		UI::Graphics::Renderer::MeasureFont(&Text, Typeface, &contentSize.W, &contentSize.H);

        subSize = contentSize;
        subSize.W += Padding.Horizontal();
        subSize.H += Padding.Vertical();

        outputSize.W = subSize.W;
    }

    return outputSize;
}
void ToolStripButton::Render() {
    if (Checked)
        Action_Checked();
    else if (Hovering)
        Action_Hover();
    else
        Action_Base();

    auto screenRect = GetScreenRect();

	UI::Graphics::Renderer::DrawRect(&screenRect, BackColor);
	UI::Graphics::Renderer::DrawRect(&screenRect, BorderColor);

    if (Icon) {
        SDL_Rect iconDst = {
            screenRect.x + (screenRect.w - IconSize.W) / 2,
            screenRect.y + (screenRect.h - IconSize.H) / 2,
            IconSize.W, IconSize.H,
        };
        SDL_SetTextureColorMod(Icon, ForeColor.R, ForeColor.G, ForeColor.B);
        SDL_SetTextureAlphaMod(Icon, ForeColor.A);

		UI::Graphics::Renderer::DstRectAdjustment(&iconDst);
		SDL_RenderCopy(UI::Graphics::Renderer::Renderer, Icon, NULL, &iconDst);
    }
    else if (Text.Length > 0) {
		UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];
		int textStartX = screenRect.x + (Padding.Left + (screenRect.w - Padding.Right)) / 2;
    	int textStartY = screenRect.y + screenRect.h / 2;
		UI::Graphics::Renderer::DrawFont(&Text, Typeface,
            textStartX, textStartY, TEXT_ALIGN_CENTER | TEXT_VALIGN_MIDDLE, ForeColor);
    }
}

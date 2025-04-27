#include "Button.hpp"
#include <UI/Graphics/Renderer.hpp>

#include <Hatch/Strings.h>

ButtonBase::ButtonBase() : Control() {
	BackColor = Color(0x1C1E24, 0xFF);
	ForeColor = Color(0xFFFFFF, 0xFF);
	FocusColor = Color(0x007FFF, 0xFF);
	HoverColor = Color(0x3A3E4B, 0xFF);
	PressedColor = Color(0x000000, 0x00);

	Padding = 4;

	Dock = DOCK_NONE;

	Strings::Init(&Text, 8);

	TextPtr = &Text;

	CanFocus = true;

	Size = { 100, 20 };
}
ButtonBase::ButtonBase(CString string) : ButtonBase() {
	Strings::FromCString(&Text, string, 0);
}
ButtonBase::ButtonBase(String* string) : ButtonBase() {
	Strings::Copy(&Text, string);
}

::Size ButtonBase::get_Size() {
	::Size outputSize = internal_Size;

	if (AutoSize) {
		::Size contentSize;
		UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

		UI::Graphics::Renderer::MeasureFont(TextPtr, Typeface, &contentSize.W, &contentSize.H);

		if (AutoSizeMode == AUTOSIZEMODE_GROWONLY) {
			outputSize.W = M_MAX(outputSize.W, contentSize.W);
			outputSize.H = M_MAX(outputSize.H, contentSize.H);
		}
		else if (AutoSizeMode == AUTOSIZEMODE_GROWANDSHRINK) {
			outputSize = contentSize;
		}

		outputSize.W += Padding.Horizontal();
		outputSize.H += Padding.Vertical();
	}

	return outputSize;
}

void ButtonBase::SetText(CString title) {
	Strings::FromCString(TextPtr, title, 0);
}
void ButtonBase::SetText(String* title) {
	Strings::Copy(TextPtr, title);
}

void ButtonBase::OnMouseDown(MouseEventArgs* e) {
	Pressing = true;

	if (CaptureMouse()) {

	}
}
void ButtonBase::OnMouseUp(MouseEventArgs* e) {
	Pressing = false;

	if (MouseCaptured == this) {
		UncaptureMouse();
	}
}
void ButtonBase::OnMouseMove(MouseEventArgs* e) {

}
void ButtonBase::OnMouseEnter(MouseEventArgs* e) {

}
void ButtonBase::OnMouseLeave(MouseEventArgs* e) {

}

void ButtonBase::OnKeyDown(KeyEventArgs* e) {
	if (FocusCaptured == this) {
		switch (e->Keycode) {
		case SDLK_RETURN:
		case SDLK_SPACE:
			break;
		}
	}
}
void ButtonBase::OnKeyUp(KeyEventArgs* e) {

}

void Button::Init() {
	FillShape = NULL;
	if (FillShape)
		SDL_DestroyTexture(FillShape);
	CreateShapeTexture_EllipseFill(&FillShape, BorderRadius * 2, BorderRadius * 2);
}

void Button::set_Size(::Size size) {
	internal_Size = size;
}

void Button::DrawButtonShape(SDL_Rect* bounds, Color color) {
	SDL_Rect src;
	SDL_Rect dst;
	if (BorderRadius == 0) {
		UI::Graphics::Renderer::DrawRect(bounds, color);
		return;
	}

	// Top-left corner
	src = { 0, 0, BorderRadius, BorderRadius };
	dst = { bounds->x, bounds->y, BorderRadius, BorderRadius };
	UI::Graphics::Renderer::DrawTexture(FillShape, &src, &dst, color);

	// Top-right corner
	src = { BorderRadius, 0, BorderRadius, BorderRadius };
	dst = { bounds->x + bounds->w - BorderRadius, bounds->y, BorderRadius, BorderRadius };
	UI::Graphics::Renderer::DrawTexture(FillShape, &src, &dst, color);

	// Bottom-left corner
	src = { 0, BorderRadius, BorderRadius, BorderRadius };
	dst = { bounds->x, bounds->y + bounds->h - BorderRadius, BorderRadius, BorderRadius };
	UI::Graphics::Renderer::DrawTexture(FillShape, &src, &dst, color);

	// Bottom-right corner
	src = { BorderRadius, BorderRadius, BorderRadius, BorderRadius };
	dst = { bounds->x + bounds->w - BorderRadius, bounds->y + bounds->h - BorderRadius, BorderRadius, BorderRadius };
	UI::Graphics::Renderer::DrawTexture(FillShape, &src, &dst, color);

	// Left rectangle
	dst = { bounds->x, bounds->y + BorderRadius, BorderRadius, bounds->h - BorderRadius * 2 };
	UI::Graphics::Renderer::DrawRect(&dst, color);

	// Middle rectangle
	dst = { bounds->x + BorderRadius, bounds->y, bounds->w - BorderRadius * 2, bounds->h };
	UI::Graphics::Renderer::DrawRect(&dst, color);

	// Right rectangle
	dst = { bounds->x + bounds->w - BorderRadius, bounds->y + BorderRadius, BorderRadius, bounds->h - BorderRadius * 2 };
	UI::Graphics::Renderer::DrawRect(&dst, color);
}

void Button::Render() {
	auto bounds = GetScreenRect();
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	Color backColor = BackColor;
	Color foreColor = ForeColor;
	if (!Enabled) {
		backColor.A /= 2;
		foreColor.A /= 2;
	}

	if (Pressing)
		DrawButtonShape(&bounds, PressedColor);
	else if (MouseOver)
		DrawButtonShape(&bounds, HoverColor);
	else
		DrawButtonShape(&bounds, backColor);

	if (FocusCaptured == this)
		UI::Graphics::Renderer::StrokeRect(&bounds, FocusColor);

	int textStartX;
	int textStartY = bounds.y + bounds.h / 2;
	int textWidth = bounds.w - Padding.Horizontal();

	switch (TextAlign & 15) {
	case TEXT_ALIGN_LEFT:
		textStartX = bounds.x + Padding.Left;
		break;
	case TEXT_ALIGN_CENTER:
	default:
		textStartX = bounds.x + (Padding.Left + (bounds.w - Padding.Right)) / 2;
		break;
	case TEXT_ALIGN_RIGHT:
		textStartX = bounds.x + (bounds.w - Padding.Right);
		break;
	}

	if (AutoEllipsis)
		UI::Graphics::Renderer::DrawFontEllipsis(&Text, Typeface,
			textStartX, textStartY, textWidth, TextAlign | TEXT_VALIGN_MIDDLE, foreColor);
	else
		UI::Graphics::Renderer::DrawFont(&Text, Typeface,
			textStartX, textStartY, TextAlign | TEXT_VALIGN_MIDDLE, foreColor);
}

void RadioButton::Init() {
	Padding = 0;
	Padding.Right = 8;
	AutoSizeMode = AUTOSIZEMODE_GROWANDSHRINK;

	OuterRadius = NULL;
	OuterRadiusBorder = NULL;
	InnerRadius = NULL;

	// CreateShapeTexture_EllipseFill(&OuterRadius, 20, 20);
	CreateShapeTexture_RoundRectFill(&OuterRadius, 20, 20, 5, 5, 5, 5);
	CreateShapeTexture_EllipseStroke(&OuterRadiusBorder, 20, 20);
	CreateShapeTexture_EllipseFill(&InnerRadius, 10, 10);
}
::Size RadioButton::get_Size() {
	::Size outputSize = internal_Size;

	::Size contentSize;
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	UI::Graphics::Renderer::MeasureFont(TextPtr, Typeface, &contentSize.W, &contentSize.H);

	if (AutoSizeMode == AUTOSIZEMODE_GROWONLY) {
		outputSize.W = M_MAX(outputSize.W, contentSize.W);
		outputSize.H = M_MAX(outputSize.H, contentSize.H);
	}
	else if (AutoSizeMode == AUTOSIZEMODE_GROWANDSHRINK) {
		outputSize = contentSize;
	}

	::Size checkSize = { 20, 20 };

	switch (CheckAlign & 0x0F) {
	case TEXT_ALIGN_LEFT:
	case TEXT_ALIGN_RIGHT:
		outputSize.W += checkSize.W;
		break;
	case TEXT_ALIGN_CENTER:
		outputSize.W = M_MAX(outputSize.W, checkSize.W);
		break;
	}

	switch (CheckAlign & 0xF0) {
	case TEXT_VALIGN_TOP:
	case TEXT_VALIGN_BOTTOM:
		outputSize.H += checkSize.H;
		break;
	case TEXT_VALIGN_MIDDLE:
		outputSize.H = M_MAX(outputSize.H, checkSize.H);
		break;
	}

	outputSize.W += Padding.Horizontal();
	outputSize.H += Padding.Vertical();

	return outputSize;
}
void RadioButton::Render() {
	auto bounds = GetScreenRect();
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	// Set the position & alignment for the text & button
	::Size checkSize = { 20, 20 };
	::Position checkPos = { bounds.x, bounds.y };

	int textAlign = 0;
	::Size textSize = { 0, 0 };
	::Position textPos = { bounds.x, bounds.y };
	UI::Graphics::Renderer::MeasureFont(TextPtr, Typeface, &textSize.W, &textSize.H);

	switch (CheckAlign & 0x0F) {
	case TEXT_ALIGN_LEFT:
		checkPos.X = bounds.x;

		textAlign |= TEXT_ALIGN_LEFT;
		textPos.X = checkPos.X + checkSize.W + Padding.Right;
		break;
	case TEXT_ALIGN_RIGHT:
		checkPos.X = bounds.x + bounds.w - checkSize.W;

		textAlign |= TEXT_ALIGN_RIGHT;
		textPos.X = checkPos.X - Padding.Left;
		break;
	case TEXT_ALIGN_CENTER:
		checkPos.X = bounds.x + (bounds.w - checkSize.W) / 2;

		textAlign |= TEXT_ALIGN_CENTER;
		textPos.X = bounds.x + (bounds.w) / 2;
		break;
	}

	switch (CheckAlign & 0xF0) {
	case TEXT_VALIGN_TOP:
		checkPos.Y = bounds.y;

		textAlign |= TEXT_VALIGN_TOP;
		textPos.Y = checkPos.Y + checkSize.H + Padding.Bottom;
		break;
	case TEXT_VALIGN_BOTTOM:
		checkPos.Y = bounds.y + bounds.h - checkSize.H;

		textAlign |= TEXT_VALIGN_BOTTOM;
		textPos.Y = checkPos.Y - Padding.Top;
		break;
	case TEXT_VALIGN_MIDDLE:
		checkPos.Y = bounds.y + (bounds.h - checkSize.H) / 2;

		textAlign |= TEXT_VALIGN_MIDDLE;
		textPos.Y = bounds.y + (bounds.h) / 2;
		break;
	}

	Color backColor = BackColor;
	Color foreColor = ForeColor;
	if (!Enabled) {
		backColor.A /= 2;
		foreColor.A /= 2;
	}

	SDL_Rect checkBounds = { checkPos.X, checkPos.Y, checkSize.W, checkSize.H };

	if (MouseOver)
		UI::Graphics::Renderer::DrawTexture(OuterRadius, NULL, &checkBounds, HoverColor);
	else
		UI::Graphics::Renderer::DrawTexture(OuterRadius, NULL, &checkBounds, backColor);

	if (FocusCaptured == this)
		UI::Graphics::Renderer::DrawTexture(OuterRadiusBorder, NULL, &checkBounds, FocusColor);

	if (Checked)
		UI::Graphics::Renderer::DrawTexture(InnerRadius, NULL, checkPos.X + 5, checkPos.Y + 5, 10, 10, foreColor);

	if (AutoEllipsis)
		UI::Graphics::Renderer::DrawFontEllipsis(&Text, Typeface,
			textPos.X, textPos.Y, bounds.w, textAlign, foreColor);
	else
		UI::Graphics::Renderer::DrawFont(&Text, Typeface,
			textPos.X, textPos.Y, textAlign, foreColor);
}

void CheckBox::Init() {
	Padding = 0;
	Padding.Right = 8;
	AutoSizeMode = AUTOSIZEMODE_GROWANDSHRINK;
}
::Size CheckBox::get_Size() {
	::Size outputSize = internal_Size;

	::Size contentSize;
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	UI::Graphics::Renderer::MeasureFont(TextPtr, Typeface, &contentSize.W, &contentSize.H);

	if (AutoSizeMode == AUTOSIZEMODE_GROWONLY) {
		outputSize.W = M_MAX(outputSize.W, contentSize.W);
		outputSize.H = M_MAX(outputSize.H, contentSize.H);
	}
	else if (AutoSizeMode == AUTOSIZEMODE_GROWANDSHRINK) {
		outputSize = contentSize;
	}

	::Size checkSize = { 20, 20 };

	switch (CheckAlign & 0x0F) {
	case TEXT_ALIGN_LEFT:
	case TEXT_ALIGN_RIGHT:
		outputSize.W += checkSize.W;
		break;
	case TEXT_ALIGN_CENTER:
		outputSize.W = M_MAX(outputSize.W, checkSize.W);
		break;
	}

	switch (CheckAlign & 0xF0) {
	case TEXT_VALIGN_TOP:
	case TEXT_VALIGN_BOTTOM:
		outputSize.H += checkSize.H;
		break;
	case TEXT_VALIGN_MIDDLE:
		outputSize.H = M_MAX(outputSize.H, checkSize.H);
		break;
	}

	outputSize.W += Padding.Horizontal();
	outputSize.H += Padding.Vertical();

	return outputSize;
}
void CheckBox::Render() {
	auto bounds = GetScreenRect();
	UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];

	// Set the position & alignment for the text & button
	::Size checkSize = { 20, 20 };
	::Position checkPos = { bounds.x, bounds.y };

	int textAlign = 0;
	::Size textSize = { 0, 0 };
	::Position textPos = { bounds.x, bounds.y };
	UI::Graphics::Renderer::MeasureFont(TextPtr, Typeface, &textSize.W, &textSize.H);

	switch (CheckAlign & 0x0F) {
	case TEXT_ALIGN_LEFT:
		checkPos.X = bounds.x;

		textAlign |= TEXT_ALIGN_LEFT;
		textPos.X = checkPos.X + checkSize.W + Padding.Right;
		break;
	case TEXT_ALIGN_RIGHT:
		checkPos.X = bounds.x + bounds.w - checkSize.W;

		textAlign |= TEXT_ALIGN_RIGHT;
		textPos.X = checkPos.X - Padding.Left;
		break;
	case TEXT_ALIGN_CENTER:
		checkPos.X = bounds.x + (bounds.w - checkSize.W) / 2;

		textAlign |= TEXT_ALIGN_CENTER;
		textPos.X = bounds.x + (bounds.w) / 2;
		break;
	}

	switch (CheckAlign & 0xF0) {
	case TEXT_VALIGN_TOP:
		checkPos.Y = bounds.y;

		textAlign |= TEXT_VALIGN_TOP;
		textPos.Y = checkPos.Y + checkSize.H + Padding.Bottom;
		break;
	case TEXT_VALIGN_BOTTOM:
		checkPos.Y = bounds.y + bounds.h - checkSize.H;

		textAlign |= TEXT_VALIGN_BOTTOM;
		textPos.Y = checkPos.Y - Padding.Top;
		break;
	case TEXT_VALIGN_MIDDLE:
		checkPos.Y = bounds.y + (bounds.h - checkSize.H) / 2;

		textAlign |= TEXT_VALIGN_MIDDLE;
		textPos.Y = bounds.y + (bounds.h) / 2;
		break;
	}

	Color backColor = BackColor;
	Color foreColor = ForeColor;
	if (!Enabled) {
		backColor.A /= 2;
		foreColor.A /= 2;
	}

	SDL_Rect checkBounds = { checkPos.X, checkPos.Y, checkSize.W, checkSize.H };

	if (Pressing)
		UI::Graphics::Renderer::DrawRect(&checkBounds, PressedColor);
	else if (MouseOver)
		UI::Graphics::Renderer::DrawRect(&checkBounds, HoverColor);
	else
		UI::Graphics::Renderer::DrawRect(&checkBounds, backColor);

	if (FocusCaptured == this)
		UI::Graphics::Renderer::StrokeRect(&checkBounds, FocusColor);

	if (GetChecked()) {
		checkBounds = { checkPos.X + 5, checkPos.Y + 5, 10, 10 };
		UI::Graphics::Renderer::DrawRect(&checkBounds, foreColor);
	}

	if (AutoEllipsis)
		UI::Graphics::Renderer::DrawFontEllipsis(&Text, Typeface,
			textPos.X, textPos.Y, bounds.w, textAlign, foreColor);
	else
		UI::Graphics::Renderer::DrawFont(&Text, Typeface,
			textPos.X, textPos.Y, textAlign, foreColor);
}

#include "RadialKnob.hpp"
#include <UI/Graphics/Renderer.hpp>

#include <cmath>

RadialKnob::RadialKnob() : Control() {
	BackColor = Color(0x1C1E24, 0xFF);
	ForeColor = Color(0xFFFFFF, 0xFF);
	FocusColor = Color(0x007FFF, 0xFF);
	HoverColor = Color(0x3A3E4B, 0xFF);
	PressedColor = Color(0x000000, 0x00);

	Padding = 4;

	Dock = DOCK_NONE;

	CanFocus = true;

	Size = { 40, 40 };
}

void RadialKnob::set_Size(::Size size) {
    Control::set_Size(size);

    CreateShapeTexture_EllipseFill(&ShapeCircleFill, size.W, size.H);
    CreateShapeTexture_EllipseStroke(&ShapeCircleStroke, size.W, size.H);
	CreateShapeTexture_TriangleFill(&ShapeTriangleFill, 10, 10);
}

void RadialKnob::OnMouseDown(MouseEventArgs* e) {
	Pressing = true;

	if (CaptureMouse()) {
		dragStartEvent = *e;
	}
}
void RadialKnob::OnMouseUp(MouseEventArgs* e) {
	Pressing = false;

	if (MouseCaptured == this) {
		UncaptureMouse();

		DialValueChangedArgs e;
		e.Value = Angle;
		OnValueChanged(&e);
	}
}
void RadialKnob::OnMouseMove(MouseEventArgs* e) {
	if (MouseCaptured == this) {
		auto bounds = GetScreenRect();
		SDL_Point knobCenter = { bounds.x + bounds.w / 2, bounds.y + bounds.h / 2 };

		double oldAngle = Angle;

		// const Uint8* state = SDL_GetKeyboardState(NULL);
		// if (state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT])
		// 	SnapMode = SnapMode::None;
		// else
		// 	SnapMode = SnapMode::Snap32nd;

		Angle = std::atan2(e->Y - knobCenter.y, e->X - knobCenter.x) * (MaxAngle * 0.5) / M_PI;
		if (SnapAngle) {
			double snap = MaxAngle / SnapDivisors;
			Angle = std::round(Angle / snap) * snap;
		}

		Angle += Bias;

		while (Angle < 0.0)
			Angle += MaxAngle;
		while (Angle >= MaxAngle)
			Angle -= MaxAngle;

		if (oldAngle != Angle) {
			DialTurnedArgs e;
			e.Value = Angle;
			OnDialTurn(&e);
		}
	}
}
void RadialKnob::OnMouseEnter(MouseEventArgs* e) {

}
void RadialKnob::OnMouseLeave(MouseEventArgs* e) {

}

void RadialKnob::OnKeyDown(KeyEventArgs* e) {
	if (FocusCaptured == this) {
		switch (e->Keycode) {
		case SDLK_RETURN:
		case SDLK_SPACE:
			break;
		}
	}
}
void RadialKnob::OnKeyUp(KeyEventArgs* e) {

}

void RadialKnob::Render() {
	auto bounds = GetScreenRect();

	Color backColor = BackColor;
	Color foreColor = ForeColor;
	if (!Enabled) {
		backColor.A /= 2;
		foreColor.A /= 2;
	}

	if (MouseOver)
		UI::Graphics::Renderer::DrawTexture(ShapeCircleFill, NULL, &bounds, HoverColor);
	else
		UI::Graphics::Renderer::DrawTexture(ShapeCircleFill, NULL, &bounds, backColor);

	if (FocusCaptured == this)
		UI::Graphics::Renderer::DrawTexture(ShapeCircleStroke, NULL, &bounds, FocusColor);

	SDL_Point knobCenter = { bounds.x + bounds.w / 2, bounds.y + bounds.h / 2 };

	float outRadius = bounds.w / 2 - 1.0f;
	float innRadius = outRadius - 2.0f;
	float _sinI, _cosI, _sinO, _cosO;
	float angleInc = 360.0 / SnapDivisors;
	for (double angle = 0.0; angle < 360.0; angle += angleInc) {
		_sinI = std::sin(angle * M_PI / 180.0);
		_cosI = std::cos(angle * M_PI / 180.0);

		_sinO = _sinI * outRadius;
		_cosO = _cosI * outRadius;
		_sinI = _sinI * innRadius;
		_cosI = _cosI * innRadius;
		UI::Graphics::Renderer::DrawLine(
			knobCenter.x + _cosI, knobCenter.y + _sinI,
			knobCenter.x + _cosO, knobCenter.y + _sinO, Color(0xFFFFFF, 0x3F));
	}

	// Draw arrow
	SDL_Point center = { -bounds.w / 2 + (10), (10 / 2) };
	UI::Graphics::Renderer::DrawTexture(ShapeTriangleFill, NULL,
		knobCenter.x - center.x, knobCenter.y - center.y,
		10, 10, Color(0xFFFFFF, 0xFF), (Angle - Bias) * 360.0 / MaxAngle, &center);
}

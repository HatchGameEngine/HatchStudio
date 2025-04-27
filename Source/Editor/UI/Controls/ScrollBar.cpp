#include "ScrollBar.hpp"
#include <UI/Graphics/Renderer.hpp>

SDL_Rect HScrollBar::GetThumbBounds() {
    ScrollableControl* parent = (ScrollableControl*)Parent;
    if (!parent ||
        parent->DisplayBounds.w >= parent->ContentBounds.w ||
        parent->DisplayBounds.w == 0 ||
        parent->ContentBounds.w == 0)
        return { 0, 0, 0, 0 };

    auto Bounds = GetScreenRect();

    int arrowHeight = 0;
    int thumbMaxLength = Bounds.w;
    int thumbLength = thumbMaxLength * parent->DisplayBounds.w / parent->ContentBounds.w;

    return {
        Bounds.x + (Value * (thumbMaxLength - thumbLength) / (Maximum - Minimum)),
        Bounds.y,
        thumbLength,
        Bounds.h,
    };
}
void HScrollBar::OnMouseDown(MouseEventArgs* e) {
    ScrollableControl* parent = (ScrollableControl*)Parent;

    SDL_Point mousePos { e->X, e->Y };
    SDL_Rect thumbArea = GetThumbBounds();
    if (SDL_PointInRect(&mousePos, &thumbArea)) {
        if (CaptureMouse()) {
            DragCursorStart = mousePos;
            DragStarted = true;
            ValueStart = Value;
        }
    }
}
void HScrollBar::OnMouseMove(MouseEventArgs* e) {
    ScrollableControl* parent = (ScrollableControl*)Parent;

    SDL_Point mousePos { e->X, e->Y };
    SDL_Rect thumbArea = GetThumbBounds();
    MouseHovering = SDL_PointInRect(&mousePos, &thumbArea);

    auto Bounds = GetScreenRect();

    if (DragStarted) {
        int thumbMaxLength = Bounds.w;
        int thumbLength = thumbMaxLength * parent->DisplayBounds.w / parent->ContentBounds.w;

        int dragDelta = (e->X - DragCursorStart.x);
        int valueDelta = dragDelta * (Maximum - Minimum) / (thumbMaxLength - thumbLength);


        ScrollEventArgs e;
        e.OldValue = Value;

        Value = ValueStart + valueDelta;

        e.NewValue = Value;
        e.Orientation = ScrollOrientation::HorizontalScroll;
        e.Type = ScrollEventType::ThumbTrack;
        OnScroll(&e);
    }
}
void HScrollBar::OnMouseLeave(MouseEventArgs* e) {
    MouseHovering = false;
}
void HScrollBar::OnMouseUp(MouseEventArgs* e) {
    if (DragStarted) {
        UncaptureMouse();
    }
    DragStarted = false;
}
void HScrollBar::Render() { // Could probably combine this under ScrollBar instead
    ScrollableControl* parent = (ScrollableControl*)Parent;

    SDL_Rect r = GetThumbBounds();
    if (!r.w || !r.h)
        return;

    r.x += 1;
    r.y += 1;
    r.w -= 2;
    r.h -= 2;

    auto Bounds = GetScreenRect();

	UI::Graphics::Renderer::DrawRect(&Bounds, Color(0xF0F0F0, 0xFF));

    Color color = Color(0xC0C0C0, 0xFF);
    if (DragStarted)
        color = Color(0x909090, 0xFF);
    else if (MouseHovering)
        color = Color(0xA8A8A8, 0xFF);
	UI::Graphics::Renderer::DrawRect(&r, color);
}

SDL_Rect VScrollBar::GetThumbBounds() {
    ScrollableControl* parent = (ScrollableControl*)Parent;
    if (!parent ||
        parent->DisplayBounds.h >= parent->ContentBounds.h ||
        parent->DisplayBounds.h == 0 ||
        parent->ContentBounds.h == 0)
        return { 0, 0, 0, 0 };

    auto Bounds = GetScreenRect();

    int arrowHeight = 0;
    int thumbMaxLength = Bounds.h;
    int thumbLength = thumbMaxLength * parent->DisplayBounds.h / parent->ContentBounds.h;

    return {
        Bounds.x,
        Bounds.y + (Value * (thumbMaxLength - thumbLength) / (Maximum - Minimum)),
        Bounds.w,
        thumbLength,
    };
}
void VScrollBar::OnMouseDown(MouseEventArgs* e) {
    ScrollableControl* parent = (ScrollableControl*)Parent;

    SDL_Point mousePos { e->X, e->Y };
    SDL_Rect thumbArea = GetThumbBounds();
    if (SDL_PointInRect(&mousePos, &thumbArea)) {
        if (CaptureMouse()) {
            DragCursorStart = mousePos;
            DragStarted = true;
            ValueStart = Value;
        }
    }
}
void VScrollBar::OnMouseMove(MouseEventArgs* e) {
    ScrollableControl* parent = (ScrollableControl*)Parent;

    SDL_Point mousePos { e->X, e->Y };
    SDL_Rect thumbArea = GetThumbBounds();
    MouseHovering = SDL_PointInRect(&mousePos, &thumbArea);

    auto Bounds = GetScreenRect();

    if (DragStarted) {
        int thumbMaxLength = Bounds.h;
        int thumbLength = thumbMaxLength * parent->DisplayBounds.h / parent->ContentBounds.h;

        int dragDelta = (e->Y - DragCursorStart.y);
        int valueDelta = dragDelta * (Maximum - Minimum) / (thumbMaxLength - thumbLength);

        ScrollEventArgs e;
        e.OldValue = Value;

        Value = ValueStart + valueDelta;

        e.NewValue = Value;
        e.Orientation = ScrollOrientation::VerticalScroll;
        e.Type = ScrollEventType::ThumbTrack;
        OnScroll(&e);
    }
}
void VScrollBar::OnMouseLeave(MouseEventArgs* e) {
    MouseHovering = false;
}
void VScrollBar::OnMouseUp(MouseEventArgs* e) {
    if (DragStarted) {
        UncaptureMouse();
    }
    DragStarted = false;
}
void VScrollBar::Render() {
    ScrollableControl* parent = (ScrollableControl*)Parent;

    auto Bounds = GetScreenRect();

	UI::Graphics::Renderer::DrawRect(&Bounds, Color(0xF0F0F0, 0xFF));

    SDL_Rect r = GetThumbBounds();
    if (!r.w || !r.h)
        return;

    r.x += 1;
    r.y += 1;
    r.w -= 2;
    r.h -= 2;

    Color color = Color(0xC0C0C0, 0xFF);
    if (DragStarted)
        color = Color(0x909090, 0xFF);
    else if (MouseHovering)
        color = Color(0xA8A8A8, 0xFF);
	UI::Graphics::Renderer::DrawRect(&r, color);
}

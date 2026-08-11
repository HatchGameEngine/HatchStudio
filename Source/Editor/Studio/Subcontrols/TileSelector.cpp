#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Graphics.h>
#include <Hatch/Strings.h>

#include <Studio/Impl.hpp>
#include <Studio/Structs.hpp>

#include <UI/Graphics/Font.hpp>
#include <UI/Graphics/Renderer.hpp>

// Control imports
#include <UI/Controls/Control.hpp>

#include <UI/Controls/Button.hpp>
#include <UI/Controls/ScrollBar.hpp>

#include <Studio/Subcontrols/TileSelector.hpp>

TileSelector::TileSelector(StageTileset* tileset) : Panel() {
    Tileset = tileset;

    Margin = 1;
    Padding = 7;

    TileSpace = TileSize + Margin.Left;

    DoHScroll = false;
    DoVScroll = true;

    HideEmptyVScroll = false;

    BackColor = Color(0x282C34, 0xFF);

    Strings::FromCString(&DefaultTextLine1, "Import Tileset Images", 0);
    Strings::FromCString(&DefaultTextLine2, "To Get Started!", 0);
}

void TileSelector::SetTileset(StageTileset* tileset) {
    Tileset = tileset;
}

void TileSelector::OnMouseDown(MouseEventArgs* e) {
    Control::OnMouseDown(e);

    if (Tileset && e->Button == SDL_BUTTON(SDL_BUTTON_LEFT) && CaptureMouse()) {
        Position windowPos = GetPositionInWindowCoords();

        int mx = e->X, my = e->Y;
        mx -= windowPos.X;
        my -= windowPos.Y;
        mx -= ContentBounds.x + Padding.Left;
        my -= ContentBounds.y + Padding.Top;
        my += VScrollControl->Value;
        if (mx >= 0 &&
            my >= 0 &&
            mx < ContentBounds.x + ContentBounds.w - (Padding.Left + Padding.Right) &&
            my < ContentBounds.y + ContentBounds.h - (Padding.Top + Padding.Bottom)) {
            int tileIndex = M_MIN((mx / TileSpace) + (my / TileSpace) * Columns, Tileset->TileCount);
            SelectRange(tileIndex, tileIndex);
            Select(tileIndex);
        }
    }
}
void TileSelector::OnMouseMove(MouseEventArgs* e) {
    Control::OnMouseMove(e);

    if (Tileset && e->Button == SDL_BUTTON(SDL_BUTTON_LEFT) && MouseCaptured == this) {
        RequestUpdatedBounds();

        Position windowPos = GetPositionInWindowCoords();

        int mx = e->X, my = e->Y;
        mx -= windowPos.X;
        my -= windowPos.Y;
        mx -= ContentBounds.x + Padding.Left;
        my -= ContentBounds.y + Padding.Top;
        my += VScrollControl->Value;
        if (mx >= 0 &&
            my >= 0 &&
            mx < ContentBounds.x + ContentBounds.w - (Padding.Left + Padding.Right) &&
            my < ContentBounds.y + ContentBounds.h - (Padding.Top + Padding.Bottom)) {
            int tileIndex = M_MIN((mx / TileSpace) + (my / TileSpace) * Columns, Tileset->TileCount);
            SelectRange(SelectedTileRange_Start, tileIndex);
            Select(tileIndex);
        }
    }
}
void TileSelector::OnMouseUp(MouseEventArgs* e) {
    if (MouseCaptured == this) {
        UncaptureMouse();
    }
}

void TileSelector::RequestUpdatedBounds() {
    if (!Tileset)
        return;

    auto Bounds = GetScreenRect();

    TileSpace = TileSize + Margin.Left;

    Columns = (Size.Get().W - (Padding.Horizontal() + VScrollControl->Size.Get().W)) / TileSpace;
    Columns = M_MAX(Columns, 1);

    ContentBounds.x = 0;
    ContentBounds.y = 0;
    ContentBounds.w = TileSpace * Columns - Margin.Left + Padding.Horizontal();
    ContentBounds.h = TileSpace * ((Tileset->TileCount + (Columns - 1)) / Columns) - Margin.Left + Padding.Vertical();

    // Bounds.w = ContentBounds.w + VScrollControl->Bounds.w;
}
void TileSelector::ResizeChildren() {
    HideEmptyVScroll = !Tileset || Tileset->TileCount == 0;

    RequestUpdatedBounds();

    auto size = Size.Get();
    DisplayBounds.w = size.W;
    DisplayBounds.h = size.H;

    bool showHScrollBar = DoHScroll && DisplayBounds.w < ContentBounds.w;
    bool showVScrollBar = DoVScroll;
    ::Size hScrollBarSize = HScrollControl->Size;
    ::Size vScrollBarSize = VScrollControl->Size;

    if (showHScrollBar)
        DisplayBounds.h -= hScrollBarSize.H;
    if (showVScrollBar)
        DisplayBounds.w -= vScrollBarSize.W;

    HScrollControl->Location = { 0, DisplayBounds.h };
    HScrollControl->Size = { DisplayBounds.w, hScrollBarSize.H };

    VScrollControl->Location = { DisplayBounds.w, 0 };
    VScrollControl->Size = { vScrollBarSize.W, DisplayBounds.h };

    VScrollControl->Minimum = 0;
    VScrollControl->Maximum = ContentBounds.h - DisplayBounds.h;

    VScrollControl->SmallChange = TileSpace;
    VScrollControl->LargeChange = TileSpace * 4;

    Control::ResizeChildren();
}

void TileSelector::GetHighlightBounds(int* start, int* end) {
    *start = M_MIN(SelectedTileRange_Start, SelectedTileRange_End);
    *end = M_MAX(SelectedTileRange_Start, SelectedTileRange_End);
}
bool TileSelector::IsCellWithinHighlight(int x, int y) {
    int tCount = Tileset->TileCount;
    int xCount = Columns;
    int yCount = (tCount + xCount - 1) / xCount;

    if (x < 0 || x >= xCount)
        return false;
    if (y < 0 || y >= yCount)
        return false;

    int index = x + y * xCount;
    if (index < M_MIN(SelectedTileRange_Start, SelectedTileRange_End)|| index > M_MAX(SelectedTileRange_Start, SelectedTileRange_End))
        return false;

    return true;
}
void TileSelector::DrawHighlightSection(SDL_Rect* dst, int bitFlag, Color colorInner, Color colorOuter) {
    enum CheckDirs {
        CHK_TOP_LEFT = 1,
        CHK_TOP = 2,
        CHK_TOP_RIGHT = 4,
        CHK_LEFT = 8,
        CHK_RIGHT = 16,
        CHK_BOTTOM_LEFT = 32,
        CHK_BOTTOM = 64,
        CHK_BOTTOM_RIGHT = 128,
    };

    int x1i = dst->x,
        y1i = dst->y,
        x2i = dst->x + dst->w - 1,
        y2i = dst->y + dst->h - 1,
        xwi = dst->w - 2,
        yhi = dst->h - 2;
    int x1o = dst->x - 1,
        y1o = dst->y - 1,
        x2o = dst->x + dst->w,
        y2o = dst->y + dst->h,
        xwo = dst->w,
        yho = dst->h;

    // colorOuter = colorInner;

    #define INNER_TOP UI::Graphics::Renderer::DrawRect(x1i + 1, y1i, xwi, 1, colorInner)
    #define INNER_LEFT UI::Graphics::Renderer::DrawRect(x1i, y1i + 1, 1, yhi, colorInner)
    #define INNER_RIGHT UI::Graphics::Renderer::DrawRect(x2i, y1i + 1, 1, yhi, colorInner)
    #define INNER_BOTTOM UI::Graphics::Renderer::DrawRect(x1i + 1, y2i, xwi, 1, colorInner)
    #define INNER_TOP_LEFT UI::Graphics::Renderer::DrawRect(x1i, y1i, 1, 1, colorInner)
    #define INNER_TOP_RIGHT UI::Graphics::Renderer::DrawRect(x2i, y1i, 1, 1, colorInner)
    #define INNER_BOTTOM_LEFT UI::Graphics::Renderer::DrawRect(x1i, y2i, 1, 1, colorInner)
    #define INNER_BOTTOM_RIGHT UI::Graphics::Renderer::DrawRect(x2i, y2i, 1, 1, colorInner)

    #define OUTER_TOP UI::Graphics::Renderer::DrawRect(x1o + 1, y1o, xwo, 1, colorOuter)
    #define OUTER_LEFT UI::Graphics::Renderer::DrawRect(x1o, y1o + 1, 1, yho, colorOuter)
    #define OUTER_RIGHT UI::Graphics::Renderer::DrawRect(x2o, y1o + 1, 1, yho, colorOuter)
    #define OUTER_BOTTOM UI::Graphics::Renderer::DrawRect(x1o + 1, y2o, xwo, 1, colorOuter)
    #define OUTER_TOP_LEFT UI::Graphics::Renderer::DrawRect(x1o, y1o, 1, 1, colorOuter)
    #define OUTER_TOP_RIGHT UI::Graphics::Renderer::DrawRect(x2o, y1o, 1, 1, colorOuter)
    #define OUTER_BOTTOM_LEFT UI::Graphics::Renderer::DrawRect(x1o, y2o, 1, 1, colorOuter)
    #define OUTER_BOTTOM_RIGHT UI::Graphics::Renderer::DrawRect(x2o, y2o, 1, 1, colorOuter)

    if (!(bitFlag & CHK_TOP)) {
        INNER_TOP; OUTER_TOP;
    }
    if (!(bitFlag & CHK_LEFT)) {
        INNER_LEFT; OUTER_LEFT;
    }
    if (!(bitFlag & CHK_RIGHT)) {
        INNER_RIGHT; OUTER_RIGHT;
    }
    if (!(bitFlag & CHK_BOTTOM)) {
        INNER_BOTTOM; OUTER_BOTTOM;
    }

    // Outside TL Corner (0|0), Inner TL Corner (1|1), L H Connector (0|1), T V Connector (1|0)
    if (!(bitFlag & CHK_TOP_LEFT)) {
        INNER_TOP_LEFT; OUTER_TOP_LEFT;
    }
    if (!(bitFlag & CHK_TOP_RIGHT)) {
        INNER_TOP_RIGHT; OUTER_TOP_RIGHT;
    }
    if (!(bitFlag & CHK_BOTTOM_LEFT)) {
        INNER_BOTTOM_LEFT; OUTER_BOTTOM_LEFT;
    }
    if (!(bitFlag & CHK_BOTTOM_RIGHT)) {
        INNER_BOTTOM_RIGHT; OUTER_BOTTOM_RIGHT;
    }

}

int TileSelector::TileIndexToColumn(int t) {
    return t % Columns;
}
int TileSelector::TileIndexToRow(int t) {
    return t / Columns;
}

void TileSelector::Select(int id) {
    if (SelectedTileID != id) {
        SelectedTileID = id;
        OnSelectedTileIDChanged(NULL);
    }
}
void TileSelector::SelectRange(int start, int end) {
    if (SelectedTileRange_Start != start ||
        SelectedTileRange_End != end) {
        SelectedTileRange_Start = start;
        SelectedTileRange_End = end;
        OnSelectedTileRangeChanged(NULL);
    }
}

void TileSelector::Render() {
    Panel::Render();

    auto Bounds = GetScreenRect();

    if (!Tileset || Tileset->TileCount == 0) {
        ::Size lineSz1, lineSz2;
        UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];
        UI::Graphics::Renderer::MeasureFont(&DefaultTextLine1, Typeface, &lineSz1.W, &lineSz1.H);
        UI::Graphics::Renderer::MeasureFont(&DefaultTextLine2, Typeface, &lineSz2.W, &lineSz2.H);

        UI::Graphics::Renderer::DrawFont(&DefaultTextLine1, Typeface, Bounds.x + Bounds.w / 2, Bounds.y + Bounds.h / 2 - (lineSz1.H + lineSz2.H) / 2, TEXT_ALIGN_CENTER | TEXT_VALIGN_MIDDLE, Color(0xFFFFFF, 0xFF));
        UI::Graphics::Renderer::DrawFont(&DefaultTextLine2, Typeface, Bounds.x + Bounds.w / 2, Bounds.y + Bounds.h / 2 + (lineSz1.H + lineSz2.H) / 2, TEXT_ALIGN_CENTER | TEXT_VALIGN_MIDDLE, Color(0xFFFFFF, 0xFF));
    }
    else {
        SDL_Rect buffer;
        ClipStart(&buffer, &Bounds);

        const int columnMask = 63;
        const int columnCount = 64;
        const int columnBitshift = 6;

        int rows = TileIndexToRow(Tileset->TileCount + Columns - 1);
        if (rows < 1)
            rows = 1;

        UI::Graphics::Renderer::DrawRect(
            Bounds.x + Padding.Left - 1,
            Bounds.y + Padding.Top - 1 - VScrollControl->Value,
            1 + Columns * TileSpace,
            1 + rows * TileSpace, Color(0x000000, 0xFF));

        SDL_SetTextureColorMod(Tileset->TileCollisionTextures[TileCollisionPlane], 0xFF, 0xFF, 0xFF);

        int tileSpc = TileSpace;
        for (int t = 0; t < Tileset->TileCount; t++) {
            int tX = Padding.Left + TileIndexToColumn(t) * tileSpc;
            int tY = Padding.Top + TileIndexToRow(t) * tileSpc - VScrollControl->Value;
            SDL_Rect src = { (t & columnMask) << 4, (t >> columnBitshift) << 4, TileSize, TileSize };
            SDL_Rect dst = { Bounds.x + tX, Bounds.y + tY, TileSize, TileSize };

            UI::Graphics::Renderer::DstRectAdjustment(&dst);
            if (ShowTileGraphics) {
                SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, Tileset->TileImageTexture, &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
            }
            if (ShowTileCollision) {
                SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, Tileset->TileCollisionTextures[TileCollisionPlane], &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
            }
        }

        if (SelectedTileRange_Start != -1) {
            Color colorInner = Color(0x7F7F7F, 0xFF);
            Color colorOuter = Color(0xFFFFFF, 0xFF);
            int indexStart, indexEnd;
            int s = TileSpace * 2;

            GetHighlightBounds(&indexStart, &indexEnd);

            enum CheckDirs {
                CHK_TOP_LEFT = 1,
                CHK_TOP = 2,
                CHK_TOP_RIGHT = 4,
                CHK_LEFT = 8,
                CHK_RIGHT = 16,
                CHK_BOTTOM_LEFT = 32,
                CHK_BOTTOM = 64,
                CHK_BOTTOM_RIGHT = 128,
            };

            int bitFlag, cx, cy, tX, tY;
            for (int t = indexStart; t <= indexEnd; t++) {
                bitFlag = 0;
                cx = TileIndexToColumn(t);
                cy = TileIndexToRow(t);
                tX = Padding.Left + cx * tileSpc;
                tY = Padding.Top + cy * tileSpc - VScrollControl->Value;

                // Cardinal directions
                if (IsCellWithinHighlight(cx, cy - 1))
                    bitFlag |= CHK_TOP;
                if (IsCellWithinHighlight(cx - 1, cy))
                    bitFlag |= CHK_LEFT;
                if (IsCellWithinHighlight(cx + 1, cy))
                    bitFlag |= CHK_RIGHT;
                if (IsCellWithinHighlight(cx, cy + 1))
                    bitFlag |= CHK_BOTTOM;

                if ((bitFlag & (CHK_TOP | CHK_LEFT)) == (CHK_TOP | CHK_LEFT) && IsCellWithinHighlight(cx - 1, cy - 1))
                    bitFlag |= CHK_TOP_LEFT;
                if ((bitFlag & (CHK_TOP | CHK_RIGHT)) == (CHK_TOP | CHK_RIGHT) && IsCellWithinHighlight(cx + 1, cy - 1))
                    bitFlag |= CHK_TOP_RIGHT;
                if ((bitFlag & (CHK_BOTTOM | CHK_LEFT)) == (CHK_BOTTOM | CHK_LEFT) && IsCellWithinHighlight(cx - 1, cy + 1))
                    bitFlag |= CHK_BOTTOM_LEFT;
                if ((bitFlag & (CHK_BOTTOM | CHK_RIGHT)) == (CHK_BOTTOM | CHK_RIGHT) && IsCellWithinHighlight(cx + 1, cy + 1))
                    bitFlag |= CHK_BOTTOM_RIGHT;

                SDL_Rect dst = { Bounds.x + tX, Bounds.y + tY, TileSize, TileSize };
                DrawHighlightSection(&dst, bitFlag, colorInner, colorOuter);
            }
        }

        ClipEnd(&buffer);
    }
}

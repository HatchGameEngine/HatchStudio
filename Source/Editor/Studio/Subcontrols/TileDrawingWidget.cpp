#include <SDL2/SDL.h>

#include <cmath>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Studio/Impl.hpp>
#include <Studio/Structs.hpp>

#include <UI/Graphics/Renderer.hpp>

#include <Studio/Editors/SceneEditor.hpp>

// Control imports
#include <UI/Controls/Control.hpp>

#include <UI/Controls/Button.hpp>

#include <Studio/Subcontrols/TileCollisionEditorPanel.hpp>
#include <Studio/Subcontrols/TileDrawingWidget.hpp>

TileDrawingWidget::TileDrawingWidget(TileCollisionEditorPanel* tileCollisionEditor) : Control() {
    this->tileCollisionEditor = tileCollisionEditor;
    this->Tileset = tileCollisionEditor->Tileset;
}

int TileDrawingWidget::GetPlane() {
    if (tileCollisionEditor->radioButtonShowA->Checked)
        return 0;
    if (tileCollisionEditor->radioButtonShowB->Checked)
        return 1;

    return 0;
}

void TileDrawingWidget::MouseSelect(MouseEventArgs* e) {
    auto bounds = GetScreenRect();
    TileSelector* tileSelector = tileCollisionEditor->tileSelector;
    int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
    int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

    int p = GetPlane();

    int column = (e->X - bounds.x) * 16 / bounds.w;
    int row = (e->Y - bounds.y) * 16 / bounds.h;
    if (column < 0 || column >= 16)
        return;
    if (row < 0 || row >= 16)
        return;

    dragPxEnd = { (e->X - bounds.x), (e->Y - bounds.y) };

    if (Tileset) {
        if (tileSelector->SelectedTileID >= 0) {
            if (editMode == EditMode::Collision) {
                if (e->Button == SDL_BUTTON(SDL_BUTTON_LEFT)) {
                    for (int t = tS; t <= tE; t++) {
                        EditableTileConfig* tileData = &Tileset->TileCfg[p][t];
                        tileData->Collision[column] = row;
                    }
                }
                else if (e->Button == SDL_BUTTON(SDL_BUTTON_RIGHT)) {
                    for (int t = tS; t <= tE; t++) {
                        EditableTileConfig* tileData = &Tileset->TileCfg[p][t];
                        tileData->Collision[column] = 0xFF;
                    }
                }
            }
            else if (editMode == EditMode::Angle) {
                int columnS = (dragStart.X - bounds.x) * 16 / bounds.w;
                int rowS = (dragStart.Y - bounds.y) * 16 / bounds.h;
                if (columnS < 0 || columnS >= 16)
                    return;
                if (rowS < 0 || rowS >= 16)
                    return;

                for (int t = tS; t <= tE; t++) {
                    EditableTileConfig* tileData = &Tileset->TileCfg[p][t];
                    int newAngle = Math::ATan(dragPxEnd.X - dragPxStart.X, dragPxEnd.Y - dragPxStart.Y);

                    switch (tileCollisionEditor->GetAngleEditSide()) {
                    case 0: tileData->AngleTop = newAngle; break;
                    case 1: tileData->AngleLeft = newAngle; break;
                    case 2: tileData->AngleRight = newAngle; break;
                    case 3: tileData->AngleBottom = newAngle; break;
                    }
                }

                tileCollisionEditor->UpdateTileInfoUI();
            }
        }
    }
}

void TileDrawingWidget::OnMouseDown(MouseEventArgs* e) {
    const Uint8* state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT])
        editMode = EditMode::Angle;
    else
        editMode = EditMode::Collision;

    if (CaptureMouse()) {
        dragStart = *e;

        MouseSelect(e);

        auto bounds = GetScreenRect();
        dragPxStart = { (e->X - bounds.x), (e->Y - bounds.y) };
    }
}
void TileDrawingWidget::OnMouseMove(MouseEventArgs* e) {
    if (MouseCaptured == this) {
        MouseSelect(e);
    }
}
void TileDrawingWidget::OnMouseUp(MouseEventArgs* e) {
    if (MouseCaptured == this) {
        TileSelector* tileSelector = tileCollisionEditor->tileSelector;
        int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
        int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

        int p = GetPlane();

        if (editMode == EditMode::Collision) {
            if (Tileset) {
                for (int t = tS; t <= tE; t++)
                    Tileset->UpdateTileCollisionTexture(p, t);
            }

            SceneEditor* editor = tileCollisionEditor->Editor;
            if (editor) {
                editor->tilePlacementField->UpdateRenderTarget = true;
            }
        }

        UncaptureMouse();

        editMode = EditMode::Collision;
    }
}

void TileDrawingWidget::DrawCheckedRect(int x, int y, int w, int h, int oddMod) {
    const Color white = Color(0xFFFFFF, 0x80);
    const Color black = Color(0x000000, 0x80);

    // for (int xx = x; xx < x + w; xx++) {
    //     for (int yy = y; yy < y + h; yy++) {
    //         UI::Graphics::Renderer::DrawRect(xx, yy, 1, 1, ((xx + yy) & 1) ? black : white);
    //     }
    // }
    UI::Graphics::Renderer::DrawRect(x, y, w, h, white);
}

void TileDrawingWidget::DrawArrow(int x0, int y0, int x1, int y1, Color color) {
    const float thickness = 3.0f;

    double angle = atan2(y1 - y0, x1 - x0) + M_PI;

    int x2 = (int)(x1 + 20 * cos(angle - M_PI / 8));
    int y2 = (int)(y1 + 20 * sin(angle - M_PI / 8));
    int x3 = (int)(x1 + 20 * cos(angle + M_PI / 8));
    int y3 = (int)(y1 + 20 * sin(angle + M_PI / 8));

    UI::Graphics::Renderer::DrawLine(x1, y1, x0, y0, color, thickness);
    UI::Graphics::Renderer::DrawLine(x1, y1, x2, y2, color, thickness);
    UI::Graphics::Renderer::DrawLine(x1, y1, x3, y3, color, thickness);
}

void TileDrawingWidget::Render() {
    auto bounds = GetScreenRect();

    const Color gridColor = Color(0x808080, 0x80);

    const Color greay = Color(0x808080, 0xFF);
    const Color white = Color(0xFFFFFF, 0xFF);
    TileSelector* tileSelector = tileCollisionEditor->tileSelector;
    bool showGrid = tileCollisionEditor->checkBoxShowGrid->GetChecked();
    bool showArrow = tileCollisionEditor->checkBoxShowAngle->GetChecked();

    int p = GetPlane();

    UI::Graphics::Renderer::DrawRect(&bounds, Color(0x000000, 0xFF));

    if (Tileset) {
        if (tileSelector->SelectedTileID >= 0) {
            const int TileSize = 16;
            const int columnMask = 63;
            const int columnCount = 64;
            const int columnBitshift = 6;
            int t = tileSelector->SelectedTileID;

            int pxSz = bounds.w / TileSize;

            SDL_Rect src = { (t & columnMask) << 4, (t >> columnBitshift) << 4, TileSize, TileSize };
            SDL_Rect dst = bounds;
            UI::Graphics::Renderer::DstRectAdjustment(&dst);
            SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, Tileset->TileImageTextures[0], &src, &dst, 0.0, NULL, SDL_FLIP_NONE);

            EditableTileConfig* tileData = &Tileset->TileCfg[p][t];

            if (editMode == EditMode::Collision) {
                if (tileData->Orientation) {
                    // Top anchored
                    for (int column = 0; column < TILE_SIZE; column++) {
                        auto col = tileData->Collision[column];
                        auto colh = col * pxSz + pxSz;
                        if (col != 0xFF)
                            DrawCheckedRect(bounds.x + column * pxSz, bounds.y, pxSz, colh, 0);
                    }
                }
                else {
                    // Bottom anchored
                    for (int column = 0; column < TILE_SIZE; column++) {
                        auto col = tileData->Collision[column];
                        auto colh = col * pxSz;
                        if (col != 0xFF)
                            DrawCheckedRect(bounds.x + column * pxSz, bounds.y + colh, pxSz, bounds.h - colh, 0);
                    }
                }

                if (showGrid) {
                    for (int column = 0; column < TILE_SIZE; column++) {
                        UI::Graphics::Renderer::StrokeRect(bounds.x, bounds.y + column * pxSz - 1, bounds.w, 1, gridColor);
                        UI::Graphics::Renderer::StrokeRect(bounds.x + column * pxSz - 1, bounds.y, 1, bounds.h, gridColor);
                    }
                }

                if (showArrow) {
                    double realAngle = TileAngle * M_PI / 128;

                    DrawArrow(
                        bounds.x + (bounds.w / 2),
                        bounds.y + (bounds.h / 2),
                        bounds.x + (int)(bounds.w / 2 + sin(realAngle) * bounds.w / 2),
                        bounds.y + (int)(bounds.h / 2 - cos(realAngle) * bounds.h / 2),
                        Color(0xFF0000, 0xFF));
                }
            }
            else {
                DrawArrow(
                    bounds.x + dragPxStart.X, bounds.y + dragPxStart.Y,
                    bounds.x + dragPxEnd.X, bounds.y + dragPxEnd.Y, Color(0xFF0000, 0xFF));
            }
        }
    }

    UI::Graphics::Renderer::StrokeRect(bounds.x - 1, bounds.y - 1, bounds.w + 2, bounds.h + 2, greay);
}

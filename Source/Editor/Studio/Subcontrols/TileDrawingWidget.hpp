#pragma once

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Studio/Impl.hpp>
#include <Studio/Structs.hpp>

#include <UI/Graphics/Renderer.hpp>

// Control imports
#include <UI/Controls/Control.hpp>

#include <UI/Controls/Button.hpp>

struct TileCollisionEditorPanel;

struct TileDrawingWidget : Control {
    enum class EditMode {
        Collision,
        Angle,
    };

    TileCollisionEditorPanel* tileCollisionEditor = NULL;
    StageTileset* Tileset = NULL;
    EditMode editMode = EditMode::Collision;

    MouseEventArgs dragStart = { };
    Vector2 dragPxStart;
    Vector2 dragPxEnd;

    int TileAngle = 0;

    TileDrawingWidget(TileCollisionEditorPanel* tileCollisionEditor);

    int GetPlane();

    void MouseSelect(MouseEventArgs* e);

    void OnMouseDown(MouseEventArgs* e);
    void OnMouseMove(MouseEventArgs* e);
    void OnMouseUp(MouseEventArgs* e);

    void DrawCheckedRect(int x, int y, int w, int h, int oddMod);
    void DrawArrow(int x0, int y0, int x1, int y1, Color color);

    void Render();
};

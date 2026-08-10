#pragma once

#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Strings.h>

#include <Studio/Impl.hpp>
#include <Studio/Structs.hpp>

// Control imports
#include <UI/Controls/Control.hpp>

#include <UI/Controls/ScrollBar.hpp>

struct TileSelector : Panel {
    Stage* LinkedStage = NULL;

    int TileSize = 16;
    int TileSpace = 17;
    int Columns = 16;

    int SelectedTileID = 0;
    int SelectedTileRange_Start = 0;
    int SelectedTileRange_End = 0;

    String DefaultTextLine1;
    String DefaultTextLine2;

    Tile StampTileBuffer[256];

    bool ShowTileGraphics = false;
    bool ShowTileCollision = false;
    int TileCollisionPlane = 0;

    DEFINE_SIMPLE_EVENT(SelectedTileIDChanged);
    DEFINE_SIMPLE_EVENT(SelectedTileRangeChanged);

    TileSelector(Stage* stage);

    void OnMouseDown(MouseEventArgs* e);
    void OnMouseMove(MouseEventArgs* e);
    void OnMouseUp(MouseEventArgs* e);

    void RequestUpdatedBounds();
    void ResizeChildren();

    void GetHighlightBounds(int* start, int* end);
    bool IsCellWithinHighlight(int x, int y);
    void DrawHighlightSection(SDL_Rect* dst, int bitFlag, Color colorInner, Color colorOuter);

    inline int TileIndexToColumn(int t);
    inline int TileIndexToRow(int t);

    void Select(int id);
    void SelectRange(int start, int end);

    void Render();
};

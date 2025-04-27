#pragma once

namespace Graphics {
    extern Pixel       Palette[MAX_PALETTE_COUNT][0x100];
    extern Uint8       PaletteIndexLines[MAX_FRAMEBUFFER_HEIGHT];

    extern View        Views[MAX_VIEWPORTS];
    extern ViewOutput  ViewOutputs[MAX_VIEWPORTS];
    extern View*       CurrentView;
    extern Uint32      CurrentViewIndex;

    extern DrawGroup   DrawGroups[MAX_DRAWGROUPS];
    extern Uint32      CurrentDrawGroupIndex;
    extern ScanLine    ScanLineBuffer[MAX_FRAMEBUFFER_HEIGHT];

    extern float       StereoscopicSplit;

    extern bool        DidDraw;
    extern bool        DrawToScreen;

    void Init();

    void View_SetSize(int viewIndex, int width, int height);
    void View_GetSize(int viewIndex, int* width, int* height);

    // Palette things
    void  PaletteLoad(CString filename);
    Color PaletteGetColor(int paletteIndex, int colorIndex);
    void  PaletteSetColor(int paletteIndex, int colorIndex, Color color);
    void  PaletteMixPalettes(int destPaletteIndex, int paletteIndexA, int paletteIndexB, int mixRatio, int colorIndexStart, int colorCount);
    void  PaletteRotateColorsLeft(int paletteIndex, int colorIndexStart, int colorCount);
    void  PaletteRotateColorsRight(int paletteIndex, int colorIndexStart, int colorCount);
    void  PaletteCopyColors(int srcPaletteIndex, int srcColorIndexStart, int destPaletteIndex, int destColorIndexStart, int colorCount);
    void  PaletteSetPaletteIndexLines(int paletteIndex, int lineStart, int lineEnd);

    void DrawSprite(Resource sprite, int animation, int frame, Vector2* position);
    void DrawAnimation(Animator* animator, Vector2* position);
    void DrawImage(Resource image, Vector2* position);
    void DrawImagePart(Resource image, Vector2* position, int srcX, int srcY, int srcW, int srcH);
    void DrawSpriteText(String* string, Vector2* position, Resource sprite, int animation, int startIndex, int endIndex, int alignment, int spacing, Vector2* offsets);
    void DrawDebugText(CString text, int x, int y, Color color);

    void DrawTile(Tile tile, Vector2* position, bool flipX, bool flipY);
    void CopyImageToTiles(Resource image, int startTileID, int srcX, int srcY, int srcW, int srcH);

    void SetCompareColor(Color color);
    void SetPixelFilter(Uint16* filter);

    void DrawLine(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Color color, int blendFlag);
    void DrawCircle(Subpixels x, Subpixels y, Subpixels radius, Color color, int blendFlag);
    void DrawCircleStroke(Subpixels x, Subpixels y, Subpixels radius, Color color, int blendFlag);
    void DrawRing(Subpixels x, Subpixels y, Subpixels innerRadius, Subpixels outerRadius, Color color, int blendFlag);
    void DrawEllipse(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Color color, int blendFlag);
    void DrawRectangle(Subpixels x, Subpixels y, Subpixels w, Subpixels h, Color color, int blendFlag);
    void DrawTriangle(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Subpixels x3, Subpixels y3, Color color, int blendFlag);
    void DrawPolygon(Vector2* positions, Color color, int vertexCount, int blendFlag);
    void DrawPolygonBlend(Vector2* positions, Color* colors, int vertexCount, int blendFlag);
    void DrawPolygonTextured(Vector2* positions, Vector2* uvs, int vertexCount, int blendFlag);
    void FadeScreen(Color color, int rMult, int gMult, int bMult);

    void View3D_SetAmbientLighting(Resource view3dIndex, Uint32 r, Uint32 g, Uint32 b);
    void View3D_SetDiffuseLighting(Resource view3dIndex, Uint32 r, Uint32 g, Uint32 b);
    void View3D_SetSpecularLighting(Resource view3dIndex, Uint32 r, Uint32 g, Uint32 b);
    void View3D_DrawBegin(Resource view3dIndex);
    void View3D_DrawFinish(Resource view3dIndex, Uint32 drawMode);
    void View3D_DrawModel(Resource view3dIndex, Resource meshIndex, int frame, Matrix4x4* viewMatrix, Matrix4x4* normalMatrix, Color color);

    void DrawAll();
    void DrawAll_Editor();
}

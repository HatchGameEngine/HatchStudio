#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Graphics.h>

#include <Hatch/Diagnostics.h>
#include <Hatch/Game.h>
#include <Hatch/GameLinker.h>
#include <Hatch/Math.h>
#include <Hatch/Memory.h>
#include <Hatch/Resources.h>
#include <Hatch/Scene.h>

#define GET_R(color) (color).R
#define GET_G(color) (color).G
#define GET_B(color) (color).B
#define ISOLATE_R(color) (color & 0xFF0000)
#define ISOLATE_G(color) (color & 0x00FF00)
#define ISOLATE_B(color) (color & 0x0000FF)

namespace Graphics {
    Pixel       Palette[MAX_PALETTE_COUNT][0x100];
    Uint8       PaletteIndexLines[MAX_FRAMEBUFFER_HEIGHT];

    View        Views[MAX_VIEWPORTS];
    ViewOutput  ViewOutputs[MAX_VIEWPORTS];
    View*       CurrentView;
    Uint32      CurrentViewIndex;

    DrawGroup   DrawGroups[MAX_DRAWGROUPS];
    Uint32      CurrentDrawGroupIndex;
    ScanLine    ScanLineBuffer[MAX_FRAMEBUFFER_HEIGHT];

    Uint8       MultTablePos[0x2000];
    Uint8       MultTableNeg[0x2000];
    Uint8       MultTableInv[0x2000];
    #define     COLOR_BITS 5 // = 8 - 3 bits per color for RGB5A1
    const int   ColorMaxValue = (1 << COLOR_BITS) - 1;

    Pixel       CompareColor;
    Pixel       FilterTable[0x8000];

    float       StereoscopicSplit = 1.0f;

    bool        DidDraw = false;
    bool        DrawToScreen = false;

    Uint8 font8x8_basic[128][8] = {
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0000 (nul)
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0001
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0002
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0003
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0004
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0005
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0006
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0007
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0008
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0009
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000A
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000B
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000C
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000D
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000E
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000F
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0010
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0011
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0012
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0013
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0014
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0015
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0016
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0017
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0018
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0019
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001A
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001B
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001C
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001D
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001E
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001F
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0020 (space)
        { 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00 },   // U+0021 (!)
        { 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0022 (")
        { 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00 },   // U+0023 (#)
        { 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00 },   // U+0024 ($)
        { 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00 },   // U+0025 (%)
        { 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00 },   // U+0026 (&)
        { 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0027 (')
        { 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00 },   // U+0028 (()
        { 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00 },   // U+0029 ())
        { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00 },   // U+002A (*)
        { 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00 },   // U+002B (+)
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06 },   // U+002C (,)
        { 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00 },   // U+002D (-)
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00 },   // U+002E (.)
        { 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00 },   // U+002F (/)
        { 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00 },   // U+0030 (0)
        { 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00 },   // U+0031 (1)
        { 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00 },   // U+0032 (2)
        { 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00 },   // U+0033 (3)
        { 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00 },   // U+0034 (4)
        { 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00 },   // U+0035 (5)
        { 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00 },   // U+0036 (6)
        { 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00 },   // U+0037 (7)
        { 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00 },   // U+0038 (8)
        { 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00 },   // U+0039 (9)
        { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00 },   // U+003A (:)
        { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06 },   // U+003B (;)
        { 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00 },   // U+003C (<)
        { 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00 },   // U+003D (=)
        { 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00 },   // U+003E (>)
        { 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00 },   // U+003F (?)
        { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00 },   // U+0040 (@)
        { 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00 },   // U+0041 (A)
        { 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00 },   // U+0042 (B)
        { 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00 },   // U+0043 (C)
        { 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00 },   // U+0044 (D)
        { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00 },   // U+0045 (E)
        { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00 },   // U+0046 (F)
        { 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00 },   // U+0047 (G)
        { 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00 },   // U+0048 (H)
        { 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0049 (I)
        { 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00 },   // U+004A (J)
        { 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00 },   // U+004B (K)
        { 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00 },   // U+004C (L)
        { 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00 },   // U+004D (M)
        { 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00 },   // U+004E (N)
        { 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00 },   // U+004F (O)
        { 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00 },   // U+0050 (P)
        { 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00 },   // U+0051 (Q)
        { 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00 },   // U+0052 (R)
        { 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00 },   // U+0053 (S)
        { 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0054 (T)
        { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00 },   // U+0055 (U)
        { 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00 },   // U+0056 (V)
        { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 },   // U+0057 (W)
        { 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00 },   // U+0058 (X)
        { 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0059 (Y)
        { 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00 },   // U+005A (Z)
        { 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00 },   // U+005B ([)
        { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00 },   // U+005C (\)
        { 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00 },   // U+005D (])
        { 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00 },   // U+005E (^)
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },   // U+005F (_)
        { 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0060 (`)
        { 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00 },   // U+0061 (a)
        { 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00 },   // U+0062 (b)
        { 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00 },   // U+0063 (c)
        { 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00 },   // U+0064 (d)
        { 0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00 },   // U+0065 (e)
        { 0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00 },   // U+0066 (f)
        { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F },   // U+0067 (g)
        { 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00 },   // U+0068 (h)
        { 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0069 (i)
        { 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E },   // U+006A (j)
        { 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00 },   // U+006B (k)
        { 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+006C (l)
        { 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00 },   // U+006D (m)
        { 0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00 },   // U+006E (n)
        { 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00 },   // U+006F (o)
        { 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F },   // U+0070 (p)
        { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78 },   // U+0071 (q)
        { 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00 },   // U+0072 (r)
        { 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00 },   // U+0073 (s)
        { 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00 },   // U+0074 (t)
        { 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00 },   // U+0075 (u)
        { 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00 },   // U+0076 (v)
        { 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00 },   // U+0077 (w)
        { 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00 },   // U+0078 (x)
        { 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F },   // U+0079 (y)
        { 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00 },   // U+007A (z)
        { 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00 },   // U+007B ({)
        { 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00 },   // U+007C (|)
        { 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00 },   // U+007D (})
        { 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+007E (~)
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }    // U+007F
    };

    #define GET_CLIP_BOUNDS(x1, y1, x2, y2) { \
        int clip_x1 = CurrentView->ClipStartX, \
            clip_y1 = CurrentView->ClipStartY, \
            clip_x2 = CurrentView->ClipEndX, \
            clip_y2 = CurrentView->ClipEndY; \
        if (x1 < clip_x1) \
            x1 = clip_x1; \
        if (y1 < clip_y1) \
            y1 = clip_y1; \
        if (x2 > clip_x2) \
            x2 = clip_x2; \
        if (y2 > clip_y2) \
            y2 = clip_y2; \
    }

    void  Init() {
        memset(Views, 0, sizeof(Views));
        memset(ViewOutputs, 0, sizeof(ViewOutputs));
        memset(DrawGroups, 0, sizeof(DrawGroups));
        memset(PaletteIndexLines, 0, sizeof(PaletteIndexLines));

        for (Uint32 i = 0; i < MAX_FRAMEBUFFER_HEIGHT; i++)
            ScanLineBuffer[i] = ScanLine();

        DrawToScreen = false;

        // Create views
        CurrentViewIndex = 0;
        CurrentView = &Views[CurrentViewIndex];

        View_SetSize(0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
        Game::State.ViewCount = 1;

        #ifdef ENABLE_STEREOSCOPIC_VIEW
        View_SetSize(1, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
        Game::State.ViewCount = 2;
        #endif

        // Create view outputs
        ViewOutput* viewOutput = &ViewOutputs[0];
        viewOutput->Active = true;
        viewOutput->ViewIndex = 0;
        viewOutput->ScaleType = VOSCALE_RESIZE_TO_SCREEN;

        int tableInd = 0;
        for (int opacity = 0; opacity < 0x100; opacity++) {
            for (int color = 0; color <= ColorMaxValue; color++) {
                MultTablePos[tableInd] = (color * opacity) >> 8;
                MultTableNeg[tableInd] = (color * -opacity) >> 8;
                MultTableInv[tableInd] = (color * (0x100 - opacity)) >> 8;
                tableInd++;
            }
        }
    }

    void View_SetSize(int viewIndex, int width, int height) {
        View* v = &Views[viewIndex];
        v->Width = width;
        v->Height = height;
        v->WidthHalf = width >> 1;
        v->HeightHalf = height >> 1;
        v->Pitch = width;
        v->ClipStartX = 0;
        v->ClipStartY = 0;
        v->ClipEndX = width;
        v->ClipEndY = height;

        v->DirtySize = true;

        Memory::Alloc(&v->Pixels, v->Width * v->Height * sizeof(Pixel), Memory::MEMPOOL_VIEWS, true);
    }
    void View_GetSize(int viewIndex, int* width, int* height) {
        View* v = &Views[viewIndex];
        if (width) *width = v->Width;
        if (height) *height = v->Height;
    }

    inline void PixelSetOpaque(Pixel* src, Pixel* dst, Uint8* multPosTableAt, Uint8* multNegTableAt, Uint8* multInvTableAt) {
        *dst = *src;
    }
    inline void PixelSetHalfTransparent(Pixel* src, Pixel* dst, Uint8* multPosTableAt, Uint8* multNegTableAt, Uint8* multInvTableAt) {
        *dst = Pixel(
            (src->R + dst->R) >> 1,
            (src->G + dst->G) >> 1,
            (src->B + dst->B) >> 1);
    }
    inline void PixelSetTransparent(Pixel* src, Pixel* dst, Uint8* multPosTableAt, Uint8* multNegTableAt, Uint8* multInvTableAt) {
        *dst = Pixel(
            multPosTableAt[src->R] + multInvTableAt[dst->R],
            multPosTableAt[src->G] + multInvTableAt[dst->G],
            multPosTableAt[src->B] + multInvTableAt[dst->B]);
    }
    inline void PixelSetAdditive(Pixel* src, Pixel* dst, Uint8* multPosTableAt, Uint8* multNegTableAt, Uint8* multInvTableAt) {
        int R = multPosTableAt[src->R] + dst->R; if (R > ColorMaxValue) R = ColorMaxValue;
        int G = multPosTableAt[src->G] + dst->G; if (G > ColorMaxValue) G = ColorMaxValue;
        int B = multPosTableAt[src->B] + dst->B; if (B > ColorMaxValue) B = ColorMaxValue;
        *dst = Pixel(R, G, B);
    }
    inline void PixelSetSubtract(Pixel* src, Pixel* dst, Uint8* multPosTableAt, Uint8* multNegTableAt, Uint8* multInvTableAt) {
        int R = multNegTableAt[src->R] + dst->R; if (R < 0) R = 0;
        int G = multNegTableAt[src->G] + dst->G; if (G < 0) G = 0;
        int B = multNegTableAt[src->B] + dst->B; if (B < 0) B = 0;
        *dst = Pixel(R, G, B);
    }
    inline void PixelSetMatchEqual(Pixel* src, Pixel* dst, Uint8* multPosTableAt, Uint8* multNegTableAt, Uint8* multInvTableAt) {
        if (*dst == CompareColor)
            *dst = *src;
    }
    inline void PixelSetMatchNotEqual(Pixel* src, Pixel* dst, Uint8* multPosTableAt, Uint8* multNegTableAt, Uint8* multInvTableAt) {
        if (*dst != CompareColor)
            *dst = *src;
    }
    inline void PixelSetFiltered(Pixel* src, Pixel* dst, Uint8* multPosTableAt, Uint8* multNegTableAt, Uint8* multInvTableAt) {
        *dst = FilterTable[dst->Full];
    }

    // Palette things
    void  PaletteLoad(CString filename) {}
    Color PaletteGetColor(int paletteIndex, int colorIndex) {
        Pixel px = Palette[paletteIndex][colorIndex];
        return Color(px.R << 3, px.G << 3, px.B << 3, 0xFF);
    }
    void  PaletteSetColor(int paletteIndex, int colorIndex, Color color) {
        Palette[paletteIndex][colorIndex] = color;
    }
    void  PaletteMixPalettes(int destPaletteIndex, int paletteIndexA, int paletteIndexB, int mixRatio, int colorIndexStart, int colorCount) {
        Pixel* palette = Palette[destPaletteIndex];
        Pixel* paletteA = Palette[paletteIndexA];
        Pixel* paletteB = Palette[paletteIndexB];
        Uint8* multPosTableAt = &MultTablePos[mixRatio << COLOR_BITS];
        Uint8* multInvTableAt = &MultTableInv[mixRatio << COLOR_BITS];
        for (int c = colorIndexStart; c < colorIndexStart + colorCount; c++) {
            palette[c] = Pixel(
                multInvTableAt[paletteA[c].R] + multPosTableAt[paletteB[c].R],
                multInvTableAt[paletteA[c].G] + multPosTableAt[paletteB[c].G],
                multInvTableAt[paletteA[c].B] + multPosTableAt[paletteB[c].B]);
        }
    }
    void  PaletteRotateColorsLeft(int paletteIndex, int colorIndexStart, int colorCount) {
        if (colorCount > 0x100 - colorIndexStart)
            colorCount = 0x100 - colorIndexStart;

        Pixel* palette = Palette[paletteIndex];
        Uint32 temp = palette[colorIndexStart];
        for (int i = colorIndexStart + 1; i < colorIndexStart + colorCount; i++) {
            palette[i - 1] = palette[i];
        }
        palette[colorIndexStart + colorCount - 1] = temp;
    }
    void  PaletteRotateColorsRight(int paletteIndex, int colorIndexStart, int colorCount) {
        if (colorCount > 0x100 - colorIndexStart)
            colorCount = 0x100 - colorIndexStart;

        Pixel* palette = Palette[paletteIndex];
        Uint32 temp = palette[colorIndexStart + colorCount - 1];
        for (int i = colorIndexStart + colorCount - 1; i >= colorIndexStart; i--) {
            palette[i] = palette[i - 1];
        }
        palette[colorIndexStart] = temp;
    }
    void  PaletteCopyColors(int srcPaletteIndex, int srcColorIndexStart, int destPaletteIndex, int destColorIndexStart, int colorCount) {
        if (colorCount > 0x100 - destColorIndexStart)
            colorCount = 0x100 - destColorIndexStart;
        if (colorCount > 0x100 - srcColorIndexStart)
            colorCount = 0x100 - srcColorIndexStart;

        memcpy(&Palette[destPaletteIndex][destColorIndexStart], &Palette[srcPaletteIndex][srcColorIndexStart], colorCount * sizeof(Pixel));
    }
    void  PaletteSetPaletteIndexLines(int paletteIndex, int lineStart, int lineEnd) {
        int lastLine = MAX_FRAMEBUFFER_HEIGHT - 1;
        if (lineStart > lastLine)
            lineStart = lastLine;
        if (lineEnd > lastLine)
            lineEnd = lastLine;

        for (int i = lineStart; i < lineEnd; i++)
            PaletteIndexLines[i] = (Uint8)paletteIndex;
    }

    // Be sure to set DidDraw if any of these drawing functions draw at least a pixel
    void DrawSpriteImage(Image* texture, int x, int y, int srcx, int srcy, int w, int h, int flipFlag, int blendFlag, int opacity) {
        Uint8*  srcPx = texture->Data;
        Uint32  srcStride = texture->Width;
        Uint8*  srcPxLine;

        Pixel*  dstPx = (Pixel*)CurrentView->Pixels;
        Uint32  dstStride = CurrentView->Pitch;
        Pixel*  dstPxLine;

        if (!DrawToScreen) {
            x -= CurrentView->X;
            y -= CurrentView->Y;
        }

        int src_x1 = srcx;
        int src_y1 = srcy;
        int src_x2 = srcx + w - 1;
        int src_y2 = srcy + h - 1;

        int dst_x1 = x;
        int dst_y1 = y;
        int dst_x2 = x + w;
        int dst_y2 = y + h;

        {
            int clip_x1 = CurrentView->ClipStartX,
                clip_y1 = CurrentView->ClipStartY,
                clip_x2 = CurrentView->ClipEndX,
                clip_y2 = CurrentView->ClipEndY;

            if (dst_x2 > clip_x2)
                dst_x2 = clip_x2;
            if (dst_y2 > clip_y2)
                dst_y2 = clip_y2;

            if (dst_x1 < clip_x1) {
                src_x1 += clip_x1 - dst_x1;
                src_x2 -= clip_x1 - dst_x1;
                dst_x1 = clip_x1;
            }
            if (dst_y1 < clip_y1) {
                src_y1 += clip_y1 - dst_y1;
                src_y2 -= clip_y1 - dst_y1;
                dst_y1 = clip_y1;
            }
        }

        if (dst_x1 >= dst_x2 || dst_y1 >= dst_y2)
            return;

        DidDraw = true;

        #define DRAW_PLACEPIXEL(pixelFunction) \
            if ((color = srcPxLine[src_x])) \
                pixelFunction(&index[color], &dstPxLine[dst_x], multPosTableAt, multNegTableAt, multInvTableAt);

        #define DRAW_NOFLIP(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
            srcPxLine = srcPx + src_strideY; \
            dstPxLine = dstPx + dst_strideY; \
            index = &Palette[PaletteIndexLines[dst_y]][0]; \
            for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; dst_x++, src_x++) { \
                placePixelMacro(pixelFunction) \
            } \
            dst_strideY += dstStride; src_strideY += srcStride; \
        }
        #define DRAW_FLIPX(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
            srcPxLine = srcPx + src_strideY; \
            dstPxLine = dstPx + dst_strideY; \
            index = &Palette[PaletteIndexLines[dst_y]][0]; \
            for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; dst_x++, src_x--) { \
                placePixelMacro(pixelFunction) \
            } \
            dst_strideY += dstStride; src_strideY += srcStride; \
        }
        #define DRAW_FLIPY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
            srcPxLine = srcPx + src_strideY; \
            dstPxLine = dstPx + dst_strideY; \
            index = &Palette[PaletteIndexLines[dst_y]][0]; \
            for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; dst_x++, src_x++) { \
                placePixelMacro(pixelFunction) \
            } \
            dst_strideY += dstStride; src_strideY -= srcStride; \
        }
        #define DRAW_FLIPXY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
            srcPxLine = srcPx + src_strideY; \
            dstPxLine = dstPx + dst_strideY; \
            index = &Palette[PaletteIndexLines[dst_y]][0]; \
            for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; dst_x++, src_x--) { \
                placePixelMacro(pixelFunction) \
            } \
            dst_strideY += dstStride; src_strideY -= srcStride; \
        }

        #define BLENDFLAGS(flipMacro, placePixelMacro) \
            switch (blendFlag) { \
                case BLEND_NONE: \
                    flipMacro(PixelSetOpaque, placePixelMacro); \
                    break; \
                case BLEND_TRANSPARENT: \
                    flipMacro(PixelSetTransparent, placePixelMacro); \
                    break; \
                case BLEND_ADDITIVE: \
                    flipMacro(PixelSetAdditive, placePixelMacro); \
                    break; \
                case BLEND_SUBTRACT: \
                    flipMacro(PixelSetSubtract, placePixelMacro); \
                    break; \
                case BLEND_MATCH: \
                    flipMacro(PixelSetMatchEqual, placePixelMacro); \
                    break; \
                case BLEND_NON_MATCH: \
                    flipMacro(PixelSetMatchNotEqual, placePixelMacro); \
                    break; \
                case BLEND_FILTERED: \
                    flipMacro(PixelSetFiltered, placePixelMacro); \
                    break; \
            }

        Uint8  color;
        Pixel* index;
        int dst_strideY, src_strideY;

        Uint8* multPosTableAt = &MultTablePos[opacity << COLOR_BITS];
        Uint8* multNegTableAt = &MultTableNeg[opacity << COLOR_BITS];
        Uint8* multInvTableAt = &MultTableInv[opacity << COLOR_BITS];

        switch (flipFlag) {
            case 0:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y1 * srcStride;
                BLENDFLAGS(DRAW_NOFLIP, DRAW_PLACEPIXEL);
                break;
            case 1:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y1 * srcStride;
                BLENDFLAGS(DRAW_FLIPX, DRAW_PLACEPIXEL);
                break;
            case 2:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y2 * srcStride;
                BLENDFLAGS(DRAW_FLIPY, DRAW_PLACEPIXEL);
                break;
            case 3:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y2 * srcStride;
                BLENDFLAGS(DRAW_FLIPXY, DRAW_PLACEPIXEL);
                break;
        }

        #undef DRAW_PLACEPIXEL
        #undef DRAW_NOFLIP
        #undef DRAW_FLIPX
        #undef DRAW_FLIPY
        #undef DRAW_FLIPXY
        #undef BLENDFLAGS
    }
    void DrawSpriteImageTransformed(Image* texture, int x, int y, int offx, int offy, int w, int h, int sx, int sy, int sw, int sh, int flipFlag, int rotation, int blendFlag, int opacity) {
        Uint8*  srcPx = texture->Data;
        Uint32  srcStride = texture->Width;
        // Uint32* srcPxLine;

        Pixel*  dstPx = (Pixel*)CurrentView->Pixels;
        Uint32  dstStride = CurrentView->Pitch;
        Pixel*  dstPxLine;

        if (!DrawToScreen) {
            x -= CurrentView->X;
            y -= CurrentView->Y;
        }

        int src_x;
        int src_y;
        int src_x1 = sx;
        int src_y1 = sy;
        int src_x2 = sx + sw - 1;
        int src_y2 = sy + sh - 1;

        int cos = Math::CosTbl_0x200[rotation & 0x1FF];
        int sin = Math::SinTbl_0x200[rotation & 0x1FF];
        int rcos = Math::CosTbl_0x200[(0x200 - rotation + 0x200) & 0x1FF];
        int rsin = Math::SinTbl_0x200[(0x200 - rotation + 0x200) & 0x1FF];

        int _x1 = offx;
        int _y1 = offy;
        int _x2 = offx + w;
        int _y2 = offy + h;

        switch (flipFlag) {
    		case 1: _x1 = -offx - w; _x2 = -offx; break;
    		case 2: _y1 = -offy - h; _y2 = -offy; break;
            case 3:
    			_x1 = -offx - w; _x2 = -offx;
    			_y1 = -offy - h; _y2 = -offy;
        }

        int dst_x1 = _x1;
        int dst_y1 = _y1;
        int dst_x2 = _x2;
        int dst_y2 = _y2;

        #define SET_MIN(a, b) if (a > b) a = b;
        #define SET_MAX(a, b) if (a < b) a = b;

        int px, py, cx, cy;

        py = _y1;
        {
            px = _x1;
            cx = (px * cos - py * sin); SET_MIN(dst_x1, cx); SET_MAX(dst_x2, cx);
            cy = (px * sin + py * cos); SET_MIN(dst_y1, cy); SET_MAX(dst_y2, cy);

            px = _x2;
            cx = (px * cos - py * sin); SET_MIN(dst_x1, cx); SET_MAX(dst_x2, cx);
            cy = (px * sin + py * cos); SET_MIN(dst_y1, cy); SET_MAX(dst_y2, cy);
        }

        py = _y2;
        {
            px = _x1;
            cx = (px * cos - py * sin); SET_MIN(dst_x1, cx); SET_MAX(dst_x2, cx);
            cy = (px * sin + py * cos); SET_MIN(dst_y1, cy); SET_MAX(dst_y2, cy);

            px = _x2;
            cx = (px * cos - py * sin); SET_MIN(dst_x1, cx); SET_MAX(dst_x2, cx);
            cy = (px * sin + py * cos); SET_MIN(dst_y1, cy); SET_MAX(dst_y2, cy);
        }

        #undef SET_MIN
        #undef SET_MAX

        dst_x1 >>= 9;
        dst_y1 >>= 9;
        dst_x2 >>= 9;
        dst_y2 >>= 9;

        dst_x1 += x;
        dst_y1 += y;
        dst_x2 += x + 1;
        dst_y2 += y + 1;

        {
            int clip_x1 = CurrentView->ClipStartX,
                clip_y1 = CurrentView->ClipStartY,
                clip_x2 = CurrentView->ClipEndX,
                clip_y2 = CurrentView->ClipEndY;

            if (dst_x2 > clip_x2)
                dst_x2 = clip_x2;
            if (dst_y2 > clip_y2)
                dst_y2 = clip_y2;
            if (dst_x1 < clip_x1)
                dst_x1 = clip_x1;
            if (dst_y1 < clip_y1)
                dst_y1 = clip_y1;
        }

        if (dst_x1 >= dst_x2 || dst_y1 >= dst_y2)
            return;

        DidDraw = true;

        #define DRAW_PLACEPIXEL(pixelFunction) \
            if ((color = srcPx[src_x + src_strideY])) \
                pixelFunction(&index[color], &dstPxLine[dst_x], multPosTableAt, multNegTableAt, multInvTableAt);

        #define DRAW_NOFLIP(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
            i_y_rsin = -i_y * rsin; \
            i_y_rcos =  i_y * rcos; \
            dstPxLine = dstPx + dst_strideY; \
            index = &Palette[PaletteIndexLines[dst_y]][0]; \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x1 + (src_x - _x1) * sw / w); \
                    src_strideY = (src_y1 + (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
            } \
            dst_strideY += dstStride; \
        }
        #define DRAW_FLIPX(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
            i_y_rsin = -i_y * rsin; \
            i_y_rcos =  i_y * rcos; \
            dstPxLine = dstPx + dst_strideY; \
            index = &Palette[PaletteIndexLines[dst_y]][0]; \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x2 - (src_x - _x1) * sw / w); \
                    src_strideY = (src_y1 + (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
            } \
            dst_strideY += dstStride; \
        }
        #define DRAW_FLIPY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
            i_y_rsin = -i_y * rsin; \
            i_y_rcos =  i_y * rcos; \
            dstPxLine = dstPx + dst_strideY; \
            index = &Palette[PaletteIndexLines[dst_y]][0]; \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x1 + (src_x - _x1) * sw / w); \
                    src_strideY = (src_y2 - (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
            } \
            dst_strideY += dstStride; \
        }
        #define DRAW_FLIPXY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
            i_y_rsin = -i_y * rsin; \
            i_y_rcos =  i_y * rcos; \
            dstPxLine = dstPx + dst_strideY; \
            index = &Palette[PaletteIndexLines[dst_y]][0]; \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x2 - (src_x - _x1) * sw / w); \
                    src_strideY = (src_y2 - (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
            } \
            dst_strideY += dstStride; \
        }

        #define BLENDFLAGS(flipMacro, placePixelMacro) \
            switch (blendFlag) { \
                case BLEND_NONE: \
                    flipMacro(PixelSetOpaque, placePixelMacro); \
                    break; \
                case BLEND_TRANSPARENT: \
                    flipMacro(PixelSetTransparent, placePixelMacro); \
                    break; \
                case BLEND_ADDITIVE: \
                    flipMacro(PixelSetAdditive, placePixelMacro); \
                    break; \
                case BLEND_SUBTRACT: \
                    flipMacro(PixelSetSubtract, placePixelMacro); \
                    break; \
                case BLEND_MATCH: \
                    flipMacro(PixelSetMatchEqual, placePixelMacro); \
                    break; \
                case BLEND_NON_MATCH: \
                    flipMacro(PixelSetMatchNotEqual, placePixelMacro); \
                    break; \
                case BLEND_FILTERED: \
                    flipMacro(PixelSetFiltered, placePixelMacro); \
                    break; \
            }

        Uint32 color;
        Pixel* index;
        int i_y_rsin, i_y_rcos;
        int dst_strideY, src_strideY;

        Uint8* multPosTableAt = &MultTablePos[opacity << COLOR_BITS];
        Uint8* multNegTableAt = &MultTableNeg[opacity << COLOR_BITS];
        Uint8* multInvTableAt = &MultTableInv[opacity << COLOR_BITS];

        switch (flipFlag) {
            case 0:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y1 * srcStride;
                BLENDFLAGS(DRAW_NOFLIP, DRAW_PLACEPIXEL);
                break;
            case 1:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y1 * srcStride;
                BLENDFLAGS(DRAW_FLIPX, DRAW_PLACEPIXEL);
                break;
            case 2:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y2 * srcStride;
                BLENDFLAGS(DRAW_FLIPY, DRAW_PLACEPIXEL);
                break;
            case 3:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y2 * srcStride;
                BLENDFLAGS(DRAW_FLIPXY, DRAW_PLACEPIXEL);
                break;
        }

        #undef DRAW_PLACEPIXEL
        #undef DRAW_NOFLIP
        #undef DRAW_FLIPX
        #undef DRAW_FLIPY
        #undef DRAW_FLIPXY
        #undef BLENDFLAGS
    }

    // Drawing Images
    void DrawSprite(Resource sprite, int animation, int frame, Vector2* position) {
        if (sprite < 0 || sprite >= MAX_SPRITES)
            return;

        Resources::ResSprite* resSprite = &Resources::ResourceSprites[sprite];
        Frame* frameData = &resSprite->SpriteData.Frames[resSprite->SpriteData.Animations[animation].StartFrameIndex + frame];
        if (frameData->Image < 0 || frameData->Image >= MAX_IMAGES)
            return;

        Image* image = &Resources::ResourceImages[frameData->Image].ImageData;

        int blendFlag = Scene::CurrentEntity->BlendFlag;
        int opacity = M_CLAMP(Scene::CurrentEntity->Opacity, 0, 255);

        int flipFlag = 0;
        int rotation = 0;
        Vector2 scale = Vector2(0x10000, 0x10000);
        if (Scene::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_FLIP) {
            flipFlag = Scene::CurrentEntity->FlipFlag;
        }
        if (Scene::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_ROTATE) {
            rotation = Scene::CurrentEntity->Rotation;

            Uint8 rotationFlag = resSprite->SpriteData.Animations[animation].RotationFlag;
            switch (rotationFlag) {
                default:
                case RotationFlag_NoRotation:
                    rotation = 0;
                    break;
                case RotationFlag_RotationFull:
                    rotation = (rotation) & 0x1FF;
                    break;
                case RotationFlag_RotationEighth:
                    rotation = (rotation + 0x20) & 0x1C0;
                    break;
                case RotationFlag_RotationQuarter:
                    rotation = (rotation + 0x40) & 0x180;
                    break;
                case RotationFlag_RotationHalf:
                    rotation = (rotation + 0x80) & 0x100;
                    break;
            }
        }
        if (Scene::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_SCALE) {
            scale = Scene::CurrentEntity->Scale;
        }

        if (Scene::CurrentEntity->TransformFlag & (TRANSFORM_ALLOW_ROTATE | TRANSFORM_ALLOW_SCALE)) {
            scale.X.Full >>= 8;
            scale.Y.Full >>= 8;
            DrawSpriteImageTransformed(image,
                position->X.Whole, position->Y.Whole,
                (frameData->OffsetX * scale.X.Full) >> 8, (frameData->OffsetY * scale.Y.Full) >> 8,
                (frameData->Width * scale.X.Full) >> 8, (frameData->Height * scale.Y.Full) >> 8,
                frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height,
                flipFlag, rotation, blendFlag, opacity);
        }
        else {
            switch (flipFlag) {
                case FLIPXY_NONE:
                    DrawSpriteImage(image,
                        position->X.Whole + frameData->OffsetX,
                        position->Y.Whole + frameData->OffsetY,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, flipFlag, blendFlag, opacity);
                    break;
                case FLIPXY_X:
                    DrawSpriteImage(image,
                        position->X.Whole - frameData->OffsetX - frameData->Width,
                        position->Y.Whole + frameData->OffsetY,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, flipFlag, blendFlag, opacity);
                    break;
                case FLIPXY_Y:
                    DrawSpriteImage(image,
                        position->X.Whole + frameData->OffsetX,
                        position->Y.Whole - frameData->OffsetY - frameData->Height,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, flipFlag, blendFlag, opacity);
                    break;
                case FLIPXY_XY:
                    DrawSpriteImage(image,
                        position->X.Whole - frameData->OffsetX - frameData->Width,
                        position->Y.Whole - frameData->OffsetY - frameData->Height,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, flipFlag, blendFlag, opacity);
                    break;
            }
        }
    }
    void DrawAnimation(Animator* animator, Vector2* position) {
        if (!animator || !animator->StartFrame)
            return;

        Frame* frameData = &animator->StartFrame[animator->FrameIndex];
        Image* image = &Resources::ResourceImages[frameData->Image].ImageData;

        int blendFlag = Scene::CurrentEntity->BlendFlag;
        int opacity = M_CLAMP(Scene::CurrentEntity->Opacity, 0, 255);

        int flipFlag = 0;
        int rotation = 0;
        Vector2 scale = Vector2(0x10000, 0x10000);
        if (Scene::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_FLIP) {
            flipFlag = Scene::CurrentEntity->FlipFlag;
        }
        if (Scene::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_ROTATE) {
            rotation = Scene::CurrentEntity->Rotation;

            Uint8 rotationFlag = animator->RotationFlag;
            switch (rotationFlag) {
                default:
                case RotationFlag_NoRotation:
                    rotation = 0;
                    break;
                case RotationFlag_RotationFull:
                    rotation = (rotation) & 0x1FF;
                    break;
                case RotationFlag_RotationEighth:
                    rotation = (rotation + 0x20) & 0x1C0;
                    break;
                case RotationFlag_RotationQuarter:
                    rotation = (rotation + 0x40) & 0x180;
                    break;
                case RotationFlag_RotationHalf:
                    rotation = (rotation + 0x80) & 0x100;
                    break;
            }
        }
        if (Scene::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_SCALE) {
            scale = Scene::CurrentEntity->Scale;
        }

        if (Scene::CurrentEntity->TransformFlag & (TRANSFORM_ALLOW_ROTATE | TRANSFORM_ALLOW_SCALE)) {
            scale.X.Full >>= 8;
            scale.Y.Full >>= 8;
            DrawSpriteImageTransformed(image,
                position->X.Whole, position->Y.Whole,
                (frameData->OffsetX * scale.X.Full) >> 8, (frameData->OffsetY * scale.Y.Full) >> 8,
                (frameData->Width * scale.X.Full) >> 8, (frameData->Height * scale.Y.Full) >> 8,
                frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height,
                flipFlag, rotation, blendFlag, opacity);
        }
        else {
            switch (flipFlag) {
                case FLIPXY_NONE:
                    DrawSpriteImage(image,
                        position->X.Whole + frameData->OffsetX,
                        position->Y.Whole + frameData->OffsetY,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, flipFlag, blendFlag, opacity);
                    break;
                case FLIPXY_X:
                    DrawSpriteImage(image,
                        position->X.Whole - frameData->OffsetX - frameData->Width,
                        position->Y.Whole + frameData->OffsetY,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, flipFlag, blendFlag, opacity);
                    break;
                case FLIPXY_Y:
                    DrawSpriteImage(image,
                        position->X.Whole + frameData->OffsetX,
                        position->Y.Whole - frameData->OffsetY - frameData->Height,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, flipFlag, blendFlag, opacity);
                    break;
                case FLIPXY_XY:
                    DrawSpriteImage(image,
                        position->X.Whole - frameData->OffsetX - frameData->Width,
                        position->Y.Whole - frameData->OffsetY - frameData->Height,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, flipFlag, blendFlag, opacity);
                    break;
            }
        }
    }
    void DrawImage(Resource image, Vector2* position) {
        if (image < 0 || image >= MAX_IMAGES)
            return;

        Resources::ResImage resImage = Resources::ResourceImages[image];
        DrawSpriteImage(&resImage.ImageData, position->X.Whole, position->Y.Whole, 0, 0, resImage.ImageData.Width, resImage.ImageData.Height, 0, 1, 0x80);
    }
    void DrawSpriteText(String* string, Vector2* position, Resource spriteIndex, int animIndex, int startIndex, int endIndex, int alignment, int spacing, Vector2* offsets) {
        if (spriteIndex < 0 || spriteIndex >= MAX_SPRITES)
            return;
        if (!string)
            return;
        if (animIndex < 0 || animIndex >= Resources::ResourceSprites[spriteIndex].SpriteData.AnimationCount)
            return;

        auto animEntry = &Resources::ResourceSprites[spriteIndex].SpriteData.Animations[animIndex];
        if (animEntry->FrameCount == 0)
            return;

        if (startIndex > 0)
            startIndex = M_MIN(startIndex, string->Length - 1);
        else
            startIndex = 0;

        if (endIndex > 0)
            endIndex = M_MIN(endIndex, string->Length);
        else
            endIndex = string->Length;

        int x = position->X.Whole;
        int y = position->Y.Whole;

        int blendFlag = Scene::CurrentEntity->BlendFlag;
        int opacity = M_CLAMP(Scene::CurrentEntity->Opacity, 0, 255);

        int xoff = 0;
        switch (alignment) {
        case TEXT_ALIGN_CENTER:
            for (int i = startIndex; i < endIndex; i++) {
                auto character = string->Text[i];
                if (character < 0 || character >= animEntry->FrameCount)
                    continue;

                auto frame = &Resources::ResourceSprites[spriteIndex].SpriteData.Frames[animEntry->StartFrameIndex + character];
                xoff += frame->Width + spacing;
            }
            x -= xoff >> 1;
        case TEXT_ALIGN_LEFT:
            if (offsets) {
                for (int i = startIndex; i < endIndex; i++) {
                    auto character = string->Text[i];
                    if (character < 0 || character >= animEntry->FrameCount)
                        continue;

                    auto frameData = &Resources::ResourceSprites[spriteIndex].SpriteData.Frames[animEntry->StartFrameIndex + character];
                    Image* image = &Resources::ResourceImages[frameData->Image].ImageData;
                    DrawSpriteImage(image,
                        x + offsets[i].X.Whole,
                        y + offsets[i].Y.Whole + frameData->OffsetY,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, 0, blendFlag, opacity);

                    x += frameData->Width + spacing;
                }
            }
            else {
                for (int i = startIndex; i < endIndex; i++) {
                    auto character = string->Text[i];
                    if (character < 0 || character >= animEntry->FrameCount)
                        continue;

                    auto frameData = &Resources::ResourceSprites[spriteIndex].SpriteData.Frames[animEntry->StartFrameIndex + character];
                    Image* image = &Resources::ResourceImages[frameData->Image].ImageData;
                    DrawSpriteImage(image,
                        x,
                        y + frameData->OffsetY,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, 0, blendFlag, opacity);

                    x += frameData->Width + spacing;
                }
            }
            break;
        case TEXT_ALIGN_RIGHT:
            if (offsets) {
                for (int i = endIndex - 1; i >= startIndex; i--) {
                    auto character = string->Text[i];
                    if (character < 0 || character >= animEntry->FrameCount)
                        continue;

                    auto frameData = &Resources::ResourceSprites[spriteIndex].SpriteData.Frames[animEntry->StartFrameIndex + character];
                    Image* image = &Resources::ResourceImages[frameData->Image].ImageData;

                    x -= frameData->Width;

                    DrawSpriteImage(image,
                        x + offsets[i].X.Whole,
                        y + offsets[i].Y.Whole + frameData->OffsetY,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, 0, blendFlag, opacity);

                    x -= spacing;
                }
            }
            else {
                for (int i = endIndex - 1; i >= startIndex; i--) {
                    auto character = string->Text[i];
                    if (character < 0 || character >= animEntry->FrameCount)
                        continue;

                    auto frameData = &Resources::ResourceSprites[spriteIndex].SpriteData.Frames[animEntry->StartFrameIndex + character];
                    Image* image = &Resources::ResourceImages[frameData->Image].ImageData;

                    x -= frameData->Width;

                    DrawSpriteImage(image,
                        x,
                        y + frameData->OffsetY,
                        frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height, 0, blendFlag, opacity);

                    x -= spacing;
                }
            }
            break;
        }
    }
    void DrawDebugText(CString text, int x, int y, Color color) {
        View* view = &Views[0];
        int pitch = view->Pitch;
        for (size_t i = 0; i < strlen(text); i++) {
            char letter = text[i];
            int rowy = y;
            for (int row = 0; row < 8; row++) {
                int rowx = x;
                char rowBits = font8x8_basic[letter][row];
                int ypitch = y * pitch;

                if (rowBits & 1)
                    view->Pixels[rowx + ypitch] = color;
                rowBits >>= 1; rowx++;

                if (rowBits & 1)
                    view->Pixels[rowx + ypitch] = color;
                rowBits >>= 1; rowx++;

                if (rowBits & 1)
                    view->Pixels[rowx + ypitch] = color;
                rowBits >>= 1; rowx++;

                if (rowBits & 1)
                    view->Pixels[rowx + ypitch] = color;
                rowBits >>= 1; rowx++;

                if (rowBits & 1)
                    view->Pixels[rowx + ypitch] = color;
                rowBits >>= 1; rowx++;

                if (rowBits & 1)
                    view->Pixels[rowx + ypitch] = color;
                rowBits >>= 1; rowx++;

                if (rowBits & 1)
                    view->Pixels[rowx + ypitch] = color;
                rowBits >>= 1; rowx++;

                if (rowBits & 1)
                    view->Pixels[rowx + ypitch] = color;
                rowBits >>= 1; rowx++;

                y++;
            }

            x += 8;
            y = rowy;
        }
    }

    // Tile-related things
    void DrawTile(Tile tile, Vector2* position, bool flipX, bool flipY) {
        if (tile == TILE_EMPTY)
            return;

        Image image;
        image.Data = Scene::TileImageData;
        image.Width = 16;
        image.Height = MAX_TILE_COUNT * 16;

        int blendFlag = Scene::CurrentEntity->BlendFlag;
        int opacity = M_CLAMP(Scene::CurrentEntity->Opacity, 0, 255);

        int flipFlag = 0;
        int rotation = 0;
        Vector2 scale = Vector2(0x10000, 0x10000);
        if (Scene::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_FLIP) {
            if (tile.FlipX)
                flipFlag |= FLIPXY_X;
            if (tile.FlipY)
                flipFlag |= FLIPXY_Y;
        }
        if (Scene::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_ROTATE) {
            rotation = Scene::CurrentEntity->Rotation;
        }
        if (Scene::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_SCALE) {
            scale = Scene::CurrentEntity->Scale;
        }

        int width = TILE_SIZE, height = TILE_SIZE;
        int offsetX = -TILE_SIZE >> 1, offsetY = -TILE_SIZE >> 1;

        if (Scene::CurrentEntity->TransformFlag & (TRANSFORM_ALLOW_ROTATE | TRANSFORM_ALLOW_SCALE)) {
            scale.X.Full >>= 8;
            scale.Y.Full >>= 8;
            DrawSpriteImageTransformed(&image,
                position->X.Whole, position->Y.Whole,
                (offsetX * scale.X.Full) >> 8, (offsetY * scale.Y.Full) >> 8,
                (width * scale.X.Full) >> 8, (height * scale.Y.Full) >> 8,
                0, tile.ID << TILE_SIZE_IN_BITS, TILE_SIZE, TILE_SIZE,
                flipFlag, rotation, blendFlag, opacity);
        }
        else {
            switch (flipFlag) {
            case FLIPXY_NONE:
                DrawSpriteImage(&image,
                    position->X.Whole + offsetX,
                    position->Y.Whole + offsetY,
                    0, tile.ID << TILE_SIZE_IN_BITS, TILE_SIZE, TILE_SIZE, flipFlag, blendFlag, opacity);
                break;
            case FLIPXY_X:
                DrawSpriteImage(&image,
                    position->X.Whole - offsetX - width,
                    position->Y.Whole + offsetY,
                    0, tile.ID << TILE_SIZE_IN_BITS, TILE_SIZE, TILE_SIZE, flipFlag, blendFlag, opacity);
                break;
            case FLIPXY_Y:
                DrawSpriteImage(&image,
                    position->X.Whole + offsetX,
                    position->Y.Whole - offsetY - height,
                    0, tile.ID << TILE_SIZE_IN_BITS, TILE_SIZE, TILE_SIZE, flipFlag, blendFlag, opacity);
                break;
            case FLIPXY_XY:
                DrawSpriteImage(&image,
                    position->X.Whole - offsetX - width,
                    position->Y.Whole - offsetY - height,
                    0, tile.ID << TILE_SIZE_IN_BITS, TILE_SIZE, TILE_SIZE, flipFlag, blendFlag, opacity);
                break;
            }
        }
    }
    void CopyImageToTiles(Resource image, int startTileID, int srcX, int srcY, int srcW, int srcH) {
        int tCount = (srcW * srcH / TILE_SIZE / TILE_SIZE);
        if (image < 0 || image >= MAX_IMAGES)
            return;
        if (startTileID < 0 || startTileID >= (MAX_TILE_COUNT - tCount))
            return;

        Image* imageData = &Resources::ResourceImages[image].ImageData;
        Uint32 srcStride = imageData->Width;

        Uint8* tileSrc;
        Uint8* tileDst;
        const int MAX_TILE_PIXELS = MAX_TILE_COUNT * TILE_SIZE * TILE_SIZE;

        int tSrcX = srcX;
        int tSrcY = srcY;
        for (int tID = startTileID; tID < startTileID + tCount; tID++) {
            Uint8* currentTileImageData = &Scene::TileImageData[tID * TILE_SIZE * TILE_SIZE];
            Uint8* srcPx = &imageData->Data[tSrcX + tSrcY * srcStride];
            for (int row = 0; row < TILE_SIZE * TILE_SIZE; row += TILE_SIZE) {
                memcpy(&currentTileImageData[row], srcPx, TILE_SIZE);
                srcPx += srcStride;
            }

            // Flip tiles horizontally
            tileSrc = &currentTileImageData[0];
            tileDst = &currentTileImageData[MAX_TILE_PIXELS];
            for (int line = 0; line < TILE_SIZE; line++) {
                int xSrc = 0;
                int xDst = TILE_SIZE - 1;
                for (; xSrc < TILE_SIZE; ) {
                    tileDst[xDst] = tileSrc[xSrc];
                    xSrc++;
                    xDst--;
                }
                tileSrc += TILE_SIZE; // Move to next line
                tileDst += TILE_SIZE; // Move to next line
            }

            // Flip tiles vertically
            tileSrc = &currentTileImageData[0];
            tileDst = &currentTileImageData[MAX_TILE_PIXELS << 1];
            {
                int ySrc = 0;
                int yDst = (TILE_SIZE - 1) * TILE_SIZE;
                for (; ySrc < TILE_SIZE * TILE_SIZE; ) {
                    // Copy tile line
                    memcpy(&tileDst[yDst], &tileSrc[ySrc], TILE_SIZE * sizeof(Uint8));
                    ySrc += TILE_SIZE;
                    yDst -= TILE_SIZE;
                }
                tileSrc += TILE_SIZE * TILE_SIZE; // Move to next tile
                tileDst += TILE_SIZE * TILE_SIZE; // Move to next tile
            }

            // Flip tiles horizontally & vertically
            tileSrc = &currentTileImageData[MAX_TILE_PIXELS << 1];
            tileDst = &currentTileImageData[MAX_TILE_PIXELS << 1 | MAX_TILE_PIXELS];
            for (int line = 0; line < TILE_SIZE; line++) {
                int xSrc = 0;
                int xDst = TILE_SIZE - 1;
                for (; xSrc < TILE_SIZE; ) {
                    tileDst[xDst] = tileSrc[xSrc];
                    xSrc++;
                    xDst--;
                }
                tileSrc += TILE_SIZE; // Move to next line
                tileDst += TILE_SIZE; // Move to next line
            }

            // Iterate
            tSrcX += TILE_SIZE;
            if (tSrcX >= srcX + srcW) {
                tSrcX = srcX;
                tSrcY += TILE_SIZE;
            }
        }
        // TODO: Implement
        // Copies to main 16x16 tiles and flipped versions simultaneously? maybe do it separately if 3DS effieciency gives issues
        // memcpy(Scene::TileImageData, tiles16x16.Data, MAX_TILE_PIXELS * sizeof(Uint8));
    }

    // Drawing Primitives
    struct Contour {
        int MinX;
        int MinR;
        int MinG;
        int MinB;
        int MinU;
        int MinV;

        int MaxX;
        int MaxR;
        int MaxG;
        int MaxB;
        int MaxU;
        int MaxV;
    };
    Contour ContourField[MAX_FRAMEBUFFER_HEIGHT];
    void memset16(void* dst, Uint16 val, size_t dwords) {
        #if defined(__GNUC__) && defined(i386)
            int u0, u1, u2;
            __asm__ __volatile__ (
                "cld \n\t"
                "rep ; stosw \n\t"
                : "=&D" (u0), "=&a" (u1), "=&c" (u2)
                : "0" (dst), "1" (val), "2" ((Uint32)dwords)
                : "memory"
            );
        #else
            size_t  _n = (dwords + 3) >> 2;
            Uint16* _p = ((Uint16*)dst);
            Uint16  _val = (val);
            if (dwords == 0)
                return;

            switch (dwords & 3) {
                case 0: do {    *_p++ = _val;   /* fallthrough */
                case 3:         *_p++ = _val;   /* fallthrough */
                case 2:         *_p++ = _val;   /* fallthrough */
                case 1:         *_p++ = _val;   /* fallthrough */
                } while (--_n);
            }
        #endif
    }
    void memset32(void* dst, Uint32 val, size_t dwords) {
        #if defined(__GNUC__) && defined(i386)
            int u0, u1, u2;
            __asm__ __volatile__ (
                "cld \n\t"
                "rep ; stosl \n\t"
                : "=&D" (u0), "=&a" (u1), "=&c" (u2)
                : "0" (dst), "1" (val), "2" ((Uint32)dwords)
                : "memory"
            );
        #else
            size_t  _n = (dwords + 3) >> 2;
            Uint32* _p = ((Uint32*)dst);
            Uint32  _val = (val);
            if (dwords == 0)
                return;

            switch (dwords & 3) {
                case 0: do {    *_p++ = _val;   /* fallthrough */
                case 3:         *_p++ = _val;   /* fallthrough */
                case 2:         *_p++ = _val;   /* fallthrough */
                case 1:         *_p++ = _val;   /* fallthrough */
                } while (--_n);
            }
        #endif
    }
    void DrawLine(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Color color, int blendFlag) {
        Pixel*  dstPx = (Pixel*)CurrentView->Pixels;
        Uint32  dstStride = CurrentView->Pitch;

        if (!DrawToScreen) {
            x1.Whole -= CurrentView->X;
            y1.Whole -= CurrentView->Y;
            x2.Whole -= CurrentView->X;
            y2.Whole -= CurrentView->Y;
        }

        int dst_x1 = x1.Whole;
        int dst_y1 = y1.Whole;
        int dst_x2 = x2.Whole;
        int dst_y2 = y2.Whole;

        int minX = INT_MIN, minY = INT_MIN, maxX = INT_MAX, maxY = INT_MAX;
        GET_CLIP_BOUNDS(minX, minY, maxX, maxY);

        DidDraw = true;

        int dx = M_ABS(dst_x2 - dst_x1), sx = dst_x1 < dst_x2 ? 1 : -1;
        int dy = M_ABS(dst_y2 - dst_y1), sy = dst_y1 < dst_y2 ? 1 : -1;
        int err = (dx > dy ? dx : -dy) / 2, e2;

        #define DRAW_LINE(pixelFunction) while (true) { \
            if (dst_x1 >= minX && dst_y1 >= minY && dst_x1 < maxX && dst_y1 < maxY) \
                pixelFunction(&pxCol, &dstPx[dst_x1 + dst_y1 * dstStride], multPosTableAt, multNegTableAt, multInvTableAt); \
            if (dst_x1 == dst_x2 && dst_y1 == dst_y2) break; \
            e2 = err; \
            if (e2 > -dx) { err -= dy; dst_x1 += sx; } \
            if (e2 <  dy) { err += dx; dst_y1 += sy; } \
        }

        Pixel pxCol = color;
        int   opacity = color.A;

        Uint8* multPosTableAt = &MultTablePos[opacity << COLOR_BITS];
        Uint8* multNegTableAt = &MultTableNeg[opacity << COLOR_BITS];
        Uint8* multInvTableAt = &MultTableInv[opacity << COLOR_BITS];
        // int    dst_strideY = dst_y1 * dstStride;
        switch (blendFlag) {
            case BLEND_NONE:
                DRAW_LINE(PixelSetOpaque);
                break;
            case BLEND_TRANSPARENT:
                DRAW_LINE(PixelSetTransparent);
                break;
            case BLEND_ADDITIVE:
                DRAW_LINE(PixelSetAdditive);
                break;
            case BLEND_SUBTRACT:
                DRAW_LINE(PixelSetSubtract);
                break;
            case BLEND_MATCH:
                DRAW_LINE(PixelSetMatchEqual);
                break;
            case BLEND_NON_MATCH:
                DRAW_LINE(PixelSetMatchNotEqual);
                break;
            case BLEND_FILTERED:
                DRAW_LINE(PixelSetFiltered);
                break;
        }

        #undef DRAW_LINE
    }
    void DrawCircle(Subpixels x, Subpixels y, Subpixels radius, Color color, int blendFlag) {}
    void DrawCircleStroke(Subpixels x, Subpixels y, Subpixels radius, Color color, int blendFlag) {}
    void DrawRing(Subpixels x, Subpixels y, Subpixels innerRadius, Subpixels outerRadius, Color color, int blendFlag) {}
    void DrawEllipse(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Color color, int blendFlag) {}
    void DrawRectangle(Subpixels x, Subpixels y, Subpixels w, Subpixels h, Color color, int blendFlag) {
        Pixel*  dstPx = (Pixel*)CurrentView->Pixels;
        Uint32  dstStride = CurrentView->Pitch;

        if (!DrawToScreen) {
            x.Whole -= CurrentView->X;
            y.Whole -= CurrentView->Y;
        }

        int dst_x1 = x.Whole;
        int dst_y1 = y.Whole;
        int dst_x2 = x.Whole + w.Whole;
        int dst_y2 = y.Whole + h.Whole;

        GET_CLIP_BOUNDS(dst_x1, dst_y1, dst_x2, dst_y2);
        if (dst_x1 >= dst_x2 || dst_y1 >= dst_y2)
            return;

        DidDraw = true;

        #define DRAW_RECT(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
            for (int dst_x = dst_x1; dst_x < dst_x2; dst_x++) { \
                pixelFunction(&pxCol, &dstPx[dst_x + dst_strideY], multPosTableAt, multNegTableAt, multInvTableAt); \
            } \
            dst_strideY += dstStride; \
        }

        Pixel pxCol = color;
        int   opacity = color.A;

        Uint8* multPosTableAt = &MultTablePos[opacity << COLOR_BITS];
        Uint8* multNegTableAt = &MultTableNeg[opacity << COLOR_BITS];
        Uint8* multInvTableAt = &MultTableInv[opacity << COLOR_BITS];
        int    dst_strideY = dst_y1 * dstStride;
        switch (blendFlag) {
            case BLEND_NONE:
                {
                    const int stride = (dst_x2 - dst_x1);
                    for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
                        memset16(&dstPx[dst_x1 + dst_strideY], pxCol, stride);
                        dst_strideY += dstStride;
                    }
                }
                // DRAW_RECT(PixelSetOpaque);
                break;
            case BLEND_TRANSPARENT:
                DRAW_RECT(PixelSetTransparent);
                break;
            case BLEND_ADDITIVE:
                DRAW_RECT(PixelSetAdditive);
                break;
            case BLEND_SUBTRACT:
                DRAW_RECT(PixelSetSubtract);
                break;
            case BLEND_MATCH:
                DRAW_RECT(PixelSetMatchEqual);
                break;
            case BLEND_NON_MATCH:
                DRAW_RECT(PixelSetMatchNotEqual);
                break;
            case BLEND_FILTERED:
                DRAW_RECT(PixelSetFiltered);
                break;
        }

        #undef DRAW_RECT
    }
    void DrawTriangle(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Subpixels x3, Subpixels y3, Color color, int blendFlag) {
        Vector2 positions[3];
        positions[0] = Vector2(x1, y1);
        positions[1] = Vector2(x2, y2);
        positions[2] = Vector2(x3, y3);
        DrawPolygon(positions, color, 3, blendFlag);
    }
    void DrawPolygonTexturedScanLine(Vector2 position1, Vector2 position2, Color color1, Color color2, Vector2 uv1, Vector2 uv2) {
        if (position1.Y.Whole == position2.Y.Whole)
            return;

        Vector2 positionStart = Vector2(position1.X >> 16, position1.Y >> 16);
        Vector2 positionEnd = Vector2(position2.X >> 16, position2.Y >> 16);
        Color   colorStart = color1;
        Color   colorEnd = color2;
        Vector2 uvStart = Vector2(uv1.X >> 16, uv1.Y >> 16);
        Vector2 uvEnd = Vector2(uv2.X >> 16, uv2.Y >> 16);
        if (positionStart.Y > positionEnd.Y) {
            positionStart = Vector2(position2.X >> 16, position2.Y >> 16);
            positionEnd = Vector2(position1.X >> 16, position1.Y >> 16);
            colorStart = color2;
            colorEnd = color1;
            uvStart = Vector2(uv2.X >> 16, uv2.Y >> 16);
            uvEnd = Vector2(uv1.X >> 16, uv1.Y >> 16);
        }

        int minX = INT_MIN, minY = INT_MIN, maxX = INT_MAX, maxY = INT_MAX;
        GET_CLIP_BOUNDS(minX, minY, maxX, maxY);

        int positionEndBoundY = positionEnd.Y + 1;
        if (positionEndBoundY < minY || positionStart.Y >= maxY)
            return;
        if (positionEndBoundY > maxY)
            positionEndBoundY = maxY;

        int     linePosX = positionStart.X << 16;
        int     lineColR = colorStart.R << 16;
        int     lineColG = colorStart.G << 16;
        int     lineColB = colorStart.B << 16;
        Vector2 lineUV = Vector2(uvStart.X << 16, uvStart.Y << 16);

        int     deltaPosY = (positionEnd.Y - positionStart.Y);
        int     deltaPosX = ((positionEnd.X - positionStart.X) << 16) / deltaPosY;
        int     deltaColR = ((colorEnd.R - colorStart.R) << 16) / deltaPosY;
        int     deltaColG = ((colorEnd.G - colorStart.G) << 16) / deltaPosY;
        int     deltaColB = ((colorEnd.B - colorStart.B) << 16) / deltaPosY;
        Vector2 deltaUV = Vector2(((uvEnd.X - uvStart.X) << 16) / deltaPosY, ((uvEnd.Y - uvStart.Y) << 16) / deltaPosY);

        // Advance the lines
        if (positionStart.Y < 0) {
            linePosX -= positionStart.Y * deltaPosX;
            lineColR -= positionStart.Y * deltaColR;
            lineColG -= positionStart.Y * deltaColG;
            lineColB -= positionStart.Y * deltaColB;
            lineUV.X -= positionStart.Y * deltaUV.X;
            lineUV.Y -= positionStart.Y * deltaUV.Y;
            positionStart.Y = 0;
        }

        Contour* contour = &ContourField[positionStart.Y];
        if (positionStart.Y < positionEndBoundY) {
            int lineHeight = positionEndBoundY - positionStart.Y;
            while (lineHeight--) {
                int linePointX = linePosX >> 16;
                if (linePointX <= minX) {
                    contour->MinX = minX;
                    contour->MinR = lineColR;
                    contour->MinG = lineColG;
                    contour->MinB = lineColB;
                    contour->MinU = lineUV.X;
                    contour->MinV = lineUV.Y;
                }
                else if (linePointX >= maxX) {
                    contour->MaxX = maxX;
                    contour->MaxR = lineColR;
                    contour->MaxG = lineColG;
                    contour->MaxB = lineColB;
                    contour->MaxU = lineUV.X;
                    contour->MaxV = lineUV.Y;
                }
                else {
                    if (linePointX < contour->MinX) {
                        contour->MinX = linePointX;
                        contour->MinR = lineColR;
                        contour->MinG = lineColG;
                        contour->MinB = lineColB;
                        contour->MinU = lineUV.X;
                        contour->MinV = lineUV.Y;
                    }
        			if (linePointX > contour->MaxX) {
                        contour->MaxX = linePointX;
                        contour->MaxR = lineColR;
                        contour->MaxG = lineColG;
                        contour->MaxB = lineColB;
                        contour->MaxU = lineUV.X;
                        contour->MaxV = lineUV.Y;
                    }
                }

                linePosX += deltaPosX;
                lineColR += deltaColR;
                lineColG += deltaColG;
                lineColB += deltaColB;
                lineUV.X += deltaUV.X;
                lineUV.Y += deltaUV.Y;
                contour++;
        	}
        }
    }
    void DrawPolygonTextured(Vector2* positions, Color* colors, Vector2* uvs, Image* texture, int vertexCount, int blendFlag) {
        Uint8*  srcPx = texture->Data;
        Uint32  srcStride = texture->Width;

        Vector2* tempVertex;
        int      tempCount;
        if (vertexCount == 0)
            return;

        Pixel*  dstPx = (Pixel*)CurrentView->Pixels;
        Uint32  dstStride = CurrentView->Pitch;

        int minVal = INT_MAX;
        int maxVal = INT_MIN;

        int viewX = 0;
        int viewY = 0;
        if (!DrawToScreen) {
            viewX = CurrentView->X;
            viewY = CurrentView->Y;
        }

        int tempY;
        tempVertex = positions;
        tempCount = vertexCount;
        while (tempCount--) {
            tempY = (tempVertex->Y >> 16) - viewY;
            if (minVal > tempY)
                minVal = tempY;
            if (maxVal < tempY)
                maxVal = tempY;
            tempVertex++;
        }

        int dst_x1 = 0, dst_x2 = 0; // These are unused.
        int dst_y1 = minVal;
        int dst_y2 = maxVal;

        GET_CLIP_BOUNDS(dst_x1, dst_y1, dst_x2, dst_y2);
        if (dst_y1 >= dst_y2)
            return;

        DidDraw = true;

        int scanLineCount = dst_y2 - dst_y1;
        Contour* contourPtr = &ContourField[dst_y1];
        while (scanLineCount--) {
            contourPtr->MinX = INT_MAX;
            contourPtr->MaxX = INT_MIN;
            contourPtr++;
        }

        // Offset vertex positions
        tempVertex = positions;
        tempCount = vertexCount;
        while (tempCount--) {
            tempVertex->X.Whole -= viewX;
            tempVertex->Y.Whole -= viewY;
            tempVertex++;
        }

        // Get scanlines
        Vector2* lastUV = uvs;
        Color*   lastColor = colors;
        Vector2* lastPosition = positions;
        if (vertexCount > 1) {
            int countRem = vertexCount - 1;
            while (countRem--) {
                DrawPolygonTexturedScanLine(lastPosition[0], lastPosition[1], lastColor[0], lastColor[1], lastUV[0], lastUV[1]);
                lastPosition++;
                lastColor++;
                lastUV++;
            }
        }
        DrawPolygonTexturedScanLine(lastPosition[0], positions[0], lastColor[0], colors[0], lastUV[0], uvs[0]);

        // Un-offset vertex positions
        tempVertex = positions;
        tempCount = vertexCount;
        while (tempCount--) {
            tempVertex->X.Whole += viewX;
            tempVertex->Y.Whole += viewY;
            tempVertex++;
        }

        Sint32 colR, colG, colB, colU, colV, dxR, dxG, dxB, dxU, dxV, contLen;
        #define DRAW_POLYGONTEXTURED(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
            Contour contour = ContourField[dst_y]; \
            contLen = contour.MaxX - contour.MinX; \
            if (contLen <= 0) { \
                dst_strideY += dstStride; \
                continue; \
            } \
            colR = contour.MinR; \
            colG = contour.MinG; \
            colB = contour.MinB; \
            colU = contour.MinU; \
            colV = contour.MinV; \
            dxR = (contour.MaxR - colR) / contLen; \
            dxG = (contour.MaxG - colG) / contLen; \
            dxB = (contour.MaxB - colB) / contLen; \
            dxU = (contour.MaxU - colU) / contLen; \
            dxV = (contour.MaxV - colV) / contLen; \
            index = &Palette[PaletteIndexLines[dst_y]][0]; \
            for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
                if ((color = srcPx[(colU >> 16) + (colV >> 16) * srcStride])) { \
                    pxCol = index[color]; \
                    pxCol = Pixel((pxCol.R * colR) >> 24, (pxCol.G * colG) >> 24, (pxCol.B * colB) >> 24); \
                    pixelFunction(&pxCol, &dstPx[dst_x + dst_strideY], multPosTableAt, multNegTableAt, multInvTableAt); \
                } \
                colR += dxR; \
                colG += dxG; \
                colB += dxB; \
                colU += dxU; \
                colV += dxV; \
            } \
            dst_strideY += dstStride; \
        }

        Pixel  pxCol;
        Uint8  color;
        Pixel* index;
        int    opacity = 0xFF;

        Uint8* multPosTableAt = &MultTablePos[opacity << COLOR_BITS];
        Uint8* multNegTableAt = &MultTableNeg[opacity << COLOR_BITS];
        Uint8* multInvTableAt = &MultTableInv[opacity << COLOR_BITS];
        int    dst_strideY = dst_y1 * dstStride;
        switch (blendFlag) {
        case BLEND_NONE:
            DRAW_POLYGONTEXTURED(PixelSetOpaque);
            break;
        case BLEND_TRANSPARENT:
            DRAW_POLYGONTEXTURED(PixelSetTransparent);
            break;
        case BLEND_ADDITIVE:
            DRAW_POLYGONTEXTURED(PixelSetAdditive);
            break;
        case BLEND_SUBTRACT:
            DRAW_POLYGONTEXTURED(PixelSetSubtract);
            break;
        case BLEND_MATCH:
            DRAW_POLYGONTEXTURED(PixelSetMatchEqual);
            break;
        case BLEND_NON_MATCH:
            DRAW_POLYGONTEXTURED(PixelSetMatchNotEqual);
            break;
        case BLEND_FILTERED:
            DRAW_POLYGONTEXTURED(PixelSetFiltered);
            break;
        }

        #undef DRAW_POLYGONTEXTURED
    }
    void DrawPolygonBlendScanLine(Color color1, Color color2, int x1, int y1, int x2, int y2) {
        int xStart = x1 >> 16;
        int xEnd   = x2 >> 16;
        int yStart = y1 >> 16;
        int yEnd   = y2 >> 16;
        Color cStart = color1;
        Color cEnd   = color2;
        if (yStart == yEnd)
            return;

        // swap
        if (yStart > yEnd) {
            xStart = x2 >> 16;
            xEnd   = x1 >> 16;
            yStart = y2 >> 16;
            yEnd   = y1 >> 16;
            cStart = color2;
            cEnd   = color1;
        }

        int minX = INT_MIN, minY = INT_MIN, maxX = INT_MAX, maxY = INT_MAX;
        GET_CLIP_BOUNDS(minX, minY, maxX, maxY);

        int yEndBound = yEnd + 1;
        if (yEndBound < minY || yStart >= maxY)
            return;

        if (yEndBound > maxY)
            yEndBound = maxY;

        int colorBegRED = (cStart.R) << 16;
        int colorEndRED = (cEnd.R) << 16;
        int colorBegGREEN = (cStart.G) << 16;
        int colorEndGREEN = (cEnd.G) << 16;
        int colorBegBLUE = (cStart.B) << 16;
        int colorEndBLUE = (cEnd.B) << 16;

        int linePointSubpxX = xStart << 16;
        int yDiff = (yEnd - yStart);
        int dx = ((xEnd - xStart) << 16) / yDiff;
        int dxRED = 0, dxGREEN = 0, dxBLUE = 0;

        if (colorBegRED != colorEndRED)
            dxRED = (colorEndRED - colorBegRED) / yDiff;
        if (colorBegGREEN != colorEndGREEN)
            dxGREEN = (colorEndGREEN - colorBegGREEN) / yDiff;
        if (colorBegBLUE != colorEndBLUE)
            dxBLUE = (colorEndBLUE - colorBegBLUE) / yDiff;

        if (yStart < 0) {
            linePointSubpxX -= yStart * dx;
            colorBegRED -= yStart * dxRED;
            colorBegGREEN -= yStart * dxGREEN;
            colorBegBLUE -= yStart * dxBLUE;
            yStart = 0;
        }

        Contour* contour = &ContourField[yStart];
        if (yStart < yEndBound) {
            int lineHeight = yEndBound - yStart;
            while (lineHeight--) {
                int linePointX = linePointSubpxX >> 16;

                if (linePointX <= minX) {
                    contour->MinX = minX;
                    contour->MinR = colorBegRED;
                    contour->MinG = colorBegGREEN;
                    contour->MinB = colorBegBLUE;
                }
                else if (linePointX >= maxX) {
                    contour->MaxX = maxX;
                    contour->MaxR = colorBegRED;
                    contour->MaxG = colorBegGREEN;
                    contour->MaxB = colorBegBLUE;
                }
                else {
                    if (linePointX < contour->MinX) {
                        contour->MinX = linePointX;
                        contour->MinR = colorBegRED;
                        contour->MinG = colorBegGREEN;
                        contour->MinB = colorBegBLUE;
                    }
        			if (linePointX > contour->MaxX) {
                        contour->MaxX = linePointX;
                        contour->MaxR = colorBegRED;
                        contour->MaxG = colorBegGREEN;
                        contour->MaxB = colorBegBLUE;
                    }
                }

                linePointSubpxX += dx;
                colorBegRED += dxRED;
                colorBegGREEN += dxGREEN;
                colorBegBLUE += dxBLUE;
                contour++;
        	}
        }
    }
    void DrawPolygonBlend(Vector2* positions, Color* colors, int vertexCount, int blendFlag) {
        Vector2* tempVertex;
        int      tempCount;
        if (vertexCount == 0)
            return;

        Pixel*  dstPx = (Pixel*)CurrentView->Pixels;
        Uint32  dstStride = CurrentView->Pitch;

        int minVal = INT_MAX;
        int maxVal = INT_MIN;

        int viewX = 0;
        int viewY = 0;
        if (!DrawToScreen) {
            viewX = CurrentView->X;
            viewY = CurrentView->Y;
        }

        int tempY;
        tempVertex = positions;
        tempCount = vertexCount;
        while (tempCount--) {
            tempY = (tempVertex->Y >> 16) - viewY;
            if (minVal > tempY)
                minVal = tempY;
            if (maxVal < tempY)
                maxVal = tempY;
            tempVertex++;
        }

        int dst_x1 = 0, dst_x2 = 0; // These are unused.
        int dst_y1 = minVal;
        int dst_y2 = maxVal;

        GET_CLIP_BOUNDS(dst_x1, dst_y1, dst_x2, dst_y2);
        if (dst_y1 >= dst_y2)
            return;

        DidDraw = true;

        int scanLineCount = dst_y2 - dst_y1;
        Contour* contourPtr = &ContourField[dst_y1];
        while (scanLineCount--) {
            contourPtr->MinX = INT_MAX;
            contourPtr->MaxX = INT_MIN;
            contourPtr++;
        }

        // Offset vertex positions
        tempVertex = positions;
        tempCount = vertexCount;
        while (tempCount--) {
            tempVertex->X.Whole -= viewX;
            tempVertex->Y.Whole -= viewY;
            tempVertex++;
        }

        // Get scanlines
        Color* lastColor = colors;
        Vector2* lastVector = positions;
        if (vertexCount > 1) {
            int countRem = vertexCount - 1;
            while (countRem--) {
                DrawPolygonBlendScanLine(lastColor[0], lastColor[1], lastVector[0].X, lastVector[0].Y, lastVector[1].X, lastVector[1].Y);
                lastVector++;
                lastColor++;
            }
        }
        DrawPolygonBlendScanLine(lastColor[0], colors[0], lastVector[0].X, lastVector[0].Y, positions[0].X, positions[0].Y);

        // Un-offset vertex positions
        tempVertex = positions;
        tempCount = vertexCount;
        while (tempCount--) {
            tempVertex->X.Whole += viewX;
            tempVertex->Y.Whole += viewY;
            tempVertex++;
        }

        Sint32 colR, colG, colB, dxR, dxG, dxB, contLen;
        #define DRAW_POLYGONBLEND(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
            Contour contour = ContourField[dst_y]; \
            contLen = contour.MaxX - contour.MinX; \
            if (contLen <= 0) { \
                dst_strideY += dstStride; \
                continue; \
            } \
            colR = contour.MinR; \
            colG = contour.MinG; \
            colB = contour.MinB; \
            dxR = (contour.MaxR - colR) / contLen; \
            dxG = (contour.MaxG - colG) / contLen; \
            dxB = (contour.MaxB - colB) / contLen; \
            for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
                pxCol = Pixel(colR >> 19, colG >> 19, colB >> 19); \
                pixelFunction(&pxCol, &dstPx[dst_x + dst_strideY], multPosTableAt, multNegTableAt, multInvTableAt); \
                colR += dxR; \
                colG += dxG; \
                colB += dxB; \
            } \
            dst_strideY += dstStride; \
        }

        Pixel pxCol;
        int   opacity = 0xFF;

        Uint8* multPosTableAt = &MultTablePos[opacity << COLOR_BITS];
        Uint8* multNegTableAt = &MultTableNeg[opacity << COLOR_BITS];
        Uint8* multInvTableAt = &MultTableInv[opacity << COLOR_BITS];
        int    dst_strideY = dst_y1 * dstStride;
        switch (blendFlag) {
        case BLEND_NONE:
            DRAW_POLYGONBLEND(PixelSetOpaque);
            break;
        case BLEND_TRANSPARENT:
            DRAW_POLYGONBLEND(PixelSetTransparent);
            break;
        case BLEND_ADDITIVE:
            DRAW_POLYGONBLEND(PixelSetAdditive);
            break;
        case BLEND_SUBTRACT:
            DRAW_POLYGONBLEND(PixelSetSubtract);
            break;
        case BLEND_MATCH:
            DRAW_POLYGONBLEND(PixelSetMatchEqual);
            break;
        case BLEND_NON_MATCH:
            DRAW_POLYGONBLEND(PixelSetMatchNotEqual);
            break;
        case BLEND_FILTERED:
            DRAW_POLYGONBLEND(PixelSetFiltered);
            break;
        }

        #undef DRAW_POLYGONBLEND
    }
    void DrawPolygonScanLine(int x1, int y1, int x2, int y2) {
        int xStart = x1 >> 16;
        int xEnd = x2 >> 16;
        int yStart = y1 >> 16;
        int yEnd = y2 >> 16;
        if (yStart == yEnd)
            return;

        // swap
        if (yStart > yEnd) {
            xStart = x2 >> 16;
            xEnd = x1 >> 16;
            yStart = y2 >> 16;
            yEnd = y1 >> 16;
        }

        int minX = INT_MIN, minY = INT_MIN, maxX = INT_MAX, maxY = INT_MAX;
        GET_CLIP_BOUNDS(minX, minY, maxX, maxY);

        int yEndBound = yEnd + 1;
        if (yEndBound < minY || yStart >= maxY)
            return;

        if (yEndBound > maxY)
            yEndBound = maxY;

        int linePointSubpxX = xStart << 16;
        int dx = ((xEnd - xStart) << 16) / (yEnd - yStart);

        if (yStart < 0) {
            linePointSubpxX -= yStart * dx;
            yStart = 0;
        }

        Contour* contour = &ContourField[yStart];
        if (yStart < yEndBound) {
            int lineHeight = yEndBound - yStart;
            while (lineHeight--) {
                int linePointX = linePointSubpxX >> 16;

                if (linePointX <= minX)
                    contour->MinX = minX;
                else if (linePointX >= maxX)
                    contour->MaxX = maxX;
                else {
                    if (linePointX < contour->MinX)
                        contour->MinX = linePointX;
                    if (linePointX > contour->MaxX)
                        contour->MaxX = linePointX;
                }

                linePointSubpxX += dx;
                contour++;
            }
        }
    }
    void DrawPolygon(Vector2* positions, Color color, int vertexCount, int blendFlag) {
        Vector2* tempVertex;
        int      tempCount;
        if (vertexCount == 0)
            return;

        Pixel*  dstPx = (Pixel*)CurrentView->Pixels;
        Uint32  dstStride = CurrentView->Pitch;

        int minVal = INT_MAX;
        int maxVal = INT_MIN;

        int viewX = 0;
        int viewY = 0;
        if (!DrawToScreen) {
            viewX = CurrentView->X;
            viewY = CurrentView->Y;
        }

        int tempY;
        tempVertex = positions;
        tempCount = vertexCount;
        while (tempCount--) {
            tempY = (tempVertex->Y >> 16) - viewY;
            if (minVal > tempY)
                minVal = tempY;
            if (maxVal < tempY)
                maxVal = tempY;
            tempVertex++;
        }

        int dst_x1 = 0, dst_x2 = 0; // These are unused.
        int dst_y1 = minVal;
        int dst_y2 = maxVal;

        GET_CLIP_BOUNDS(dst_x1, dst_y1, dst_x2, dst_y2);
        if (dst_y1 >= dst_y2)
            return;

        DidDraw = true;

        int scanLineCount = dst_y2 - dst_y1;
        Contour* contourPtr = &ContourField[dst_y1];
        while (scanLineCount--) {
            contourPtr->MinX = INT_MAX;
            contourPtr->MaxX = INT_MIN;
            contourPtr++;
        }

        // Offset vertex positions
        tempVertex = positions;
        tempCount = vertexCount;
        while (tempCount--) {
            tempVertex->X.Whole -= viewX;
            tempVertex->Y.Whole -= viewY;
            tempVertex++;
        }

        // Get scanlines
        Vector2* lastVector = positions;
        if (vertexCount > 1) {
            int countRem = vertexCount - 1;
            while (countRem--) {
                DrawPolygonScanLine(lastVector[0].X, lastVector[0].Y, lastVector[1].X, lastVector[1].Y);
                lastVector++;
            }
        }
        DrawPolygonScanLine(lastVector[0].X, lastVector[0].Y, positions[0].X, positions[0].Y);

        // Un-offset vertex positions
        tempVertex = positions;
        tempCount = vertexCount;
        while (tempCount--) {
            tempVertex->X.Whole += viewX;
            tempVertex->Y.Whole += viewY;
            tempVertex++;
        }

        #define DRAW_POLYGON(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
            Contour contour = ContourField[dst_y]; \
            if (contour.MaxX < contour.MinX) { \
                dst_strideY += dstStride; \
                continue; \
            } \
            for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
                pixelFunction(&pxCol, &dstPx[dst_x + dst_strideY], multPosTableAt, multNegTableAt, multInvTableAt); \
            } \
            dst_strideY += dstStride; \
        }

        Pixel pxCol = color;
        int   opacity = color.A;

        Uint8* multPosTableAt = &MultTablePos[opacity << COLOR_BITS];
        Uint8* multNegTableAt = &MultTableNeg[opacity << COLOR_BITS];
        Uint8* multInvTableAt = &MultTableInv[opacity << COLOR_BITS];
        int    dst_strideY = dst_y1 * dstStride;
        switch (blendFlag) {
        case BLEND_NONE:
            for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
                Contour contour = ContourField[dst_y];
                if (contour.MaxX < contour.MinX) {
                    dst_strideY += dstStride;
                    continue;
                }

                memset16(&dstPx[contour.MinX + dst_strideY], pxCol, contour.MaxX - contour.MinX);
                dst_strideY += dstStride;
            }
            // DRAW_POLYGON(PixelSetOpaque);
            break;
        case BLEND_TRANSPARENT:
            DRAW_POLYGON(PixelSetTransparent);
            break;
        case BLEND_ADDITIVE:
            DRAW_POLYGON(PixelSetAdditive);
            break;
        case BLEND_SUBTRACT:
            DRAW_POLYGON(PixelSetSubtract);
            break;
        case BLEND_MATCH:
            DRAW_POLYGON(PixelSetMatchEqual);
            break;
        case BLEND_NON_MATCH:
            DRAW_POLYGON(PixelSetMatchNotEqual);
            break;
        case BLEND_FILTERED:
            DRAW_POLYGON(PixelSetFiltered);
            break;
        }

        #undef DRAW_POLYGON
    }
    void FadeScreen(Color color, int rMult, int gMult, int bMult) {
        rMult = M_CLAMP(rMult, 0x00, 0xFF);
        gMult = M_CLAMP(gMult, 0x00, 0xFF);
        bMult = M_CLAMP(bMult, 0x00, 0xFF);

        if (rMult + gMult + bMult == 0)
            return;

        Pixel* dstPx = (Pixel*)CurrentView->Pixels;
        Uint32  dstStride = CurrentView->Pitch;

        int dst_x1 = 0;
        int dst_y1 = 0;
        int dst_x2 = CurrentView->Width;
        int dst_y2 = CurrentView->Height;

        GET_CLIP_BOUNDS(dst_x1, dst_y1, dst_x2, dst_y2);
        if (dst_x1 >= dst_x2 || dst_y1 >= dst_y2)
            return;

        DidDraw = true;

        Pixel pxCol = color;

        Uint8* multPosTableAtR = &MultTablePos[rMult << COLOR_BITS];
        Uint8* multNegTableAtR = &MultTableNeg[rMult << COLOR_BITS];
        Uint8* multInvTableAtR = &MultTableInv[rMult << COLOR_BITS];
        Uint8* multPosTableAtG = &MultTablePos[gMult << COLOR_BITS];
        Uint8* multNegTableAtG = &MultTableNeg[gMult << COLOR_BITS];
        Uint8* multInvTableAtG = &MultTableInv[gMult << COLOR_BITS];
        Uint8* multPosTableAtB = &MultTablePos[bMult << COLOR_BITS];
        Uint8* multNegTableAtB = &MultTableNeg[bMult << COLOR_BITS];
        Uint8* multInvTableAtB = &MultTableInv[bMult << COLOR_BITS];

        int    dst_strideY = dst_y1 * dstStride;
        const int stride = (dst_x2 - dst_x1);
        for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
            for (int dst_x = dst_x1; dst_x < dst_x2; dst_x++) {
                Pixel* srcP = &pxCol;
                Pixel* dstP = &dstPx[dst_x + dst_strideY];
                *dstP = Pixel(
                    multPosTableAtR[srcP->R] + multInvTableAtR[dstP->R],
                    multPosTableAtG[srcP->G] + multInvTableAtG[dstP->G],
                    multPosTableAtB[srcP->B] + multInvTableAtB[dstP->B]);
            }
            dst_strideY += dstStride;
        }
    }

    // Drawing 3D
    void View3D_SetAmbientLighting(Resource view3dIndex, Uint32 r, Uint32 g, Uint32 b) {
        if (view3dIndex < 0 || view3dIndex >= MAX_ARRAY_BUFFERS)
            return;

        ArrayBuffer* arrayBuffer = &Resources::ResourceView3Ds[view3dIndex].View3DData;
        arrayBuffer->LightingAmbientR = r;
        arrayBuffer->LightingAmbientG = g;
        arrayBuffer->LightingAmbientB = b;
    }
    void View3D_SetDiffuseLighting(Resource view3dIndex, Uint32 r, Uint32 g, Uint32 b) {
        if (view3dIndex < 0 || view3dIndex >= MAX_ARRAY_BUFFERS)
            return;

        ArrayBuffer* arrayBuffer = &Resources::ResourceView3Ds[view3dIndex].View3DData;
        arrayBuffer->LightingDiffuseR = r;
        arrayBuffer->LightingDiffuseG = g;
        arrayBuffer->LightingDiffuseB = b;
    }
    void View3D_SetSpecularLighting(Resource view3dIndex, Uint32 r, Uint32 g, Uint32 b) {
        if (view3dIndex < 0 || view3dIndex >= MAX_ARRAY_BUFFERS)
            return;

        ArrayBuffer* arrayBuffer = &Resources::ResourceView3Ds[view3dIndex].View3DData;
        arrayBuffer->LightingSpecularR = r;
        arrayBuffer->LightingSpecularG = g;
        arrayBuffer->LightingSpecularB = b;
    }
    void View3D_DrawBegin(Resource view3dIndex) {
        if (view3dIndex < 0 || view3dIndex >= MAX_ARRAY_BUFFERS)
            return;

        ArrayBuffer* arrayBuffer = &Resources::ResourceView3Ds[view3dIndex].View3DData;
        arrayBuffer->VertexCount = 0;
        arrayBuffer->FaceCount = 0;

        // arrayBuffer->LightingAmbientR = 160;
        // arrayBuffer->LightingAmbientG = 160;
        // arrayBuffer->LightingAmbientB = 160;
        // arrayBuffer->LightingDiffuseR = 8;
        // arrayBuffer->LightingDiffuseG = 8;
        // arrayBuffer->LightingDiffuseB = 8;
        // arrayBuffer->LightingSpecularR = 14;
        // arrayBuffer->LightingSpecularG = 14;
        // arrayBuffer->LightingSpecularB = 14;
    }
    void View3D_DrawFinish(Resource view3dIndex, Uint32 drawMode) {
        if (view3dIndex < 0 || view3dIndex >= MAX_ARRAY_BUFFERS)
            return;

        int facesRemaining;
        Uint8* faceSizePtr;
        FaceInfo* faceInfoPtr;
        ArrayBuffer* arrayBuffer;
        Uint32 bitshiftX, bitshiftY;
        VertexAttribute* vertexAttribsPtr;
        FaceInfo temp, * faceInfoPtrA, * faceInfoPtrB, * faceInfoPtrTop;

        arrayBuffer = &Resources::ResourceView3Ds[view3dIndex].View3DData;

        vertexAttribsPtr = arrayBuffer->VertexBuffer;
        faceInfoPtr = arrayBuffer->FaceInfoBuffer;
        faceSizePtr = arrayBuffer->FaceSizeBuffer;
        bitshiftX = arrayBuffer->PerspectiveBitshiftX;
        bitshiftY = arrayBuffer->PerspectiveBitshiftY;

        // Get the face depth and vertices' start index
        int verticesStartIndex = 0;
        for (int f = 0; f < arrayBuffer->FaceCount; f++) {
            switch (*faceSizePtr) {
            case 2:
                // (50%, 50%)
                // faceInfoPtr->Depth  = vertexAttribsPtr[0].Position.Z >> 1;
                // faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z >> 1;
                faceInfoPtr->Depth = vertexAttribsPtr[0].Position.Z;
                faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z;
                faceInfoPtr->Depth /= 2;
                vertexAttribsPtr += 2;
                break;
            case 3:
                // (25%, 25%, 50%)
                // faceInfoPtr->Depth  = vertexAttribsPtr[0].Position.Z >> 2;
                // faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z >> 2;
                // faceInfoPtr->Depth += vertexAttribsPtr[2].Position.Z >> 1;
                faceInfoPtr->Depth = vertexAttribsPtr[0].Position.Z;
                faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z;
                faceInfoPtr->Depth += vertexAttribsPtr[2].Position.Z;
                faceInfoPtr->Depth /= 3;
                vertexAttribsPtr += 3;
                break;
            case 4:
                // (25%, 25%, 25%, 25%)
                // faceInfoPtr->Depth  = vertexAttribsPtr[0].Position.Z >> 2;
                // faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z >> 2;
                // faceInfoPtr->Depth += vertexAttribsPtr[2].Position.Z >> 2;
                // faceInfoPtr->Depth += vertexAttribsPtr[3].Position.Z >> 2;
                faceInfoPtr->Depth = vertexAttribsPtr[0].Position.Z;
                faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z;
                faceInfoPtr->Depth += vertexAttribsPtr[2].Position.Z;
                faceInfoPtr->Depth += vertexAttribsPtr[3].Position.Z;
                faceInfoPtr->Depth /= 4;
                vertexAttribsPtr += 4;
                break;
            default:
                faceInfoPtr->Depth = vertexAttribsPtr[0].Position.Z;
                vertexAttribsPtr += *faceSizePtr;
                break;
            }

            faceInfoPtr->VerticesStartIndex = verticesStartIndex;
            verticesStartIndex += *faceSizePtr;

            faceInfoPtr++;
            faceSizePtr++;
        }

        // Sort face infos by depth
        if (arrayBuffer->FaceCount > 1) {
            facesRemaining = arrayBuffer->FaceCount - 1;
            faceInfoPtrTop = arrayBuffer->FaceInfoBuffer;
            faceInfoPtrA = faceInfoPtrTop + 1;
            while (facesRemaining--) {
                temp = *faceInfoPtrA;
                faceInfoPtrB = faceInfoPtrA - 1;
                while (faceInfoPtrB >= faceInfoPtrTop && faceInfoPtrB->Depth < temp.Depth) {
                    faceInfoPtrB[1] = faceInfoPtrB[0];
                    faceInfoPtrB--;
                }
                faceInfoPtrB[1] = temp;
                faceInfoPtrA++;
            }
        }

        #define CLAMP_VAL(v, a, b) if (v < a) v = a; else if (v > b) v = b;

        // int cxL = -CurrentView->X;
        // int cyL = -CurrentView->Y;
        // int cxP = -CurrentView->X << 16;
        // int cyP = -CurrentView->Y << 16;
        int cxP = 0;
        int cyP = 0;

        bool drawToScreenCopy = DrawToScreen;
        
        DrawToScreen = false;
        if (drawMode & V3D_PERSPECTIVE)
            DrawToScreen = true;

        // sas
        VertexAttribute* vertex, * vertexFirst;
        faceInfoPtr = arrayBuffer->FaceInfoBuffer;
        faceSizePtr = arrayBuffer->FaceSizeBuffer;

        int blendFlag = BLEND_NONE;

        int widthHalfSubpx = (int)CurrentView->WidthHalf << 16;
        int heightHalfSubpx = (int)CurrentView->HeightHalf << 16;

        switch (drawMode & 7) {
            // Lines, Solid Colored
            case V3D_LINES | V3D_SOLID:
                for (int f = 0; f < arrayBuffer->FaceCount; f++) {
                    int vertexCountPerFaceMinus1 = *faceSizePtr - 1;
                    vertexFirst = &arrayBuffer->VertexBuffer[faceInfoPtr->VerticesStartIndex];
                    vertex = vertexFirst;

                    if (drawMode & V3D_PERSPECTIVE) {
                        widthHalfSubpx >>= 16;
                        heightHalfSubpx >>= 16;
                        #define FIX_X(x) (widthHalfSubpx + ((x << bitshiftX) / vertexZ) - cxP)
                        #define FIX_Y(y) (heightHalfSubpx - ((y << bitshiftY) / vertexZ) - cyP)
                        while (vertexCountPerFaceMinus1--) {
                            int vertexZ = vertex->Position.Z;
                            if (vertexZ < 0x100)
                                goto mrt_line_solid_NEXT_FACE;

                            DrawLine(FIX_X(vertex[0].Position.X), FIX_Y(vertex[0].Position.Y), FIX_X(vertex[1].Position.X), FIX_Y(vertex[1].Position.Y), vertex->Color, blendFlag);
                            vertex++;
                        }
                        int vertexZ = vertex->Position.Z;
                        if (vertexZ < 0x100)
                            goto mrt_line_solid_NEXT_FACE;

                        DrawLine(FIX_X(vertex->Position.X), FIX_Y(vertex->Position.Y), FIX_X(vertexFirst->Position.X), FIX_Y(vertexFirst->Position.Y), vertex->Color, blendFlag);
                        #undef FIX_X
                        #undef FIX_Y
                    }
                    else {
                        while (vertexCountPerFaceMinus1--) {
                            DrawLine(vertex[0].Position.X << 8, vertex[0].Position.Y << 8, vertex[1].Position.X << 8, vertex[1].Position.Y << 8, vertex->Color, blendFlag);
                            vertex++;
                        }
                        DrawLine(vertex->Position.X << 8, vertex->Position.Y << 8, vertexFirst->Position.X << 8, vertexFirst->Position.Y << 8, vertex->Color, blendFlag);
                    }

                mrt_line_solid_NEXT_FACE:
                    faceSizePtr++;
                    faceInfoPtr++;
                }
                break;
            // Lines, Flat Shading
            case V3D_LINES | V3D_FLAT:
            // Lines, Smooth Shading
            case V3D_LINES | V3D_SMOOTH:
                for (int f = 0; f < arrayBuffer->FaceCount; f++) {
                    int vertexCountPerFaceMinus1 = *faceSizePtr - 1;
                    vertexFirst = &arrayBuffer->VertexBuffer[faceInfoPtr->VerticesStartIndex];
                    vertex = vertexFirst;

                    int averageNormalY = vertex[0].Normal.Y;
                    switch (*faceSizePtr) {
                    case 2:
                        averageNormalY += vertex[1].Normal.Y;
                        break;
                    case 3:
                        averageNormalY += vertex[1].Normal.Y + vertex[2].Normal.Y;
                        break;
                    case 4:
                        averageNormalY += vertex[1].Normal.Y + vertex[2].Normal.Y + vertex[3].Normal.Y;
                        break;
                    }
                    averageNormalY /= *faceSizePtr;

                    Color color = vertex->Color;
                    int col_r = color.R;
                    int col_g = color.G;
                    int col_b = color.B;
                    int specularR = 0, specularG = 0, specularB = 0;

                    int ambientNormalY = averageNormalY >> 10;
                    int reweightedNormal = (averageNormalY >> 2) * (M_ABS(averageNormalY) >> 2);

                    // r
                    col_r = (col_r * (ambientNormalY + arrayBuffer->LightingAmbientR)) >> arrayBuffer->LightingDiffuseR;
                    specularR = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularR;
                    CLAMP_VAL(specularR, 0x00, 0xFF);
                    specularR += col_r;
                    CLAMP_VAL(specularR, 0x00, 0xFF);
                    col_r = specularR;

                    // g
                    col_g = (col_g * (ambientNormalY + arrayBuffer->LightingAmbientG)) >> arrayBuffer->LightingDiffuseG;
                    specularG = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularG;
                    CLAMP_VAL(specularG, 0x00, 0xFF);
                    specularG += col_g;
                    CLAMP_VAL(specularG, 0x00, 0xFF);
                    col_g = specularG;

                    // b
                    col_b = (col_b * (ambientNormalY + arrayBuffer->LightingAmbientB)) >> arrayBuffer->LightingDiffuseB;
                    specularB = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularB;
                    CLAMP_VAL(specularB, 0x00, 0xFF);
                    specularB += col_b;
                    CLAMP_VAL(specularB, 0x00, 0xFF);
                    col_b = specularB;

                    // color
                    color = Color(col_r, col_g, col_b);

                    if (drawMode & V3D_PERSPECTIVE) {
                        widthHalfSubpx >>= 16;
                        heightHalfSubpx >>= 16;
                        #define FIX_X(x) (widthHalfSubpx + ((x << bitshiftX) / vertexZ) - cxP)
                        #define FIX_Y(y) (heightHalfSubpx - ((y << bitshiftY) / vertexZ) - cyP)
                        while (vertexCountPerFaceMinus1--) {
                            int vertexZ = vertex->Position.Z;
                            if (vertexZ < 0x100)
                                goto mrt_line_smooth_NEXT_FACE;

                            DrawLine(FIX_X(vertex[0].Position.X), FIX_Y(vertex[0].Position.Y), FIX_X(vertex[1].Position.X), FIX_Y(vertex[1].Position.Y), color, blendFlag);
                            vertex++;
                        }
                        int vertexZ = vertex->Position.Z;
                        if (vertexZ < 0x100)
                            goto mrt_line_smooth_NEXT_FACE;

                        DrawLine(FIX_X(vertex->Position.X), FIX_Y(vertex->Position.Y), FIX_X(vertexFirst->Position.X), FIX_Y(vertexFirst->Position.Y), color, blendFlag);
                        #undef FIX_X
                        #undef FIX_Y
                    }
                    else {
                        while (vertexCountPerFaceMinus1--) {
                            DrawLine(vertex[0].Position.X << 8, vertex[0].Position.Y << 8, vertex[1].Position.X << 8, vertex[1].Position.Y << 8, color, blendFlag);
                            vertex++;
                        }
                        DrawLine(vertex->Position.X << 8, vertex->Position.Y << 8, vertexFirst->Position.X << 8, vertexFirst->Position.Y << 8, color, blendFlag);
                    }

                mrt_line_smooth_NEXT_FACE:
                    faceSizePtr++;
                    faceInfoPtr++;
                }
                break;
            // Polygons, Solid Colored
            case V3D_POLYGONS | V3D_SOLID:
                for (int f = 0; f < arrayBuffer->FaceCount; f++) {
                    int vertexCountPerFace = *faceSizePtr;
                    vertexFirst = &arrayBuffer->VertexBuffer[faceInfoPtr->VerticesStartIndex];
                    vertex = vertexFirst;

                    Vector2 polygonVertex[4];
                    int polygonVertexIndex = 0;
                    while (vertexCountPerFace--) {
                        if (drawMode & V3D_PERSPECTIVE) {
                            int vertexZ = vertex->Position.Z;
                            if (vertexZ < 0x100)
                                goto mrt_poly_solid_NEXT_FACE;

                            polygonVertex[polygonVertexIndex].X = widthHalfSubpx + ((vertex->Position.X << bitshiftX) / vertexZ << 16) - cxP;
                            polygonVertex[polygonVertexIndex].Y = heightHalfSubpx - ((vertex->Position.Y << bitshiftY) / vertexZ << 16) - cyP;
                        }
                        else {
                            polygonVertex[polygonVertexIndex].X = (vertex->Position.X << 8) - cxP;
                            polygonVertex[polygonVertexIndex].Y = (vertex->Position.Y << 8) - cyP;
                        }
                        polygonVertexIndex++;
                        vertex++;
                    }
                    DrawPolygon(polygonVertex, vertexFirst->Color, *faceSizePtr, blendFlag);

                mrt_poly_solid_NEXT_FACE:
                    faceSizePtr++;
                    faceInfoPtr++;
                }
                break;
            // Polygons, Flat Shading
            case V3D_POLYGONS | V3D_FLAT:
                for (int f = 0; f < arrayBuffer->FaceCount; f++) {
                    int vertexCountPerFace = *faceSizePtr;
                    vertexFirst = &arrayBuffer->VertexBuffer[faceInfoPtr->VerticesStartIndex];
                    vertex = vertexFirst;

                    int averageNormalY = vertex[0].Normal.Y;
                    switch (*faceSizePtr) {
                    case 2:
                        averageNormalY += vertex[1].Normal.Y;
                        break;
                    case 3:
                        averageNormalY += vertex[1].Normal.Y + vertex[2].Normal.Y;
                        break;
                    case 4:
                        averageNormalY += vertex[1].Normal.Y + vertex[2].Normal.Y + vertex[3].Normal.Y;
                        break;
                    }
                    averageNormalY /= *faceSizePtr;

                    Color color = vertex->Color;
                    int specularR = 0, specularG = 0, specularB = 0;

                    int ambientNormalY = averageNormalY >> 10;
                    int reweightedNormal = (averageNormalY >> 2) * (M_ABS(averageNormalY) >> 2);

                    // r
                    color.R = (color.R * (ambientNormalY + arrayBuffer->LightingAmbientR)) >> arrayBuffer->LightingDiffuseR;
                    specularR = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularR;
                    CLAMP_VAL(specularR, 0x00, 0xFF);
                    specularR += color.R;
                    CLAMP_VAL(specularR, 0x00, 0xFF);
                    color.R = specularR;

                    // g
                    color.G = (color.G * (ambientNormalY + arrayBuffer->LightingAmbientG)) >> arrayBuffer->LightingDiffuseG;
                    specularG = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularG;
                    CLAMP_VAL(specularG, 0x00, 0xFF);
                    specularG += color.G;
                    CLAMP_VAL(specularG, 0x00, 0xFF);
                    color.G = specularG;

                    // b
                    color.B = (color.B * (ambientNormalY + arrayBuffer->LightingAmbientB)) >> arrayBuffer->LightingDiffuseB;
                    specularB = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularB;
                    CLAMP_VAL(specularB, 0x00, 0xFF);
                    specularB += color.B;
                    CLAMP_VAL(specularB, 0x00, 0xFF);
                    color.B = specularB;

                    Vector2 polygonVertex[4];
                    int polygonVertexIndex = 0;
                    while (vertexCountPerFace--) {
                        if (drawMode & V3D_PERSPECTIVE) {
                            int vertexZ = vertex->Position.Z;
                            if (vertexZ < 0x100)
                                goto mrt_poly_flat_NEXT_FACE;

                            polygonVertex[polygonVertexIndex].X = widthHalfSubpx + ((vertex->Position.X << bitshiftX) / vertexZ << 16) - cxP;
                            polygonVertex[polygonVertexIndex].Y = heightHalfSubpx - ((vertex->Position.Y << bitshiftY) / vertexZ << 16) - cyP;
                        }
                        else {
                            polygonVertex[polygonVertexIndex].X = (vertex->Position.X << 8) - cxP;
                            polygonVertex[polygonVertexIndex].Y = (vertex->Position.Y << 8) - cyP;
                        }
                        polygonVertexIndex++;
                        vertex++;
                    }
                    DrawPolygon(polygonVertex, color, *faceSizePtr, blendFlag);

                mrt_poly_flat_NEXT_FACE:
                    faceSizePtr++;
                    faceInfoPtr++;
                }
                break;
            // Polygons, Smooth Shading
            case V3D_POLYGONS | V3D_SMOOTH:
                for (int f = 0; f < arrayBuffer->FaceCount; f++) {
                    int vertexCountPerFace = *faceSizePtr;
                    vertexFirst = &arrayBuffer->VertexBuffer[faceInfoPtr->VerticesStartIndex];
                    vertex = vertexFirst;

                    Vector2 polygonVertex[4];
                    Color   polygonVertColor[4];
                    int     polygonVertexIndex = 0;
                    while (vertexCountPerFace--) {
                        if (drawMode & V3D_PERSPECTIVE) {
                            int vertexZ = vertex->Position.Z;
                            if (vertexZ < 0x100)
                                goto mrt_poly_smooth_NEXT_FACE;

                            polygonVertex[polygonVertexIndex].X = widthHalfSubpx + ((vertex->Position.X << bitshiftX) / vertexZ << 16) - cxP;
                            polygonVertex[polygonVertexIndex].Y = heightHalfSubpx - ((vertex->Position.Y << bitshiftY) / vertexZ << 16) - cyP;
                        }
                        else {
                            polygonVertex[polygonVertexIndex].X = (vertex->Position.X << 8) - cxP;
                            polygonVertex[polygonVertexIndex].Y = (vertex->Position.Y << 8) - cyP;
                        }

                        Color color = vertex->Color;
                        int specularR = 0, specularG = 0, specularB = 0;
                        int averageNormalY = vertex->Normal.Y;

                        int ambientNormalY = averageNormalY >> 10;
                        int reweightedNormal = (averageNormalY >> 2) * (M_ABS(averageNormalY) >> 2);

                        // r
                        color.R = (color.R * (ambientNormalY + arrayBuffer->LightingAmbientR)) >> arrayBuffer->LightingDiffuseR;
                        specularR = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularR;
                        CLAMP_VAL(specularR, 0x00, 0xFF);
                        specularR += color.R;
                        CLAMP_VAL(specularR, 0x00, 0xFF);
                        color.R = specularR;

                        // g
                        color.G = (color.G * (ambientNormalY + arrayBuffer->LightingAmbientG)) >> arrayBuffer->LightingDiffuseG;
                        specularG = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularG;
                        CLAMP_VAL(specularG, 0x00, 0xFF);
                        specularG += color.G;
                        CLAMP_VAL(specularG, 0x00, 0xFF);
                        color.G = specularG;

                        // b
                        color.B = (color.B * (ambientNormalY + arrayBuffer->LightingAmbientB)) >> arrayBuffer->LightingDiffuseB;
                        specularB = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularB;
                        CLAMP_VAL(specularB, 0x00, 0xFF);
                        specularB += color.B;
                        CLAMP_VAL(specularB, 0x00, 0xFF);
                        color.B = specularB;

                        polygonVertColor[polygonVertexIndex] = color;
                        polygonVertexIndex++;
                        vertex++;
                    }
                    DrawPolygonBlend(polygonVertex, polygonVertColor, *faceSizePtr, blendFlag);

                mrt_poly_smooth_NEXT_FACE:
                    faceSizePtr++;
                    faceInfoPtr++;
                }
                break;
        }

        DrawToScreen = drawToScreenCopy;
    }
    void View3D_DrawModel(Resource view3dIndex, Resource meshIndex, int frame, Matrix4x4* viewMatrix, Matrix4x4* normalMatrix, Color color) {
        if (view3dIndex < 0 || view3dIndex >= MAX_ARRAY_BUFFERS)
            return;
        if (meshIndex < 0 || meshIndex >= MAX_MESHES)
            return;

        Mesh* model = &Resources::ResourceMeshes[meshIndex].MeshData;

        VertexAttribute* arrayVertexItem;
        int arrayVertexCount, arrayFaceCount, modelVertexIndexCount;
        Uint8* faceSizeItem;
        Sint16* modelVertexIndexPtr;
        Vector3* positionPtr;
        Color* colorPtr;
        Vector2* uvPtr;

        int vertexTypeMask = VertexType_Position | VertexType_Normal | VertexType_Color;

        #define APPLY_MAT4X4(vec3out, vec3in, M) \
            vec3out.X = ((vec3in.X * M->Column[0][0]) >> 8) + ((vec3in.Y * M->Column[0][1]) >> 8) + ((vec3in.Z * M->Column[0][2]) >> 8) + M->Column[0][3]; \
            vec3out.Y = ((vec3in.X * M->Column[1][0]) >> 8) + ((vec3in.Y * M->Column[1][1]) >> 8) + ((vec3in.Z * M->Column[1][2]) >> 8) + M->Column[1][3]; \
            vec3out.Z = ((vec3in.X * M->Column[2][0]) >> 8) + ((vec3in.Y * M->Column[2][1]) >> 8) + ((vec3in.Z * M->Column[2][2]) >> 8) + M->Column[2][3];

        while (frame >= model->FrameCount)
            frame -= model->FrameCount;
        frame *= model->VertexCount;

        if (viewMatrix) {
            ArrayBuffer* arrayBuffer = &Resources::ResourceView3Ds[view3dIndex].View3DData;

            arrayFaceCount = arrayBuffer->FaceCount;
            arrayVertexCount = arrayBuffer->VertexCount;

            faceSizeItem = &arrayBuffer->FaceSizeBuffer[arrayFaceCount];
            arrayVertexItem = &arrayBuffer->VertexBuffer[arrayVertexCount];

            modelVertexIndexCount = model->VertexIndexCount;
            modelVertexIndexPtr = model->VertexIndices;

            if (arrayVertexCount + modelVertexIndexCount <= arrayBuffer->VertexCapacity) {
                arrayBuffer->VertexCount += modelVertexIndexCount;
                arrayBuffer->FaceCount += modelVertexIndexCount / model->FaceVertexCount;

                switch (model->VertexType & vertexTypeMask) {
                case VertexType_Position:
                    // For every face,
                    while (*modelVertexIndexPtr != -1) {
                        int faceVertexCount = model->FaceVertexCount;
                        *faceSizeItem++ = faceVertexCount;

                        // For every vertex index,
                        while (faceVertexCount--) {
                            positionPtr = &model->Positions[(*modelVertexIndexPtr + frame)];
                            APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                            modelVertexIndexPtr++;
                            arrayVertexItem++;
                        }
                    }
                    break;
                case VertexType_Position | VertexType_Normal:
                    if (normalMatrix) {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->Positions[(*modelVertexIndexPtr + frame) << 1];
                                // Calculate position
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                // Calculate normals
                                APPLY_MAT4X4(arrayVertexItem->Normal, positionPtr[1], normalMatrix);
                                arrayVertexItem->Color = color;

                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    else {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->Positions[(*modelVertexIndexPtr + frame) << 1];
                                // Calculate position
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                arrayVertexItem->Normal = positionPtr[1];
                                arrayVertexItem->Color = color;
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    break;
                case VertexType_Position | VertexType_Normal | VertexType_Color:
                    if (normalMatrix) {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->Positions[(*modelVertexIndexPtr + frame) << 1];
                                colorPtr = &model->Colors[*modelVertexIndexPtr];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                APPLY_MAT4X4(arrayVertexItem->Normal, positionPtr[1], normalMatrix);
                                arrayVertexItem->Color = colorPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    else {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->Positions[(*modelVertexIndexPtr + frame) << 1];
                                colorPtr = &model->Colors[*modelVertexIndexPtr];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                arrayVertexItem->Normal = positionPtr[1];
                                arrayVertexItem->Color = colorPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    break;
                case VertexType_Position | VertexType_Normal | VertexType_UV:
                    if (normalMatrix) {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->Positions[(*modelVertexIndexPtr + frame) << 1];
                                uvPtr = &model->UVs[(*modelVertexIndexPtr + frame)];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                APPLY_MAT4X4(arrayVertexItem->Normal, positionPtr[1], normalMatrix);
                                arrayVertexItem->Color = color;
                                arrayVertexItem->UV = uvPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    else {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->Positions[(*modelVertexIndexPtr + frame) << 1];
                                uvPtr = &model->UVs[(*modelVertexIndexPtr + frame)];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                arrayVertexItem->Normal = positionPtr[1];
                                arrayVertexItem->Color = color;
                                arrayVertexItem->UV = uvPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    break;
                case VertexType_Position | VertexType_Normal | VertexType_UV | VertexType_Color:
                    if (normalMatrix) {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->Positions[(*modelVertexIndexPtr + frame) << 1];
                                uvPtr = &model->UVs[(*modelVertexIndexPtr + frame)];
                                colorPtr = &model->Colors[*modelVertexIndexPtr];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                APPLY_MAT4X4(arrayVertexItem->Normal, positionPtr[1], normalMatrix);
                                arrayVertexItem->Color = colorPtr[0];
                                arrayVertexItem->UV = uvPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    else {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->Positions[(*modelVertexIndexPtr + frame) << 1];
                                uvPtr = &model->UVs[(*modelVertexIndexPtr + frame)];
                                colorPtr = &model->Colors[*modelVertexIndexPtr];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                arrayVertexItem->Normal = positionPtr[1];
                                arrayVertexItem->Color = colorPtr[0];
                                arrayVertexItem->UV = uvPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    break;
                }
            }
            else {
                Diagnostics::SetError("Model has too many vertices (%d) to fit in size (%d) of array buffer! Increase array buffer size by %d, or use a model with less vertices!", modelVertexIndexCount, arrayBuffer->VertexCapacity, (arrayVertexCount + modelVertexIndexCount) - arrayBuffer->VertexCapacity);
            }
        }

        #undef APPLY_MAT4X4
    }

    // Drawing Layers
    void LayerInitScanLines(Layer* layer) {
        switch (layer->DrawBehavior) {
            case 3:
    		case Scene::DRAW_HORIZONTAL: {
                int viewX = CurrentView->X + layer->CameraOffsetX.Whole;
                int viewY = CurrentView->Y + layer->CameraOffsetY.Whole;
                int viewHeight = CurrentView->Height;
                int layerWidth = layer->Width << TILE_SIZE_IN_BITS;
                int layerHeight = layer->Height << TILE_SIZE_IN_BITS;

                #ifdef ENABLE_STEREOSCOPIC_VIEW
                int stereoscopicSplit = (int)(StereoscopicSplit * 0x20);
                #endif

                // Set parallax positions
                Parallax* info = &layer->ParallaxInfos[0];
                #ifdef ENABLE_STEREOSCOPIC_VIEW
                if (stereoscopicSplit) {
                    for (int i = 0; i < layer->ParallaxInfoCount; i++) {
                        int stereoscopicDistance = ((0x10000 - info->RelativeParallax.Full) * stereoscopicSplit) >> 4;
                        if (CurrentViewIndex == 0)
                            info->ParallaxPosition.Full = info->ParallaxOffset.Full + (viewX * info->RelativeParallax.Full) - stereoscopicDistance;
                        else
                            info->ParallaxPosition.Full = info->ParallaxOffset.Full + (viewX * info->RelativeParallax.Full) + stereoscopicDistance;

                        info->ParallaxPosition.Whole %= layerWidth;
                        if (info->ParallaxPosition.Whole < 0)
                            info->ParallaxPosition.Whole += layerWidth;
                        info++;
                    }
                }
                else
                #endif
                {
                    for (int i = 0; i < layer->ParallaxInfoCount; i++) {
                        info->ParallaxPosition.Full = info->ParallaxOffset.Full + (viewX * info->RelativeParallax.Full);
                        info->ParallaxPosition.Whole %= layerWidth;
                        if (info->ParallaxPosition.Whole < 0)
                            info->ParallaxPosition.Whole += layerWidth;
                        info++;
                    }
                }

                // Create scan lines
                int scrollLine = (layer->ScrollOffset.Full + (viewY * layer->RelativeScroll.Full)) >> 16;
                    scrollLine %= layerHeight;
                if (scrollLine < 0)
                    scrollLine += layerHeight;

                int* deformValues;
                Uint8* parallaxIndex;
                ScanLine* scanLine;
                const int maxDeformLineMask = (MAX_DEFORM_LINES >> 1) - 1;

                scanLine = &ScanLineBuffer[0];
                parallaxIndex = &layer->ParallaxIndexLines[scrollLine];
                deformValues = &layer->DeformSetA[(scrollLine + layer->DeformOffsetA) & maxDeformLineMask];
                for (int i = 0; i < CurrentView->DeformSplitLine; i++) {
                    // Set scan line start positions
                    info = &layer->ParallaxInfos[*parallaxIndex];
                    scanLine->SourceX = info->ParallaxPosition;
                    if (info->CanDeform)
                        scanLine->SourceX.Whole += *deformValues;
                    scanLine->SourceY = Subpixels(scrollLine, 0x0000);

                    scanLine->DeltaX = Subpixels(0x1, 0x0000);
                    scanLine->DeltaY = Subpixels(0x0, 0x0000);

                    // Iterate lines
                    scanLine++;
                    scrollLine++;
                    deformValues++;

                    // If we've reach the last line of the layer, return to the first.
                    if (scrollLine == layerHeight) {
                        scrollLine = 0;
                        parallaxIndex = &layer->ParallaxIndexLines[scrollLine];
                    }
                    else
                        parallaxIndex++;
                }

                deformValues = &layer->DeformSetB[(scrollLine + layer->DeformOffsetB) & maxDeformLineMask];
                for (int i = CurrentView->DeformSplitLine; i < viewHeight; i++) {
                    // Set scan line start positions
                    info = &layer->ParallaxInfos[*parallaxIndex];
                    scanLine->SourceX = info->ParallaxPosition;
                    if (info->CanDeform)
                        scanLine->SourceX.Whole += *deformValues;
                    scanLine->SourceY = Subpixels(scrollLine, 0x0000);

                    scanLine->DeltaX = Subpixels(0x1, 0x0000);
                    scanLine->DeltaY = Subpixels(0x0, 0x0000);

                    // Iterate lines
                    scanLine++;
                    scrollLine++;
                    deformValues++;

                    // If we've reach the last line of the layer, return to the first.
                    if (scrollLine == layerHeight) {
                        scrollLine = 0;
                        parallaxIndex = &layer->ParallaxIndexLines[scrollLine];
                    }
                    else
                        parallaxIndex++;
                }
    			break;
            }
    		case Scene::DRAW_VERTICAL: {
    			break;
            }
    		case Scene::DRAW_SCANLINES: {
                int viewX = CurrentView->X + layer->CameraOffsetX.Whole;
                int viewY = CurrentView->Y + layer->CameraOffsetY.Whole;
                int scrollOffset = Scene::Frame * layer->ConstantScroll.Full;
                int scrollPositionX = (scrollOffset + (viewX * layer->RelativeScroll.Full)) >> 16;
                    scrollPositionX %= layer->Width;
                    scrollPositionX <<= 16;
                int scrollPositionY = (scrollOffset + (viewY * layer->RelativeScroll.Full)) >> 16;
                    scrollPositionY %= layer->Height;
                    scrollPositionY <<= 16;

                ScanLine* scanLine = &ScanLineBuffer[0];
                for (int i = 0; i < CurrentView->Height; i++) {
                    scanLine->SourceX.Full = scrollPositionX;
                    scanLine->SourceY.Full = scrollPositionY;
                    scanLine->DeltaX = Subpixels(0x1, 0x0000);
                    scanLine->DeltaY = Subpixels(0x0, 0x0000);

                    scrollPositionY += 0x10000;
                    scanLine++;
                }
    			break;
            }
    	}
    }
    void LayerDrawHorizontal(Layer* layer) {
        int dst_x1 = CurrentView->ClipStartX;
        int dst_y1 = CurrentView->ClipStartY;
        int dst_x2 = CurrentView->ClipEndX;
        int dst_y2 = CurrentView->ClipEndY;

        Pixel*  dstPx = (Pixel*)CurrentView->Pixels;
        Uint32  dstStride = CurrentView->Pitch;
        int     dst_strideY = dst_y1 * dstStride;
        Pixel*  dstPxLine;

        if (dst_x1 >= dst_x2 || dst_y1 >= dst_y2)
            return;

        int layerWidth = layer->Width;
        int layerWidthInBits = layer->WidthInBits;
        int layerWidthInPixels = layer->Width << TILE_SIZE_IN_BITS;
        int sourceTileCellX, sourceTileCellY, pixelsOfTileRemaining;

        Pixel* index;
        Tile*  tile;
        Uint8* color;

        int tileID;

        ScanLine* tScanLine = &ScanLineBuffer[dst_y1];
        for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
            dstPxLine = dstPx + dst_strideY;

            if (tScanLine->SourceX.Whole < 0)
                tScanLine->SourceX.Whole += layerWidthInPixels;
            else if (tScanLine->SourceX.Whole >= layerWidthInPixels)
                tScanLine->SourceX.Whole -= layerWidthInPixels;

            int dst_x = dst_x1;
            int srcX = tScanLine->SourceX.Whole, srcY = tScanLine->SourceY.Whole, srcTX, srcTY;
            index = &Palette[PaletteIndexLines[dst_y]][0];

            int maxTileDraw = ( ((dst_x2 - dst_x1) + (srcX & TILE_SIZE_MASK)) >> TILE_SIZE_IN_BITS ) - 1;

            srcTY = srcY & TILE_SIZE_MASK;
            sourceTileCellY = (srcY >> TILE_SIZE_IN_BITS);

            // Draw leftmost tile in scanline
            srcTX = srcX & TILE_SIZE_MASK;
            sourceTileCellX = (srcX >> TILE_SIZE_IN_BITS);
            pixelsOfTileRemaining = TILE_SIZE - srcTX;
            tile = &layer->Tiles[sourceTileCellX + (sourceTileCellY << layerWidthInBits)];

            if (*tile != TILE_EMPTY) {
                tileID = *tile & TILE_FXYID_MASK;
                color = &Scene::TileImageData[(tileID << (TILE_SIZE_IN_BITS << 1)) | (srcTY << TILE_SIZE_IN_BITS) | srcTX];

                while (pixelsOfTileRemaining) {
                    if (*color) PixelSetOpaque(&index[*color], &dstPxLine[dst_x], NULL, NULL, NULL);
                    pixelsOfTileRemaining--;
                    dst_x++;
                    color++;
                }
            }
            else {
                dst_x += pixelsOfTileRemaining;
            }

            tile++;
            sourceTileCellX++;
            if (sourceTileCellX == layerWidth) {
                tile -= layerWidth;
                sourceTileCellX = 0;
            }

            // Draw scanline tiles in batches of 16 pixels
            for (int j = maxTileDraw; j > 0; j--) {
                if (*tile != TILE_EMPTY) {
                    tileID = *tile & TILE_FXYID_MASK;
                    color = &Scene::TileImageData[(tileID << (TILE_SIZE_IN_BITS << 1)) | (srcTY << TILE_SIZE_IN_BITS)];

                    #define UNLOOPED(n) if (color[n]) { PixelSetOpaque(&index[color[n]], &dstPxLine[dst_x + n], NULL, NULL, NULL); }
                    UNLOOPED(0); UNLOOPED(1); UNLOOPED(2); UNLOOPED(3); UNLOOPED(4); UNLOOPED(5); UNLOOPED(6); UNLOOPED(7); UNLOOPED(8); UNLOOPED(9); UNLOOPED(10); UNLOOPED(11); UNLOOPED(12); UNLOOPED(13); UNLOOPED(14); UNLOOPED(15);
                    #undef UNLOOPED
                }
                dst_x += TILE_SIZE;

                tile++;
                sourceTileCellX++;
                if (sourceTileCellX == layerWidth) {
                    tile -= layerWidth;
                    sourceTileCellX = 0;
                }
            }
            srcX += maxTileDraw << TILE_SIZE_IN_BITS;

            // Draw rightmost
            pixelsOfTileRemaining = dst_x2 - dst_x;
            if (pixelsOfTileRemaining > 0) {
                if (*tile != TILE_EMPTY) {
                    tileID = *tile & TILE_FXYID_MASK;
                    color = &Scene::TileImageData[(tileID << (TILE_SIZE_IN_BITS << 1)) | (srcTY << TILE_SIZE_IN_BITS)];

                    while (pixelsOfTileRemaining) {
                        if (*color) PixelSetOpaque(&index[*color], &dstPxLine[dst_x], NULL, NULL, NULL);
                        pixelsOfTileRemaining--;
                        dst_x++;
                        color++;
                    }
                }
                else {
                    // dst_x += pixelsOfTileRemaining;
                }
            }

            tScanLine++;
            dst_strideY += dstStride;
        }
    }
    void LayerDrawVertical(Layer* layer) {

    }
    void LayerDrawScanLines(Layer* layer) {
        int dst_x1 = CurrentView->ClipStartX;
        int dst_y1 = CurrentView->ClipStartY;
        int dst_x2 = CurrentView->ClipEndX;
        int dst_y2 = CurrentView->ClipEndY;

        Pixel* dstPx = (Pixel*)CurrentView->Pixels;
        Pixel* dstPxLine;
        Uint32 dstStride = CurrentView->Pitch;
        int    dstStrideY = dst_y1 * dstStride;

        if (dst_x1 >= dst_x2 || dst_y1 >= dst_y2)
            return;

		// size_t layerWidth = layer->Width;
        int layerWidthInBits = layer->WidthInBits;
		// size_t layerWidthInPixels = layer->Width << TILE_SIZE_IN_BITS;

		size_t layerWidthTileMask = (layer->DataWidth) - 1;
		size_t layerHeightTileMask = (layer->DataHeight) - 1;
        int sourceTileCellX, sourceTileCellY;

        Pixel* index;
        Uint8 color;
        Tile tile;

        ScanLine* scanLine = &ScanLineBuffer[dst_y1];
        for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
            dstPxLine = dstPx + dstStrideY;
            int srcX = scanLine->SourceX,
                srcY = scanLine->SourceY,
                srcDX = scanLine->DeltaX,
                srcDY = scanLine->DeltaY,
                srcTX, srcTY;

            index = &Palette[PaletteIndexLines[dst_y]][0];
            for (int dst_x = dst_x1; dst_x < dst_x2; dst_x++) {
                srcTX = (srcX >> 16) & 15;
                srcTY = (srcY >> 16) & 15;
                sourceTileCellX = (srcX >> 20) & layerWidthTileMask;
                sourceTileCellY = (srcY >> 20) & layerHeightTileMask;
                tile = layer->Tiles[sourceTileCellX + (sourceTileCellY << layerWidthInBits)];

                if (tile != TILE_EMPTY) {
                    color = Scene::TileImageData[((tile & TILE_FXYID_MASK) << (TILE_SIZE_IN_BITS << 1)) | (srcTY << TILE_SIZE_IN_BITS) | srcTX];
                    if (color)
                        PixelSetOpaque(&index[color], &dstPxLine[dst_x], 0, NULL, NULL);
                }
                srcX += srcDX;
                srcY += srcDY;
            }
            scanLine++;
            dstStrideY += dstStride;
        }
    }

    // Drawing Everything
    void DrawAll() {
        Scene::Frame = (Scene::Frame + 1) & 0xFFFF;

        #ifdef ENABLE_STEREOSCOPIC_VIEW
        if (StereoscopicSplit > 0.0f) {
            Views[1].X = Views[0].X;
            Views[1].Y = Views[0].Y;
            Views[1].ClipStartX = Views[0].ClipStartX;
            Views[1].ClipStartY = Views[0].ClipStartY;
            Views[1].ClipEndX = Views[0].ClipEndX;
            Views[1].ClipEndY = Views[0].ClipEndY;

            ViewCount = 2;
        }
        else {
            ViewCount = 1;
        }
        #endif

        CurrentView = Views;
        CurrentViewIndex = 0;
        while (CurrentViewIndex < Game::State.ViewCount) {
            // Clear DrawGroup Layer lists
            for (CurrentDrawGroupIndex = 0; CurrentDrawGroupIndex < MAX_DRAWGROUPS; CurrentDrawGroupIndex++)
                DrawGroups[CurrentDrawGroupIndex].LayerCount = 0;

            // Add layers to DrawGroups
            for (int layerIndex = 0; layerIndex < MAX_LAYERS; layerIndex++) {
                Layer* layer = &Scene::Layers[layerIndex];
                if (layer->Hidden[CurrentViewIndex])
                    continue;

                DrawGroup* drawGroup = &DrawGroups[layer->DrawGroup[CurrentViewIndex]];
                drawGroup->LayerIndices[drawGroup->LayerCount++] = layerIndex;
            }

            // Draw DrawGroups
            for (CurrentDrawGroupIndex = 0; CurrentDrawGroupIndex < MAX_DRAWGROUPS; CurrentDrawGroupIndex++) {
                DrawGroup* drawGroup = &DrawGroups[CurrentDrawGroupIndex];

                Game::State.CurrentDrawGroup = CurrentDrawGroupIndex;

                // Run prefix function
                if (drawGroup->PrefixFunction)
                    drawGroup->PrefixFunction();

                // Do depth sorting
                if (drawGroup->EntityDepthSortingEnabled) {
                    if (drawGroup->EntityCount > 1) {
                        for (int a = 1; a < drawGroup->EntityCount; a++) {
                            int b, temp = drawGroup->EntityIndices[a];
                            for (b = a - 1; b >= 0 && Scene::EntitySlots[drawGroup->EntityIndices[b]].Depth < Scene::EntitySlots[temp].Depth; b--) {
                                drawGroup->EntityIndices[b + 1] = drawGroup->EntityIndices[b];
                            }
                            drawGroup->EntityIndices[b + 1] = temp;
                        }
                    }
                }

                // Draw Entities
                for (int i = 0; i < drawGroup->EntityCount; i++) {
                    int slotID = drawGroup->EntityIndices[i];

                    Graphics::DidDraw = false;

                    Game::State.CurrentEntity = Scene::CurrentEntity = &Scene::EntitySlots[slotID];
                    if (Scene::CurrentEntity->CanDraw) {
                        if (Scene::CurrentEntity->ClassID > 0) {
                            auto onStageDraw = GameLinker::ClassList[Scene::ClassIndexList[Scene::CurrentEntity->ClassID]].onStageDraw;
                            if (onStageDraw)
                                onStageDraw();

                            Scene::CurrentEntity->DidDraw |= Graphics::DidDraw << CurrentViewIndex;
                        }
                    }
                }

                // Draw Layers
                for (int i = 0; i < drawGroup->LayerCount; i++) {
                    Layer* layer = &Scene::Layers[drawGroup->LayerIndices[i]];
                    if (layer->ScanLineFunction)
                        layer->ScanLineFunction(ScanLineBuffer);
                    else
                        LayerInitScanLines(layer);

                    switch (layer->DrawBehavior) {
                        case 3:
                        case Scene::DRAW_HORIZONTAL:
                            LayerDrawHorizontal(layer);
                            break;
                        case Scene::DRAW_VERTICAL:
                            LayerDrawVertical(layer);
                            break;
                        case Scene::DRAW_SCANLINES:
                            LayerDrawScanLines(layer);
                            break;
                    }
                }
            }

            /*for (int i = 0; i < MAX_ENTITIES; i++) {
                Scene::CurrentEntity = &Scene::EntitySlots[i];
                if (Scene::CurrentEntity->ClassID > 0) {
                    auto onEditorDraw = GameLinker::ClassList[Scene::ClassIndexList[Scene::CurrentEntity->ClassID]].onEditorDraw;
                    if (onEditorDraw)
                        onEditorDraw();
                }
            }*/

            // Resources::ResImage resImage = Resources::ResourceImages[1];
            // if (resImage.UnloadPolicy != 0) {
            //     bool temp = DrawToScreen;
            //     Vector2 positions[] = {
            //         Vector2(0 * 0x10000, 0 * 0x10000),
            //         Vector2(128 * 0x10000, 64 * 0x10000),
            //         Vector2(48 * 0x10000, 112 * 0x10000),
            //     };
            //     Color   colors[] = {
            //         Color(0xFF, 0x00, 0x00),
            //         Color(0x00, 0xFF, 0x00),
            //         Color(0x00, 0x00, 0xFF),
            //     };
            //     Vector2 uvs[] = {
            //         Vector2(0 * 0x10000, 0 * 0x10000),
            //         Vector2(64 * 0x10000, 64 * 0x10000),
            //         Vector2(0 * 0x10000, 128 * 0x10000),
            //     };
            //
            //     DrawToScreen = true;
            //     DrawPolygonTextured(positions, colors, uvs, &resImage.ImageData, sizeof(positions) / sizeof(positions[0]), BLEND_NONE);
            //     DrawToScreen = temp;
            // }

            CurrentView++;
            CurrentViewIndex++;
        }
    }




    void LayerDraw_Editor(Layer* layer) {
        int dst_x1 = CurrentView->ClipStartX;
        int dst_y1 = CurrentView->ClipStartY;
        int dst_x2 = CurrentView->ClipEndX;
        int dst_y2 = CurrentView->ClipEndY;

        Pixel* dstPx = (Pixel*)CurrentView->Pixels;
        Pixel* dstPxLine;
        Uint32 dstStride = CurrentView->Pitch;
        int    dstStrideY = dst_y1 * dstStride;

        if (dst_x1 >= dst_x2 || dst_y1 >= dst_y2)
            return;

        // size_t layerWidth = layer->Width;
        int layerWidthInBits = layer->WidthInBits;
        // size_t layerWidthInPixels = layer->Width << TILE_SIZE_IN_BITS;

        // int layerWidthTileMask = (layer->DataWidth) - 1;
        // int layerHeightTileMask = (layer->DataHeight) - 1;
        int sourceTileCellX, sourceTileCellY;

        Pixel* index;
        Uint8 color;
        Tile tile;

        ScanLine* scanLine = &ScanLineBuffer[dst_y1];
        for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
            dstPxLine = dstPx + dstStrideY;
            int srcX = scanLine->SourceX,
                srcY = scanLine->SourceY,
                srcDX = scanLine->DeltaX,
                srcDY = scanLine->DeltaY,
                srcTX, srcTY;

            // PaletteIndexLines[dst_y]
            index = &Palette[0][0];
            for (int dst_x = dst_x1; dst_x < dst_x2; dst_x++) {
                srcTX = (srcX >> 16) & 15;
                srcTY = (srcY >> 16) & 15;
                sourceTileCellX = (srcX >> 20);
                sourceTileCellY = (srcY >> 20);
                if (sourceTileCellX >= 0 && sourceTileCellX < (int)layer->Width &&
                    sourceTileCellY >= 0 && sourceTileCellY < (int)layer->Height) {
                    tile = layer->Tiles[sourceTileCellX + (sourceTileCellY << layerWidthInBits)];

                    if (tile != TILE_EMPTY) {
                        color = Scene::TileImageData[((tile & TILE_FXYID_MASK) << (TILE_SIZE_IN_BITS << 1)) | (srcTY << TILE_SIZE_IN_BITS) | srcTX];
                        if (color)
                            PixelSetOpaque(&index[color], &dstPxLine[dst_x], 0, NULL, NULL);
                    }
                }
                srcX += srcDX;
                srcY += srcDY;
            }
            scanLine++;
            dstStrideY += dstStride;
        }
    }

    void DrawAll_Editor() {
        CurrentView = Views;
        CurrentViewIndex = 0;

        // Clear DrawGroup Layer lists
        for (CurrentDrawGroupIndex = 0; CurrentDrawGroupIndex < MAX_DRAWGROUPS; CurrentDrawGroupIndex++)
            DrawGroups[CurrentDrawGroupIndex].LayerCount = 0;

        // Add layers to DrawGroups
        for (int layerIndex = 0; layerIndex < MAX_LAYERS; layerIndex++) {
            Layer* layer = &Scene::Layers[layerIndex];
            if (layer->Hidden[CurrentViewIndex])
                continue;

            DrawGroup* drawGroup = &DrawGroups[layer->DrawGroup[CurrentViewIndex]];
            drawGroup->LayerIndices[drawGroup->LayerCount++] = layerIndex;
        }

        // Draw DrawGroups
        for (CurrentDrawGroupIndex = 0; CurrentDrawGroupIndex < MAX_DRAWGROUPS; CurrentDrawGroupIndex++) {
            DrawGroup* drawGroup = &DrawGroups[CurrentDrawGroupIndex];

            // Draw Entities
            for (int i = 0; i < drawGroup->EntityCount * 0; i++) {
                int slotID = drawGroup->EntityIndices[i];

                Graphics::DidDraw = false;

                Scene::CurrentEntity = &Scene::EntitySlots[slotID];
                if (Scene::CurrentEntity->CanDraw) {
                    if (Scene::CurrentEntity->ClassID > 0) {
                        auto onEditorDraw = GameLinker::ClassList[Scene::ClassIndexList[Scene::CurrentEntity->ClassID]].onStageDraw;
                        if (onEditorDraw) {
                            onEditorDraw();
                        }
                        else {
                            // Draw default
                        }

                        Scene::CurrentEntity->DidDraw |= Graphics::DidDraw << CurrentViewIndex;
                    }
                }
            }

            // Draw Layers
            for (int i = 0; i < drawGroup->LayerCount; i++) {
                Layer* layer = &Scene::Layers[drawGroup->LayerIndices[i]];
                {
                    int viewX = CurrentView->X;
                    int viewY = CurrentView->Y;
                    // int scrollPositionX = (scrollOffset + (viewX * layer->RelativeScroll.Full)) >> 16;
                    // scrollPositionX %= layer->Width;
                    int scrollPositionX = viewX;
                    scrollPositionX <<= 16;
                    // int scrollPositionY = (scrollOffset + (viewY * layer->RelativeScroll.Full)) >> 16;
                    // scrollPositionY %= layer->Height;
                    int scrollPositionY = viewY;
                    scrollPositionY <<= 16;

                    ScanLine* scanLine = &ScanLineBuffer[0];
                    for (int i = 0; i < CurrentView->Height; i++) {
                        scanLine->SourceX.Full = scrollPositionX;
                        scanLine->SourceY.Full = scrollPositionY;
                        scanLine->DeltaX = Subpixels(0x1, 0x0000);
                        scanLine->DeltaY = Subpixels(0x0, 0x0000);

                        scrollPositionY += 0x10000;
                        scanLine++;
                    }
                }

                LayerDraw_Editor(layer);
            }
        }
    }
}

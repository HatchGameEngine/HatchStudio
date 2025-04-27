#pragma once

#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/IO/Stream.h>

namespace UI::Graphics::Font {
    struct Glyph {
        float Advance;
        int SourceX;
        int SourceY;
        int Width;
        int Height;
        float OffsetX;
        float OffsetY;
    };
    struct Face {
        SDL_Texture* Texture;
        Glyph Glyphs[256];
        float Ascent;
        float Descent;
        float LineGap;
        void* stbtt;
        float sampleSize;
    };

    extern Face* Arial[32];

    Face* LoadFontFace(Stream* stream, float size);
    void DisposeFontFace(Face* fontFace);
}

#include <UI/Graphics/Renderer.hpp>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <UI/System/Diagnostics.hpp>

namespace UI::Graphics::Renderer {
    // SDL_Texture* FrameBufferTexture;
    int RendererW;
    int RendererH;
    SDL_Window* Window;
    SDL_Renderer* Renderer;

    int rendererScaleI = 1;
    float rendererScaleF = 1.0f;

    bool Init() {
        Window = NULL;
        Renderer = NULL;
        // FrameBufferTexture = NULL;

        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "1");

        // FIXME: Not the best place for this.
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) < 0) {
			System::Diagnostics::SetError("SDL_Init failed with error: %s", SDL_GetError());
            return false;
        }
        // printf("SDL_Init %s\n", SDL_GetError());

        Window = SDL_CreateWindow(NULL,
            SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0),
            100, 100, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI); // SDL_WINDOW_SHOWN |
        if (!Window) {
			System::Diagnostics::SetError("SDL_CreateWindow failed with error: %s", SDL_GetError());
			return false;
        }

        Uint32 flags = SDL_RENDERER_ACCELERATED;
        if (true)
            flags |= SDL_RENDERER_PRESENTVSYNC;

        Renderer = SDL_CreateRenderer(Window, -1, flags);
        if (!Renderer) {
			System::Diagnostics::SetError("SDL_CreateRenderer failed with error: %s", SDL_GetError());
			return false;
        }

        int rW, rH;

        SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_BLEND);
        SDL_GetRendererOutputSize(Renderer, &rW, &rH);
        SDL_GetWindowSize(Window, &RendererW, &RendererH);

        rendererScaleF = (float)rH / RendererH;
        rendererScaleI = rH / RendererH;

        return true;
    }
    void Dispose() {
        SDL_DestroyRenderer(Renderer);
        SDL_DestroyWindow(Window);

        SDL_Quit();
    }

    // Public functions
    void SetDrawColor(Color color) {
        SDL_SetRenderDrawColor(Renderer::Renderer, color.R, color.G, color.B, color.A);
    }
    void DrawLine(int x1, int y1, int x2, int y2, Color color) {
        SetDrawColor(color);
        x1 *= rendererScaleI;
        y1 *= rendererScaleI;
        x2 *= rendererScaleI;
        y2 *= rendererScaleI;
        SDL_RenderDrawLine(Renderer, x1, y1, x2, y2);
    }
    void DrawRect(SDL_Rect* rect, Color color) {
        SetDrawColor(color);

        SDL_Rect rectAdj = *rect;
        DstRectAdjustment(&rectAdj);

        SDL_RenderFillRect(Renderer, &rectAdj);
    }
    void DrawRect(int x, int y, int w, int h, Color color) {
        SDL_Rect r = { x, y, w, h };
        DrawRect(&r, color);
    }
    void StrokeRect(SDL_Rect* rect, Color color) {
        SetDrawColor(color);

        SDL_Rect rectAdj = *rect;
        DstRectAdjustment(&rectAdj);

        SDL_RenderDrawRect(Renderer, &rectAdj);
    }
    void StrokeRect(int x, int y, int w, int h, Color color) {
        SDL_Rect r = { x, y, w, h };
        StrokeRect(&r, color);
    }
    void DrawTexture(SDL_Texture* texture, SDL_Rect* src, SDL_Rect* rect, Color color, double angle, SDL_Point* center, int flip) {
        if (!texture) return;

        SDL_SetTextureColorMod(texture, color.R, color.G, color.B);
		SDL_SetTextureAlphaMod(texture, color.A);

        SDL_Rect rectAdj = *rect;
        DstRectAdjustment(&rectAdj);

        int flipFlag = SDL_FLIP_NONE;
        if (flip & FLIPXY_X)
            flipFlag |= SDL_FLIP_HORIZONTAL;
        if (flip & FLIPXY_Y)
            flipFlag |= SDL_FLIP_VERTICAL;

		SDL_RenderCopyEx(Renderer, texture, src, &rectAdj, angle, center, (SDL_RendererFlip)flipFlag);
    }
    void DrawTexture(SDL_Texture* texture, SDL_Rect* src, int x, int y, int w, int h, Color color, double angle, SDL_Point* center, int flip) {
        SDL_Rect r = { x, y, w, h };
        DrawTexture(texture, src, &r, color, angle, center, flip);
    }

    void DrawFont(String* text, UI::Graphics::Font::Face* font, int x, int y, int alignment, Color color) {
        if (!text || !font)
            return;

        int w, h;
        SDL_Rect glyphSrc;
        SDL_FRect glyphDst;
        MeasureFont(text, font, &w, &h);

        float fx = x, fy = y;

        SDL_SetTextureColorMod(font->Texture, color.R, color.G, color.B);
        SDL_SetTextureAlphaMod(font->Texture, color.A);

        if ((alignment & 0x0F) == TEXT_ALIGN_CENTER)
            fx -= w >> 1;
        else if ((alignment & 0x0F) == TEXT_ALIGN_RIGHT)
            fx -= w;

        switch (alignment & 0xF0) {
        case TEXT_VALIGN_TOP:
            fy += font->Ascent;
            break;
        case TEXT_VALIGN_MIDDLE:
            fy += font->Ascent;
            fy -= (font->Ascent - font->Descent) / 2;
            break;
        case TEXT_VALIGN_BOTTOM:
            fy += font->Descent;
            break;
        }

        for (size_t i = 0; i < text->Length; i++) {
            int character = text->Text[i] & 0xFF;
            UI::Graphics::Font::Glyph* glyph = &font->Glyphs[character];

            glyphSrc.x = glyph->SourceX;
            glyphSrc.y = glyph->SourceY;
            glyphSrc.w = glyph->Width;
            glyphSrc.h = glyph->Height;

            glyphDst.x = fx + glyph->OffsetX;
            glyphDst.y = fy + glyph->OffsetY;
            glyphDst.w = glyph->Width / font->sampleSize;
            glyphDst.h = glyph->Height / font->sampleSize;

            DstRectFAdjustment(&glyphDst);

            SDL_RenderCopyF(Renderer, font->Texture, &glyphSrc, &glyphDst);
            fx += glyph->Advance;
        }
    }
    void DrawFontEllipsis(String* text, UI::Graphics::Font::Face* font, int x, int y, int maxWidth, int alignment, Color color) {
        if (!text || !font || maxWidth <= 0)
            return;

        int w, h;
        SDL_Rect glyphSrc;
        SDL_FRect glyphDst;
        size_t lastChar = 0;
        MeasureFontEllipsis(text, font, &w, &h, maxWidth, &lastChar);

        float fx = x, fy = y;

        SDL_SetTextureColorMod(font->Texture, color.R, color.G, color.B);
        SDL_SetTextureAlphaMod(font->Texture, color.A);

        if ((alignment & 0x0F) == TEXT_ALIGN_CENTER)
            fx -= w >> 1;
        else if ((alignment & 0x0F) == TEXT_ALIGN_RIGHT)
            fx -= w;

        switch (alignment & 0xF0) {
        case TEXT_VALIGN_TOP:
            fy += font->Ascent;
            break;
        case TEXT_VALIGN_MIDDLE:
            fy += font->Ascent;
            fy -= (font->Ascent - font->Descent) / 2;
            break;
        case TEXT_VALIGN_BOTTOM:
            fy += font->Descent;
            break;
        }

        for (size_t i = 0; i < text->Length && i <= lastChar; i++) {
            int character = text->Text[i] & 0xFF;
            UI::Graphics::Font::Glyph* glyph = &font->Glyphs[character];

            glyphSrc.x = glyph->SourceX;
            glyphSrc.y = glyph->SourceY;
            glyphSrc.w = glyph->Width;
            glyphSrc.h = glyph->Height;

            glyphDst.x = fx + glyph->OffsetX;
            glyphDst.y = fy + glyph->OffsetY;
            glyphDst.w = glyph->Width / font->sampleSize;
            glyphDst.h = glyph->Height / font->sampleSize;

            DstRectFAdjustment(&glyphDst);

            SDL_RenderCopyF(Renderer, font->Texture, &glyphSrc, &glyphDst);
            fx += glyph->Advance;
        }

        if (lastChar != text->Length - 1) {
            UI::Graphics::Font::Glyph* glyph = &font->Glyphs['.'];
            glyphSrc.x = glyph->SourceX;
            glyphSrc.y = glyph->SourceY;
            glyphSrc.w = glyph->Width;
            glyphSrc.h = glyph->Height;

            glyphDst.x = fx + glyph->OffsetX;
            glyphDst.y = fy + glyph->OffsetY;
            glyphDst.w = glyph->Width / font->sampleSize;
            glyphDst.h = glyph->Height / font->sampleSize;

            DstRectFAdjustment(&glyphDst);

            SDL_RenderCopyF(Renderer, font->Texture, &glyphSrc, &glyphDst);
            fx += glyph->Advance;

            glyphDst.x = fx + glyph->OffsetX;
            glyphDst.y = fy + glyph->OffsetY;
            glyphDst.w = glyph->Width / font->sampleSize;
            glyphDst.h = glyph->Height / font->sampleSize;

            DstRectFAdjustment(&glyphDst);

            SDL_RenderCopyF(Renderer, font->Texture, &glyphSrc, &glyphDst);
            fx += glyph->Advance;

            glyphDst.x = fx + glyph->OffsetX;
            glyphDst.y = fy + glyph->OffsetY;
            glyphDst.w = glyph->Width / font->sampleSize;
            glyphDst.h = glyph->Height / font->sampleSize;

            DstRectFAdjustment(&glyphDst);

            SDL_RenderCopyF(Renderer, font->Texture, &glyphSrc, &glyphDst);
            fx += glyph->Advance;
        }
    }
    void MeasureFont(String* text, UI::Graphics::Font::Face* font, int* width, int* height) {
        if (!text || !font)
            return;

        float w = 0.0f, h = 0.0f;
        for (size_t i = 0; i < text->Length; i++) {
            int character = text->Text[i] & 0xFF;
            w += font->Glyphs[character].Advance;
            h = M_MAX(h, font->Glyphs[character].Height / font->sampleSize);
        }

        *width = w;
        *height = h;
    }
    void MeasureFontEllipsis(String* text, UI::Graphics::Font::Face* font, int* width, int* height, int maxWidth, size_t* lastChar) {
        if (!text || !font)
            return;

        *lastChar = 0;

        MeasureFont(text, font, width, height);
        if (*width <= maxWidth) {
            *lastChar = text->Length - 1;
            return;
        }

        float ellipsisWidth = font->Glyphs['.'].Advance * 3.0f;

        float w = ellipsisWidth, h = 0.0f;
        for (size_t i = 0; i < text->Length; i++) {
            int character = text->Text[i] & 0xFF;
            if (w + font->Glyphs[character].Advance > maxWidth)
                break;

            w += font->Glyphs[character].Advance;
            h = M_MAX(h, font->Glyphs[character].Height / font->sampleSize);
            *lastChar = i;
        }

        *width = w;
        *height = h;
    }
}

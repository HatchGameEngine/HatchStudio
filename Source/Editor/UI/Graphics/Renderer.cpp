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
    void DrawLine(int x1, int y1, int x2, int y2, Color color, float thickness) {
        if (thickness <= 0.0) {
            return;
        }

        // Code taken from SDL2_gfx
        double dx, dy, dx1, dy1, dx2, dy2;
        double l, wl2, nx, ny, ang, adj;
        SDL_Vertex verts[4];
        const int indices[6] = { 0, 1, 2, 2, 3, 0 };

        // Calculate offsets for sides
        dx = (double)(x2 - x1);
        dy = (double)(y2 - y1);
        l = SDL_sqrt(dx*dx + dy*dy);
        ang = SDL_atan2(dx, dy);
        adj = 0.1 + 0.9 * SDL_fabs(SDL_cos(2.0 * ang));
        wl2 = ((double)thickness - adj)/(2.0 * l);
        nx = dx * wl2;
        ny = dy * wl2;

        // Build polygon
        dx1 = (double)x1;
        dy1 = (double)y1;
        dx2 = (double)x2;
        dy2 = (double)y2;

        verts[0] = {
            SDL_FPoint{(float)(dx1 + ny) * rendererScaleF, (float)(dy1 - nx) * rendererScaleF},
            SDL_Color{color.R, color.G, color.B, color.A},
            SDL_FPoint{0}
        };
        verts[1] = {
            SDL_FPoint{(float)(dx1 - ny) * rendererScaleF, (float)(dy1 + nx) * rendererScaleF},
            SDL_Color{color.R, color.G, color.B, color.A},
            SDL_FPoint{0}
        };
        verts[2] = {
            SDL_FPoint{(float)(dx2 - ny) * rendererScaleF, (float)(dy2 + nx) * rendererScaleF},
            SDL_Color{color.R, color.G, color.B, color.A},
            SDL_FPoint{0}
        };
        verts[3] = {
            SDL_FPoint{(float)(dx2 + ny) * rendererScaleF, (float)(dy2 - nx) * rendererScaleF},
            SDL_Color{color.R, color.G, color.B, color.A},
            SDL_FPoint{0}
        };

        SDL_RenderGeometry(Renderer, NULL, verts, 4, indices, 6);
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
    void DrawFontWrapped(String* text, UI::Graphics::Font::Face* font, int x, int y, int alignment, float maxWidth, Color color) {
        if (!text || !font)
            return;

        SDL_Rect glyphSrc;
        SDL_FRect glyphDst;

        Sint16* lineStart = text->Text;
        Sint16* wordStart = text->Text;
        Sint16* textEnd = text->Text + text->Length;

        float currX = x;
        float currY = y;

        SDL_SetTextureColorMod(font->Texture, color.R, color.G, color.B);
        SDL_SetTextureAlphaMod(font->Texture, color.A);

        switch (alignment & 0xF0) {
        case TEXT_VALIGN_TOP:
            currY += font->Ascent;
            break;
        case TEXT_VALIGN_MIDDLE:
            currY += font->Ascent;
            currY -= (font->Ascent - font->Descent) / 2;
            break;
        case TEXT_VALIGN_BOTTOM:
            currY += font->Descent;
            break;
        }

        int word = 0;

        for (size_t i = 0; i < text->Length; i++) {
            Sint16* textPtr = text->Text + i;

            int character = *textPtr & 0xFF;

            bool isLineBreak = character == 0x0A;

            if ((textPtr != wordStart && character == 0x20) || isLineBreak) {
                float lineWidth = 0.0f;
                for (Sint16* o = lineStart; o < textPtr; o++) {
                    lineWidth += font->Glyphs[*o].Advance;
                }

                bool canLineBreak = isLineBreak;
                if (word > 0 && lineWidth > maxWidth) {
                    canLineBreak = true;
                }

                Sint16* start = isLineBreak ? (textPtr + 1) : wordStart;
                Sint16* end = isLineBreak ? textPtr : (wordStart - 1);

                if (canLineBreak) {
                    currX = x;

                    if ((alignment & 0x0F) == TEXT_ALIGN_CENTER)
                        currX -= lineWidth * 0.5f;
                    else if ((alignment & 0x0F) == TEXT_ALIGN_RIGHT)
                        currX -= lineWidth;

                    for (Sint16* o = lineStart; o < end; o++) {
                        UI::Graphics::Font::Glyph* glyph = &font->Glyphs[*o & 0xFF];

                        glyphSrc.x = glyph->SourceX;
                        glyphSrc.y = glyph->SourceY;
                        glyphSrc.w = glyph->Width;
                        glyphSrc.h = glyph->Height;

                        glyphDst.x = currX + glyph->OffsetX;
                        glyphDst.y = currY + glyph->OffsetY;
                        glyphDst.w = glyph->Width / font->sampleSize;
                        glyphDst.h = glyph->Height / font->sampleSize;

                        DstRectFAdjustment(&glyphDst);

                        SDL_RenderCopyF(Renderer, font->Texture, &glyphSrc, &glyphDst);
                        currX += glyph->Advance;
                    }

                    lineStart = start;

                    currY += font->Ascent - font->Descent;
                }

                wordStart = textPtr + 1;
                word++;
            }
        }

        // Draw the remaining line
        currX = x;

        float lineWidth = 0.0f;
        for (Sint16* o = lineStart; o < textEnd; o++) {
            lineWidth += font->Glyphs[*o].Advance;
        }

        if ((alignment & 0x0F) == TEXT_ALIGN_CENTER)
            currX -= lineWidth * 0.5f;
        else if ((alignment & 0x0F) == TEXT_ALIGN_RIGHT)
            currX -= lineWidth;

        for (Sint16* o = lineStart; o < textEnd; o++) {
            UI::Graphics::Font::Glyph* glyph = &font->Glyphs[*o & 0xFF];

            glyphSrc.x = glyph->SourceX;
            glyphSrc.y = glyph->SourceY;
            glyphSrc.w = glyph->Width;
            glyphSrc.h = glyph->Height;

            glyphDst.x = currX + glyph->OffsetX;
            glyphDst.y = currY + glyph->OffsetY;
            glyphDst.w = glyph->Width / font->sampleSize;
            glyphDst.h = glyph->Height / font->sampleSize;

            DstRectFAdjustment(&glyphDst);

            SDL_RenderCopyF(Renderer, font->Texture, &glyphSrc, &glyphDst);
            currX += glyph->Advance;
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

        if (width) {
            *width = w;
        }
        if (height) {
            *height = h;
        }
    }
    void MeasureFontWrapped(String* text, UI::Graphics::Font::Face* font, float maxWidth, int* width, int* height) {
        if (!text || !font)
            return;

        Sint16* lineStart = text->Text;
        Sint16* wordStart = text->Text;
        Sint16* textEnd = text->Text + text->Length;

        float currentWidth = 0.0f;
        float totalWidth = 0.0f;
        float totalHeight = 0.0f;
        int word = 0;

        for (size_t i = 0; i < text->Length; i++) {
            Sint16* textPtr = text->Text + i;

            int character = *textPtr & 0xFF;

            bool isLineBreak = character == 0x0A;

            if ((textPtr != wordStart && character == 0x20) || isLineBreak) {
                float lineWidth = 0.0f;
                for (Sint16* o = lineStart; o < textPtr; o++) {
                    lineWidth += font->Glyphs[*o].Advance;
                }
                if (lineWidth > totalWidth) {
                    totalWidth = lineWidth;
                }

                bool canLineBreak = isLineBreak;
                if (word > 0 && lineWidth > maxWidth) {
                    canLineBreak = true;
                }

                Sint16* start = isLineBreak ? (textPtr + 1) : wordStart;
                Sint16* end = isLineBreak ? textPtr : (wordStart - 1);

                if (canLineBreak) {
                    currentWidth = 0.0f;

                    totalHeight += font->Ascent - font->Descent;

                    lineStart = start;
                }

                wordStart = textPtr + 1;
                word++;
            }
        }

        // Get the width of the remaining line
        float lineWidth = 0.0f;
        for (Sint16* o = lineStart; o < textEnd; o++) {
            lineWidth += font->Glyphs[*o].Advance;
        }
        if (lineWidth > totalWidth) {
            totalWidth = lineWidth;
        }

        if (width) {
            *width = (int)totalWidth;
        }
        if (height) {
            *height = (int)totalHeight;
        }
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

        if (width) {
            *width = w;
        }
        if (height) {
            *height = h;
        }
    }
}

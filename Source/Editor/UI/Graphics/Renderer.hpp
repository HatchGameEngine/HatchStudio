#pragma once

#include <SDL2/SDL.h>

#include <UI/Graphics/Font.hpp>

namespace UI::Graphics::Renderer {
    namespace Font = UI::Graphics::Font;

    extern SDL_Window* Window;
    extern SDL_Renderer* Renderer;
    extern int RendererW, RendererH;

    extern int rendererScaleI;
    extern float rendererScaleF;

    inline void DstRectAdjustment(SDL_Rect* rect) {
        rect->x *= rendererScaleI;
        rect->y *= rendererScaleI;
        rect->w *= rendererScaleI;
        rect->h *= rendererScaleI;
    }
    inline void DstRectUnadjustment(SDL_Rect* rect) {
        rect->x /= rendererScaleI;
        rect->y /= rendererScaleI;
        rect->w /= rendererScaleI;
        rect->h /= rendererScaleI;
    }
    inline void DstRectFAdjustment(SDL_FRect* rect) {
        rect->x *= rendererScaleF;
        rect->y *= rendererScaleF;
        rect->w *= rendererScaleF;
        rect->h *= rendererScaleF;
    }

    // Window Functions
    bool Init();
    void Dispose();
    bool InitMenuBar();
    void Sleep(double seconds);

    void SetDrawColor(Color color);
    void DrawLine(int x1, int y1, int x2, int y2, Color color);
    void DrawLine(int x1, int y1, int x2, int y2, Color color, float thickness);
    void DrawRect(SDL_Rect* rect, Color color);
    void DrawRect(int x, int y, int w, int h, Color color);
    void StrokeRect(SDL_Rect* rect, Color color);
    void StrokeRect(int x, int y, int w, int h, Color color);
    void DrawTexture(SDL_Texture* texture, SDL_Rect* src, SDL_Rect* rect, Color color, double angle = 0.0, SDL_Point* center = NULL, int flip = 0);
    void DrawTexture(SDL_Texture* texture, SDL_Rect* src, int x, int y, int w, int h, Color color, double angle = 0.0, SDL_Point* center = NULL, int flip = 0);
    void DrawFont(String* text, Font::Face* font, int x, int y, int alignment, Color color);
    void DrawFontWrapped(String* text, Font::Face* font, int x, int y, int alignment, float maxWidth, Color color);
    void DrawFontEllipsis(String* text, Font::Face* font, int x, int y, int maxWidth, int alignment, Color color);
    void MeasureFont(String* text, Font::Face* font, int* width, int* height);
    void MeasureFontWrapped(String* text, Font::Face* font, float maxWidth, int* width, int* height);
    void MeasureFontEllipsis(String* text, Font::Face* font, int* width, int* height, int maxWidth, size_t* lastChar);
}

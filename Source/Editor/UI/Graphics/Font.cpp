#include <UI/Graphics/Font.hpp>
#include <UI/Graphics/Renderer.hpp>

#define STB_RECT_PACK_IMPLEMENTATION
#include <Hatch/Libraries/stb_rect_pack.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <Hatch/Libraries/stb_truetype.h>

#include <ctype.h>

namespace UI::Graphics::Font {
    Face* Arial[32];

    Face* LoadFontFace(Stream* stream, float size) {
        uint8_t* atlas_buffer;
        int i, atlas_width, atlas_height;

        stbtt_pack_context pc;
        stbtt_packedchar   chardata[256];

        if (!stream)
            return NULL;

        size_t font_size = stream->Length();
        Uint8* font_data = (Uint8*)malloc(font_size);
        if (!font_data) {
            return NULL;
        }

        stream->ReadBytes(font_data, font_size);

        Face* fontFace = new Face();
        fontFace->stbtt = (void*)(new stbtt_fontinfo());
        stbtt_InitFont((stbtt_fontinfo*)fontFace->stbtt, font_data, stbtt_GetFontOffsetForIndex(font_data, 0));

        int ascent, descent, lineGap;
        float scale = stbtt_ScaleForPixelHeight((stbtt_fontinfo*)fontFace->stbtt, size);
        stbtt_GetFontVMetrics((stbtt_fontinfo*)fontFace->stbtt, &ascent, &descent, &lineGap);

        fontFace->Ascent = ascent * scale;
        fontFace->Descent = descent * scale;
        fontFace->LineGap = lineGap * scale;

        atlas_width = atlas_height = 512;

    alloc_atlas:
        atlas_buffer = (uint8_t*)calloc(atlas_height, atlas_width);

        if (!atlas_buffer)
            return NULL;

        fontFace->sampleSize = 4;
        stbtt_PackBegin(&pc, atlas_buffer,
            atlas_width, atlas_height,
            atlas_width, 1, NULL);
        stbtt_PackSetOversampling(&pc, fontFace->sampleSize, fontFace->sampleSize);
        stbtt_PackFontRange(&pc, font_data, 0, size, 0, 256, chardata);
        stbtt_PackEnd(&pc);

        for (i = 0; i < 256; ++i) {
            Glyph* g = &fontFace->Glyphs[i];
            stbtt_packedchar* c = &chardata[i];

            g->Advance = c->xadvance;
            g->SourceX = c->x0;
            g->SourceY = c->y0;
            g->OffsetX = c->xoff;
            g->OffsetY = c->yoff;
            g->Width = c->x1 - c->x0;
            g->Height = c->y1 - c->y0;

            /* make sure important characters fit */
            if (isprint(i) && !isspace(i) && (!g->Width || !g->Height)) {
                /* increase atlas by 20% in all directions */
                atlas_width *= 1.2;
                atlas_height *= 1.2;

                free(atlas_buffer);
                goto alloc_atlas;
                break;
            }
        }

        Uint32* atlas_buffer_rgba = (Uint32*)malloc(atlas_width * atlas_height * sizeof(Uint32));
        if (!atlas_buffer_rgba) {
            free(atlas_buffer);
            delete fontFace;
            return NULL;
        }

        for (i = 0; i < atlas_width * atlas_height; i++) {
            atlas_buffer_rgba[i] = 0xFFFFFFU | (atlas_buffer[i] << 24);
        }

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        fontFace->Texture = SDL_CreateTexture(Renderer::Renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, atlas_width, atlas_height);
        if (!fontFace->Texture) {
            free(atlas_buffer);
            free(atlas_buffer_rgba);
            delete fontFace;
            return NULL;
        }
        SDL_SetTextureBlendMode(fontFace->Texture, SDL_BLENDMODE_BLEND);

        SDL_UpdateTexture(fontFace->Texture, NULL, atlas_buffer_rgba, atlas_width * sizeof(Uint32));
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

        free(atlas_buffer);
        free(atlas_buffer_rgba);

        return fontFace;
    }
    void DisposeFontFace(Face* fontFace) {
        SDL_DestroyTexture(fontFace->Texture);
        delete fontFace;
    }
}

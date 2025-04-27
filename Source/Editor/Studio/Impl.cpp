#ifdef TARGET_USING_SDL2_FRAMEWORK
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include <ctype.h>
#include <math.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Hashing/MD5.h>
#include <Hatch/ImageFormats/GIF.h>
#include <Hatch/IO/Stream.h>
#include <Hatch/IO/ResourceStream.h>

#include <Hatch/Diagnostics.h>
#include <Hatch/Graphics.h>
#include <Hatch/Math.h>
#include <Hatch/Memory.h>
#include <Hatch/Resources.h>
#include <Hatch/Scene.h>
#include <Hatch/Services.h>

#include <UI/Graphics/Renderer.hpp>

#include <vector>

#include <Studio/Impl.hpp>

#include <Libraries/stb_image.h>

namespace Studio {
    namespace Textures {
        bool UpdateTextureFromImage(SDL_Texture** texture, Image* image) {
            if (!*texture)
                return false;

            Uint32 format, ConversionRMask, ConversionGMask, ConversionBMask, ConversionAMask;
            if (SDL_QueryTexture(*texture, &format, NULL, NULL, NULL)) {
                Diagnostics::SetError("SDL_QueryTexture failed with error: %s", SDL_GetError());
                return false;
            }

            int bpp;
            if (!SDL_PixelFormatEnumToMasks(format, &bpp, &ConversionRMask, &ConversionGMask, &ConversionBMask, &ConversionAMask)) {
                Diagnostics::SetError("SDL_PixelFormatEnumToMasks failed with error: %s", SDL_GetError());
                return false;
            }

            int frameBufferTexturePitch;
            Uint32* frameBufferTexturePixels;
            if (!SDL_LockTexture(*texture, NULL, (void**)&frameBufferTexturePixels, &frameBufferTexturePitch)) {
                Uint8 colorBuffer[4];
                int rowP = 0, rowC = 0;
                for (int row = image->Height; row; row--) {
                    Uint32* cRow = &frameBufferTexturePixels[rowC];
                    Uint8* pRow = &image->Data[rowP];

                    for (int x = image->Width; x; x--) {
                        Pixel gifColor = image->Palette[*pRow];

                        if (*pRow) {
                            *cRow = ConversionAMask;

                            colorBuffer[0] = colorBuffer[1] = colorBuffer[2] = colorBuffer[3] = (gifColor.R << 3);
                            *cRow |= (*(Uint32*)colorBuffer) & ConversionRMask;
                            colorBuffer[0] = colorBuffer[1] = colorBuffer[2] = colorBuffer[3] = (gifColor.G << 3);
                            *cRow |= (*(Uint32*)colorBuffer) & ConversionGMask;
                            colorBuffer[0] = colorBuffer[1] = colorBuffer[2] = colorBuffer[3] = (gifColor.B << 3);
                            *cRow |= (*(Uint32*)colorBuffer) & ConversionBMask;
                        }
                        else
                            *cRow = 0x00000000;

                        cRow++;
                        pRow++;
                    }

                    rowC += frameBufferTexturePitch / sizeof(Uint32);
                    rowP += image->Width;
                }
                SDL_UnlockTexture(*texture);

                return true;
            }

            return false;
        }
        bool CreateTextureFromImage(SDL_Texture** texture, Image* image) {
            // UI::Graphics::Renderer::WindowPixelFormat
            *texture = SDL_CreateTexture(UI::Graphics::Renderer::Renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, image->Width, image->Height);
            if (!*texture) {
                fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
                return false;
            }

            SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND);

            return UpdateTextureFromImage(texture, image);
        }
        bool UpdateTextureFromData(SDL_Texture** texture, Uint8* data, Pixel* palette, int width, int height, SDL_Rect* rect) {
            if (!*texture) {
                return false;
            }

            Uint32 format, ConversionRMask, ConversionGMask, ConversionBMask, ConversionAMask;
            if (SDL_QueryTexture(*texture, &format, NULL, NULL, NULL)) {
                Diagnostics::SetError("SDL_QueryTexture failed with error: %s", SDL_GetError());
                return false;
            }

            int bpp;
            if (!SDL_PixelFormatEnumToMasks(format, &bpp, &ConversionRMask, &ConversionGMask, &ConversionBMask, &ConversionAMask)) {
                Diagnostics::SetError("SDL_PixelFormatEnumToMasks failed with error: %s", SDL_GetError());
                return false;
            }

            int texturePitch;
            Uint32* texturePixels;
            if (!SDL_LockTexture(*texture, rect, (void**)&texturePixels, &texturePitch)) {
                Uint8 colorBuffer[4];
                int srcRow = 0, dstRow = 0;
                for (int row = height; row; row--) {
                    Uint32* dstPx = &texturePixels[dstRow];
                    Uint8* srcPx = &data[srcRow];

                    for (int x = width; x; x--) {
                        Pixel srcColor = palette[*srcPx];

                        if (*srcPx) {
                            *dstPx = ConversionAMask;

                            colorBuffer[0] = colorBuffer[1] = colorBuffer[2] = colorBuffer[3] = (srcColor.R << 3);
                            *dstPx |= (*(Uint32*)colorBuffer) & ConversionRMask;
                            colorBuffer[0] = colorBuffer[1] = colorBuffer[2] = colorBuffer[3] = (srcColor.G << 3);
                            *dstPx |= (*(Uint32*)colorBuffer) & ConversionGMask;
                            colorBuffer[0] = colorBuffer[1] = colorBuffer[2] = colorBuffer[3] = (srcColor.B << 3);
                            *dstPx |= (*(Uint32*)colorBuffer) & ConversionBMask;
                        }
                        else
                            *dstPx = 0x00000000;

                        dstPx++;
                        srcPx++;
                    }

                    dstRow += texturePitch / sizeof(Uint32);
                    srcRow += width;
                }
                SDL_UnlockTexture(*texture);

                return true;
            }
            return false;
        }
        bool CreateTextureFromData(SDL_Texture** texture, Uint8* data, Pixel* palette, int width, int height) {
            // UI::Graphics::Renderer::WindowPixelFormat
            *texture = SDL_CreateTexture(UI::Graphics::Renderer::Renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, width, height);
            if (!*texture) {
                fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
                return false;
            }

            SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND);

            return UpdateTextureFromData(texture, data, palette, width, height);
        }
        bool UpdateTextureFromSTBI(SDL_Texture** texture, unsigned char* data, int w, int h) {
            if (!*texture) {
                return false;
            }

            /*if (SDL_UpdateTexture(*texture, NULL, data, w * 4) < 0) {
                fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
                return false;
            }*/
            int frameBufferTexturePitch;
            Uint32* frameBufferTexturePixels;
            if (!SDL_LockTexture(*texture, NULL, (void**)&frameBufferTexturePixels, &frameBufferTexturePitch)) {
                memcpy(frameBufferTexturePixels, data, w * 4 * h);
                SDL_UnlockTexture(*texture);
            }
            else {
                return false;
            }

            return true;
        }
        bool CreateTextureFromSTBI(SDL_Texture** texture, unsigned char* data, int w, int h) {
            *texture = SDL_CreateTexture(UI::Graphics::Renderer::Renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
            if (!*texture) {
                fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
                return false;
            }

            SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND);

            if (!UpdateTextureFromSTBI(texture, data, w, h)) {
                return false;
            }

            return true;
        }
        bool CreateTextureFromFilePNG(SDL_Texture** texture, CString filename) {
            int w;
            int h;
            int comp;
            unsigned char* data = stbi_load(filename, &w, &h, &comp, STBI_rgb_alpha);
            if (!data) {
                fprintf(stderr, "stbi_load failed: %s\n", stbi_failure_reason());
                fprintf(stderr, "filename: %s\n", filename);
                return false;
            }

            // UI::Graphics::Renderer::WindowPixelFormat
            if (!CreateTextureFromSTBI(texture, data, w, h)) {
                return false;
            }

            stbi_image_free(data);

            return true; // UpdateTextureFromImage(texture, image);
        }
    }
}

// These are all Hatch namespace
namespace Graphics {
    Pixel       Palette[MAX_PALETTE_COUNT][0x100];

    View        Views[MAX_VIEWPORTS];
    ViewOutput  ViewOutputs[MAX_VIEWPORTS];
    View*       CurrentView;
    Uint32      CurrentViewIndex;

    DrawGroup   DrawGroups[MAX_DRAWGROUPS];
    Uint32      CurrentDrawGroupIndex;

    bool        DrawToScreen = false;

    Vector2     DrawMinPos;
    Vector2     DrawMaxPos;

    SDL_Texture** TileImageData;
    SDL_Texture** TileCollisionImageData;
    int DrawCollision = 0;

    void Init() {
        memset(Views, 0, sizeof(Views));
        memset(ViewOutputs, 0, sizeof(ViewOutputs));
        memset(DrawGroups, 0, sizeof(DrawGroups));

        DrawToScreen = false;

        // Create views
        CurrentViewIndex = 0;
        CurrentView = &Views[CurrentViewIndex];

        View_SetSize(0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

        // Create view outputs
        ViewOutput* viewOutput = &ViewOutputs[0];
        viewOutput->Active = true;
        viewOutput->ViewIndex = 0;
        viewOutput->ScaleType = VOSCALE_RESIZE_TO_SCREEN;
    }

    void ResetHighlightBounds(Vector2 pos) {
        DrawMinPos =
            DrawMaxPos = pos;
    }
    void SetHighlightBounds(SDL_Rect dst) {
        DrawMinPos = Vector2(M_MIN(dst.x, DrawMinPos.X.Full), M_MIN(dst.y, DrawMinPos.Y.Full));
        DrawMaxPos = Vector2(M_MAX(dst.x + dst.w, DrawMaxPos.X.Full), M_MAX(dst.y + dst.h, DrawMaxPos.Y.Full));
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

        // Memory::Alloc((void**)&v->Pixels, v->Width * v->Height * sizeof(Pixel), Memory::MEMPOOL_VIEWS, true);
    }
    void View_GetSize(int viewIndex, int* width, int* height) {
        View* v = &Views[viewIndex];
        if (width) *width = v->Width;
        if (height) *height = v->Height;
    }

    void DrawSprite(Resource sprite, int animation, int frame, Vector2* position) {
        if (sprite < 0 || sprite >= MAX_SPRITES)
            return;

        Resources::ResSprite* resSprite = &Resources::ResourceSprites[sprite];
        Frame* frameData = &resSprite->SpriteData.Frames[resSprite->SpriteData.Animations[animation].StartFrameIndex + frame];
        if (frameData->Image < 0 || frameData->Image >= MAX_IMAGES)
            return;

        // Image* image = &Resources::ResourceImages[frameData->Image].ImageData;
        SDL_Texture* texImage = Resources::ImageTextures[frameData->Image];

        // int blendFlag = GameLinker::CurrentEntity->BlendFlag;
        // int opacity = GameLinker::CurrentEntity->Opacity;

        int flipFlag = 0;
        int rotation = 0;
        Vector2 scale = Vector2(0x10000, 0x10000);
        if (GameLinker::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_FLIP) {
            flipFlag = GameLinker::CurrentEntity->FlipFlag;
        }
        if (GameLinker::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_ROTATE) {
            rotation = GameLinker::CurrentEntity->Rotation;

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
        if (GameLinker::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_SCALE) {
            scale = GameLinker::CurrentEntity->Scale;
        }

        if (GameLinker::CurrentEntity->TransformFlag & 6) {
            scale.X.Full >>= 8;
            scale.Y.Full >>= 8;
            /*DrawSpriteImageTransformed(image,
                position->X.Whole, position->Y.Whole,
                (frameData->OffsetX * scale.X.Full) >> 8, (frameData->OffsetY * scale.Y.Full) >> 8,
                (frameData->Width * scale.X.Full) >> 8, (frameData->Height * scale.Y.Full) >> 8,
                frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height,
                flipFlag, rotation, blendFlag, opacity);*/
        }
        else {
            SDL_Rect src;
            SDL_Rect dst;

            src = { frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height };

            switch (flipFlag) {
            case FLIPXY_NONE:
                dst = {
                    position->X.Whole + frameData->OffsetX,
                    position->Y.Whole + frameData->OffsetY,
                    frameData->Width, frameData->Height };

                SetHighlightBounds(dst);

                if (!DrawToScreen) {
                    dst.x -= Graphics::CurrentView->X;
                    dst.y -= Graphics::CurrentView->Y;
                }
                SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, texImage, &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
                break;
            case FLIPXY_X:
                dst = {
                    position->X.Whole - frameData->OffsetX - frameData->Width,
                    position->Y.Whole + frameData->OffsetY,
                    frameData->Width, frameData->Height };

                SetHighlightBounds(dst);

                if (!DrawToScreen) {
                    dst.x -= Graphics::CurrentView->X;
                    dst.y -= Graphics::CurrentView->Y;
                }
                SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, texImage, &src, &dst, 0.0, NULL, SDL_FLIP_HORIZONTAL);
                break;
            case FLIPXY_Y:
                dst = {
                    position->X.Whole + frameData->OffsetX,
                    position->Y.Whole - frameData->OffsetY - frameData->Height,
                    frameData->Width, frameData->Height };

                SetHighlightBounds(dst);

                if (!DrawToScreen) {
                    dst.x -= Graphics::CurrentView->X;
                    dst.y -= Graphics::CurrentView->Y;
                }
                SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, texImage, &src, &dst, 0.0, NULL, SDL_FLIP_VERTICAL);
                break;
            case FLIPXY_XY:
                dst = {
                    position->X.Whole - frameData->OffsetX - frameData->Width,
                    position->Y.Whole - frameData->OffsetY - frameData->Height,
                    frameData->Width, frameData->Height };

                SetHighlightBounds(dst);

                if (!DrawToScreen) {
                    dst.x -= Graphics::CurrentView->X;
                    dst.y -= Graphics::CurrentView->Y;
                }
                SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, texImage, &src, &dst, 0.0, NULL, (SDL_RendererFlip)(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL));
                break;
            }
        }
    }
    void DrawAnimation(Animator* animator, Vector2* position) {
        if (!animator || !animator->StartFrame)
            return;

        // Frame* frameData = &animator->StartFrame[animator->FrameIndex];
        auto resSprite = &Resources::ResourceSprites[animator->PrevAnimationIndex];
        Frame* frameData = &resSprite->SpriteData.Frames[resSprite->SpriteData.Animations[animator->AnimationIndex].StartFrameIndex + animator->FrameIndex];
        // Image* image = &Resources::ResourceImages[frameData->Image].ImageData;
        SDL_Texture* texImage = Resources::ImageTextures[frameData->Image];

        // int blendFlag = GameLinker::CurrentEntity->BlendFlag;
        // int opacity = GameLinker::CurrentEntity->Opacity;

        int flipFlag = 0;
        int rotation = 0;
        Vector2 scale = Vector2(0x10000, 0x10000);
        if (GameLinker::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_FLIP) {
            flipFlag = GameLinker::CurrentEntity->FlipFlag;
        }
        if (GameLinker::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_ROTATE) {
            rotation = GameLinker::CurrentEntity->Rotation;

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
        if (GameLinker::CurrentEntity->TransformFlag & TRANSFORM_ALLOW_SCALE) {
            scale = GameLinker::CurrentEntity->Scale;
        }

        if (GameLinker::CurrentEntity->TransformFlag & 6) {
            scale.X.Full >>= 8;
            scale.Y.Full >>= 8;
            /*DrawSpriteImageTransformed(image,
                position->X.Whole, position->Y.Whole,
                (frameData->OffsetX * scale.X.Full) >> 8, (frameData->OffsetY * scale.Y.Full) >> 8,
                (frameData->Width * scale.X.Full) >> 8, (frameData->Height * scale.Y.Full) >> 8,
                frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height,
                flipFlag, rotation, blendFlag, opacity);*/
        }
        else {
            SDL_Rect src;
            SDL_Rect dst;

            src = { frameData->SourceX, frameData->SourceY, frameData->Width, frameData->Height };

            switch (flipFlag) {
            case FLIPXY_NONE:
                dst = {
                    position->X.Whole + frameData->OffsetX,
                    position->Y.Whole + frameData->OffsetY,
                    frameData->Width, frameData->Height };

                SetHighlightBounds(dst);

                if (!DrawToScreen) {
                    dst.x -= Graphics::CurrentView->X;
                    dst.y -= Graphics::CurrentView->Y;
                }
                SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, texImage, &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
                break;
            case FLIPXY_X:
                dst = {
                    position->X.Whole - frameData->OffsetX - frameData->Width,
                    position->Y.Whole + frameData->OffsetY,
                    frameData->Width, frameData->Height };

                SetHighlightBounds(dst);

                if (!DrawToScreen) {
                    dst.x -= Graphics::CurrentView->X;
                    dst.y -= Graphics::CurrentView->Y;
                }
                SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, texImage, &src, &dst, 0.0, NULL, SDL_FLIP_HORIZONTAL);
                break;
            case FLIPXY_Y:
                dst = {
                    position->X.Whole + frameData->OffsetX,
                    position->Y.Whole - frameData->OffsetY - frameData->Height,
                    frameData->Width, frameData->Height };

                SetHighlightBounds(dst);

                if (!DrawToScreen) {
                    dst.x -= Graphics::CurrentView->X;
                    dst.y -= Graphics::CurrentView->Y;
                }
                SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, texImage, &src, &dst, 0.0, NULL, SDL_FLIP_VERTICAL);
                break;
            case FLIPXY_XY:
                dst = {
                    position->X.Whole - frameData->OffsetX - frameData->Width,
                    position->Y.Whole - frameData->OffsetY - frameData->Height,
                    frameData->Width, frameData->Height };

                SetHighlightBounds(dst);

                if (!DrawToScreen) {
                    dst.x -= Graphics::CurrentView->X;
                    dst.y -= Graphics::CurrentView->Y;
                }
                SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, texImage, &src, &dst, 0.0, NULL, (SDL_RendererFlip)(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL));
                break;
            }
        }
    }
    void DrawImage(Resource image, Vector2* position) {
        if (image < 0 || image >= MAX_IMAGES)
            return;

        Resources::ResImage resImage = Resources::ResourceImages[image];
        SDL_Texture* texImage = Resources::ImageTextures[image];

        SDL_Rect dst { position->X.Whole, position->Y.Whole, resImage.ImageData.Width, resImage.ImageData.Height };

        SetHighlightBounds(dst);

        if (!DrawToScreen) {
            dst.x -= Graphics::CurrentView->X;
            dst.y -= Graphics::CurrentView->Y;
        }
        SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, texImage, NULL, &dst, 0.0, NULL, SDL_FLIP_NONE);
    }
    void DrawSpriteText(String* string, Vector2* position, Resource spriteIndex, int animIndex, int startIndex, int endIndex, int alignment, int spacing, Vector2* offsets) { }

    void DrawRectangle(Subpixels x, Subpixels y, Subpixels w, Subpixels h, Color color, int blendFlag) {
        if (!DrawToScreen) {
            x.Whole -= CurrentView->X;
            y.Whole -= CurrentView->Y;
        }
        SDL_Rect dst = { x.Whole, y.Whole, w.Whole, h.Whole };
        switch (blendFlag) {
        case BLEND_NONE:
            SDL_SetRenderDrawColor(UI::Graphics::Renderer::Renderer, color.R, color.G, color.B, 255);
            break;
        case BLEND_TRANSPARENT:
            SDL_SetRenderDrawColor(UI::Graphics::Renderer::Renderer, color.R, color.G, color.B, color.A);
            break;
        }
        SDL_RenderFillRect(UI::Graphics::Renderer::Renderer, &dst);
    }
    void DrawLine(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Color color, int blendFlag) {
        if (!DrawToScreen) {
            x1.Whole -= CurrentView->X;
            y1.Whole -= CurrentView->Y;
            x2.Whole -= CurrentView->X;
            y2.Whole -= CurrentView->Y;
        }
        switch (blendFlag) {
        case BLEND_NONE:
            SDL_SetRenderDrawColor(UI::Graphics::Renderer::Renderer, color.R, color.G, color.B, 255);
            break;
        case BLEND_TRANSPARENT:
            SDL_SetRenderDrawColor(UI::Graphics::Renderer::Renderer, color.R, color.G, color.B, color.A);
            break;
        }
        SDL_RenderDrawLine(UI::Graphics::Renderer::Renderer, x1.Whole, y1.Whole, x2.Whole, y2.Whole);
    }
    void DrawCircle(Subpixels x, Subpixels y, Subpixels radius, Color color, int blendFlag) { }
    void DrawRing(Subpixels x, Subpixels y, Subpixels innerRadius, Subpixels outerRadius, Color color, int blendFlag) { }
    void DrawEllipse(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Color color, int blendFlag) { }
    void DrawTriangle(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Subpixels x3, Subpixels y3, Color color, int blendFlag) { }
    void DrawPolygonBlend(Vector2* positions, Color* colors, int vertexCount, int blendFlag) { }
    void DrawPolygon(Vector2* positions, Color color, int vertexCount, int blendFlag) { }

    void DrawTile(Subpixels x, Subpixels y, Tile tile) {
        const int columnMask = 63;
        // const int columnCount = 64;
        const int columnBitshift = 6;

        if (!DrawToScreen) {
            x.Whole -= CurrentView->X;
            y.Whole -= CurrentView->Y;
        }

        if (tile != TILE_EMPTY) {
            // SDL_Rect src = { 0, tile.ID * 16, 16, 16 };
            SDL_Rect src = { (tile.ID & columnMask) << 4, (tile.ID >> columnBitshift) << 4, 16, 16 };
            SDL_Rect dst = { x.Whole, y.Whole, 16, 16 };

            SetHighlightBounds(dst);

            int tileFID = tile.FlipX | (tile.FlipY << 1);
            SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, TileImageData[tileFID], &src, &dst, 0.0, NULL, SDL_FLIP_NONE);

            if (DrawCollision > 0) {
                int plane = DrawCollision - 1;
                int planeSolidity = plane ? tile.PlaneB : tile.PlaneA;
                int imgIndex = tileFID | (plane << 2);

                switch (planeSolidity) {
                case SOLID_NONE:
                    break;
                case SOLID_PLATFORM:
                    SDL_SetTextureColorMod(TileCollisionImageData[imgIndex], 0xFF, 0xFF, 0x00);
                    SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, TileCollisionImageData[imgIndex], &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
                    break;
                case SOLID_FALLTHROUGH:
                    SDL_SetTextureColorMod(TileCollisionImageData[imgIndex], 0xFF, 0x00, 0x00);
                    SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, TileCollisionImageData[imgIndex], &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
                    break;
                case SOLID_FULL:
                    SDL_SetTextureColorMod(TileCollisionImageData[imgIndex], 0xFF, 0xFF, 0xFF);
                    SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, TileCollisionImageData[imgIndex], &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
                    break;
                }
            }
        }
    }

    void LayerDraw_Editor(Layer* layer) {
        bool temp = DrawToScreen;

        DrawToScreen = false;
        int tileStartX = CurrentView->X / 16;
        int tileStartY = CurrentView->Y / 16;
        int tileEndX = tileStartX + CurrentView->Width / 16 + 3;
        int tileEndY = tileStartY + CurrentView->Height / 16 + 3;

        tileStartX = M_MAX(tileStartX, 0);
        tileStartY = M_MAX(tileStartY, 0);
        tileEndX = M_MIN(tileEndX, (int)layer->Width);
        tileEndY = M_MIN(tileEndY, (int)layer->Height);

        int tileX, tileY;

        tileY = tileStartY * 16;
        for (int y = tileStartY; y < tileEndY; y++) {
            Tile* tile = &layer->Tiles[tileStartX + (y << layer->WidthInBits)];
            tileX = tileStartX * 16;
            for (int x = tileStartX; x < tileEndX; x++) {
                DrawTile(tileX << 16, tileY << 16, *tile);
                tile++;
                tileX += 16;
            }
            tileY += 16;
        }

        DrawToScreen = temp;
    }
    void DrawAll_Editor(int layerCount) {
        CurrentView = Views;
        CurrentViewIndex = 0;

        // Clear DrawGroup Layer lists
        for (CurrentDrawGroupIndex = 0; CurrentDrawGroupIndex < MAX_DRAWGROUPS; CurrentDrawGroupIndex++)
            DrawGroups[CurrentDrawGroupIndex].LayerCount = 0;

        // Add layers to DrawGroups
        for (int layerIndex = 0; layerIndex < layerCount; layerIndex++) {
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

                GameLinker::CurrentEntity = &Scene::EntitySlots[slotID];
                // if (GameLinker::CurrentEntity->CanDraw) {
                    if (GameLinker::CurrentEntity->ClassID > 0) {
                        auto onEditorDraw = GameLinker::ClassList[Scene::ClassIndexList[GameLinker::CurrentEntity->ClassID]].onStageDraw;
                        if (onEditorDraw) {
                            onEditorDraw();
                        }
                        else {
                            // Draw default
                        }
                    }
                // }
            }

            // Draw Layers
            for (int i = 0; i < drawGroup->LayerCount; i++) {
                Layer* layer = &Scene::Layers[drawGroup->LayerIndices[i]];

                LayerDraw_Editor(layer);
            }
        }
    }
}

namespace Diagnostics {
    char ErrorString[1024];

    void SetError(CString text, ...) {
        va_list args;
        va_start(args, text);

        vprintf(text, args);
        vsnprintf(ErrorString, 1024, text, args);
        printf("\n");

        va_end(args);
    }
}

namespace Scene {
    Layer* Layers = NULL;
    Uint8* TileImageData = NULL;
    Uint32 Frame;

    Entity* CurrentEntity = NULL;
    EntitySlot* EntitySlots = NULL;

    Uint16* ClassIndexList = NULL;
    Uint32  ClassIndexCount = 0;
};

namespace Resources {
    SDL_Texture* ImageTextures[MAX_IMAGES];

    bool PlatformInit() {
        memset(ImageTextures, 0, sizeof(ImageTextures));
        return true;
    }
    void PlatformDispose() {

    }

    Resource LoadImageWrapper(CString filename, int unloadPolicy) {
        Resource result = LoadImage(filename, unloadPolicy);
        if (result != -1) {
            if (!ImageTextures[result]) {
                if (!Studio::Textures::CreateTextureFromImage(&ImageTextures[result], &ResourceImages[result].ImageData)) {
                    return -1;
                }
                if (!ImageTextures[result]) {
                    return -1;
                }
            }
        }
        return result;
    }
    Resource LoadSpriteWrapper(CString filename, int unloadPolicy) {
        if (unloadPolicy < 0 || unloadPolicy > 2)
            return -1;

        Hash name = MD5_HashString(filename);

        int emptyIndex = -1;
        for (int index = 0; index < MAX_SPRITES; index++) {
            ResSprite* resource = &ResourceSprites[index];
            if (emptyIndex < 0 && !resource->UnloadPolicy)
                emptyIndex = index;

            if (resource->UnloadPolicy && resource->Name == name) {
                // Upgrade unload policy if needed.
                resource->UnloadPolicy = M_MAX(resource->UnloadPolicy, unloadPolicy);
                return index;
            }
        }

        if (emptyIndex > -1) {
            Resource sheets[16];
            ResSprite* resource = &ResourceSprites[emptyIndex];
            resource->Name = name;

            PREFIX_FILENAME(filename, "Sprites/");

            Stream* stream = ResourceStream::New(BufferString);
            if (stream) {
                char streamStringBuffer[256];
                if (stream->ReadUInt32() == 0x00525053) {
                    int totalFrameCount = stream->ReadInt32();
                    Memory::Alloc(&resource->SpriteData.Frames, totalFrameCount * sizeof(Frame), Memory::MEMPOOL_STAGE, false);

                    int totalSheetCount = stream->ReadByte();
                    for (int i = 0; i < totalSheetCount; i++) {
                        stream->ReadHeaderedString(streamStringBuffer);
                        sheets[i] = LoadImageWrapper(streamStringBuffer, unloadPolicy);
                    }

                    int hitboxCount = stream->ReadByte();
                    for (int i = 0; i < hitboxCount; i++) {
                        stream->Skip(stream->ReadByte()); // Skip over hitbox names
                    }

                    int animationCount = stream->ReadUInt16();
                    Memory::Alloc(&resource->SpriteData.Animations, animationCount * sizeof(Animation), Memory::MEMPOOL_STAGE, false);
                    resource->SpriteData.AnimationCount = animationCount;

                    Frame* currentFrame = resource->SpriteData.Frames;
                    Animation* currentAnimation = resource->SpriteData.Animations;

                    for (int a = 0; a < animationCount; a++) {
                        stream->ReadHeaderedString(streamStringBuffer);
                        currentAnimation->Name = MD5_HashString(streamStringBuffer);

                        currentAnimation->FrameCount = stream->ReadUInt16();
                        currentAnimation->Speed = stream->ReadUInt16();
                        currentAnimation->LoopFrameIndex = stream->ReadByte();

                        // 0: No rotation
                        // 1: Full rotation
                        // 2: Round to 45 degrees
                        // 3: Round to 90 degrees
                        // 4: Round to 180 degrees
                        // 5: Player rotation using extra frames
                        currentAnimation->RotationFlag = stream->ReadByte(); // Rotation Flags

                        currentAnimation->StartFrameIndex = (int)(currentFrame - resource->SpriteData.Frames);
                        for (int i = 0; i < currentAnimation->FrameCount; i++) {
                            currentFrame->Image = sheets[stream->ReadByte()];

                            currentFrame->Duration = stream->ReadInt16();
                            currentFrame->ID = stream->ReadUInt16();
                            currentFrame->SourceX = stream->ReadUInt16();
                            currentFrame->SourceY = stream->ReadUInt16();
                            currentFrame->Width = stream->ReadUInt16();
                            currentFrame->Height = stream->ReadUInt16();
                            currentFrame->OffsetX = stream->ReadInt16();
                            currentFrame->OffsetY = stream->ReadInt16();

                            if (hitboxCount) {
                                for (int h = 0; h < hitboxCount; h++) {
                                    currentFrame->Hitboxes[h].Left = stream->ReadInt16();
                                    currentFrame->Hitboxes[h].Top = stream->ReadInt16();
                                    currentFrame->Hitboxes[h].Right = stream->ReadInt16();
                                    currentFrame->Hitboxes[h].Bottom = stream->ReadInt16();
                                }
                            }
                            currentFrame++;
                        }
                        currentAnimation++;
                    }
                }

                stream->Close();
            }
            else {
                fprintf(stderr, "Couldn't open stream for %s!\n", filename);
                return -1;
            }

            resource->UnloadPolicy = unloadPolicy;
        }
        return emptyIndex;
    }
}

namespace GameLinker {
    HatchFunctionSet HatchFuncs;
    ServicesFunctionSet ServiceFuncs;
    GameState State;

    Class   ClassList[MAX_CLASSES];
    CString ClassNames[MAX_CLASSES];
    int   ClassCount = 0;

    void* GameLogicSharedObject = NULL;

    Entity* CurrentEntity = NULL;

    bool Animator_Set(Animator* animator, Resource sprite, int animationIndex, int frameIndex, bool resetFrame) {
        if (!animator)
            return false;

        if (sprite < 0 || sprite >= MAX_SPRITES) {
            animator->StartFrame = NULL;
            return false;
        }

        Resources::ResSprite* resSprite = &Resources::ResourceSprites[sprite];
        if (animationIndex >= resSprite->SpriteData.AnimationCount)
            return false;

        auto animationPtr = &Resources::ResourceSprites[sprite].SpriteData.Animations[animationIndex];
        auto startFramePtr = &resSprite->SpriteData.Frames[resSprite->SpriteData.Animations[animationIndex].StartFrameIndex];
        if (animator->StartFrame == startFramePtr && !resetFrame)
            return false;

        animator->StartFrame = startFramePtr;
        animator->Time = 0;
        animator->FrameIndex = frameIndex;
        animator->FrameCount = animationPtr->FrameCount;
        animator->FrameDuration = startFramePtr[frameIndex].Duration;
        animator->Speed = animationPtr->Speed;
        animator->RotationFlag = animationPtr->RotationFlag;
        animator->FrameLoop = animationPtr->LoopFrameIndex;
        animator->PrevAnimationIndex = sprite;
        animator->AnimationIndex = animationIndex;
        return true;
    }
    bool Animator_Set3D(Animator* animator, Resource mesh, int animationSpeed, int frameLoopIndex, int frameIndex, bool resetFrame) {
        if (!animator)
            return false;

        if (mesh < 0 || mesh >= MAX_MESHES) {
            animator->StartFrame = NULL;
            return false;
        }

        if (animator->AnimationIndex != mesh || resetFrame) {
            animator->StartFrame = (Frame*)1;
            animator->Time = 0;
            animator->FrameIndex = frameIndex;
            animator->FrameCount = Resources::ResourceMeshes[mesh].MeshData.FrameCount;
            animator->FrameDuration = 256;
            animator->Speed = animationSpeed;
            animator->FrameLoop = frameLoopIndex;
            animator->PrevAnimationIndex = animator->AnimationIndex;
            animator->AnimationIndex = mesh;

            return true;
        }

        return false;
    }
    void Animator_Update(Animator* animator) {
        if (animator) {
            if (animator->StartFrame) {
                animator->Time += animator->Speed;
                // Mesh Animation
                if (animator->StartFrame == (Frame*)1) {
                    while (animator->Time > animator->FrameDuration) {
                        animator->FrameIndex++;
                        animator->Time -= animator->FrameDuration;
                        if (animator->FrameIndex >= animator->FrameCount)
                            animator->FrameIndex = animator->FrameLoop;
                    }
                }
                // Sprite Animation
                else {
                    while (animator->Time > animator->FrameDuration) {
                        animator->FrameIndex++;
                        animator->Time -= animator->FrameDuration;
                        if (animator->FrameIndex >= animator->FrameCount)
                            animator->FrameIndex = animator->FrameLoop;
                        animator->FrameDuration = animator->StartFrame[animator->FrameIndex].Duration;
                    }
                }
            }
        }
    }

    void Init() {
        Math::SetupMathTables();

        memset(&HatchFuncs, 0, sizeof(HatchFuncs));
        memcpy(&ServiceFuncs, &Services::Service, sizeof(ServiceFuncs));

        HatchFuncs.AllocateGlobals = [](Globals** globals, size_t size) -> void {
            // Memory::Alloc((void**)globals, size, Memory::MEMPOOL_STAGE, true);
        };

        HatchFuncs.Class.Add = Classes::Add;
        HatchFuncs.Class.SetupAttribute = Classes::SetupAttribute;
        HatchFuncs.Class.AddEnumValue = Classes::AddEnumValue;
        HatchFuncs.Class.CreateGlobalClass = [](CString className, void** staticObjectPtr, size_t staticObjectSize, void (*onStaticConstructor)(void* staticObject)) -> void {
            // I don't caare
        };

        HatchFuncs.Draw.Sprite = Graphics::DrawSprite;
        HatchFuncs.Draw.Animation = Graphics::DrawAnimation;
        HatchFuncs.Draw.Image = Graphics::DrawImage;
        HatchFuncs.Draw.SpriteText = Graphics::DrawSpriteText;

        HatchFuncs.Draw.Rectangle = Graphics::DrawRectangle;
        HatchFuncs.Draw.Triangle = Graphics::DrawTriangle;
        HatchFuncs.Draw.Line = Graphics::DrawLine;
        HatchFuncs.Draw.Circle = Graphics::DrawCircle;
        // HatchFuncs.Draw.CircleStroke = Graphics::DrawCircleStroke;
        HatchFuncs.Draw.Ring = Graphics::DrawRing;
        HatchFuncs.Draw.Ellipse = Graphics::DrawEllipse;

        HatchFuncs.Math.Sin256 = [](int n) -> int { n &= 0xFF; return Math::SinTbl_0x100[n]; };
        HatchFuncs.Math.Cos256 = [](int n) -> int { n &= 0xFF; return Math::CosTbl_0x100[n]; };
        HatchFuncs.Math.Tan256 = [](int n) -> int { n &= 0xFF; return Math::TanTbl_0x100[n]; };
        HatchFuncs.Math.Asin256 = [](int n) -> int { n &= 0xFF; return Math::ASinTbl_0x100[n]; };
        HatchFuncs.Math.Acos256 = [](int n) -> int { n &= 0xFF; return Math::ACosTbl_0x100[n]; };
        HatchFuncs.Math.Atan256 = Math::ATan;

        HatchFuncs.Math.Sin512 = [](int n) -> int { n &= 0x1FF; return Math::SinTbl_0x200[n]; };
        HatchFuncs.Math.Cos512 = [](int n) -> int { n &= 0x1FF; return Math::CosTbl_0x200[n]; };
        HatchFuncs.Math.Tan512 = [](int n) -> int { n &= 0x1FF; return Math::TanTbl_0x200[n]; };
        HatchFuncs.Math.Asin512 = [](int n) -> int { n &= 0x1FF; return Math::ASinTbl_0x200[n]; };
        HatchFuncs.Math.Acos512 = [](int n) -> int { n &= 0x1FF; return Math::ACosTbl_0x200[n]; };

        HatchFuncs.Math.Sin1024 = [](int n) -> int { n &= 0x3FF; return Math::SinTbl_0x400[n]; };
        HatchFuncs.Math.Cos1024 = [](int n) -> int { n &= 0x3FF; return Math::CosTbl_0x400[n]; };
        HatchFuncs.Math.Tan1024 = [](int n) -> int { n &= 0x3FF; return Math::TanTbl_0x400[n]; };
        HatchFuncs.Math.Asin1024 = [](int n) -> int { n &= 0x3FF; return Math::ASinTbl_0x400[n]; };
        HatchFuncs.Math.Acos1024 = [](int n) -> int { n &= 0x3FF; return Math::ACosTbl_0x400[n]; };

        HatchFuncs.Math.Sqrt = Math::Sqrt;
        HatchFuncs.Math.Distance = [](int x1, int y1, int x2, int y2) -> int {
            return (int)sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
        };
        HatchFuncs.Math.Abs = [](int n) -> int { return M_ABS(n); };
        HatchFuncs.Math.Min = [](int a, int b) -> int { return M_MIN(a, b); };
        HatchFuncs.Math.Max = [](int a, int b) -> int { return M_MAX(a, b); };
        HatchFuncs.Math.Clamp = [](int n, int min, int max) -> int { return M_CLAMP(n, min, max); };
        HatchFuncs.Math.GetRandom = Math::RandomRange;
        HatchFuncs.Math.SetRandomSeed = Math::RandomSetSeed;
        HatchFuncs.Math.GetRandomSeeded = Math::RandomRangeSeeded;

        HatchFuncs.Resources.LoadSprite = Resources::LoadSpriteWrapper;
        HatchFuncs.Resources.LoadImage = Resources::LoadImageWrapper;

        HatchFuncs.Animator.Set = Animator_Set;
        HatchFuncs.Animator.Set3D = Animator_Set3D;
        HatchFuncs.Animator.Update = Animator_Update;
        // HatchFuncs.Resources.LoadMesh = Resources::LoadMesh;
        // HatchFuncs.Resources.LoadView3D = Resources::LoadView3D;
        // HatchFuncs.Resources.LoadSound = Resources::LoadSound;
    }
    void Load(const char* projectFolder) {
        ClassCount = 0;
        Classes::LinkedClasses.Clear();

        if (GameLogicSharedObject) {
            SDL_UnloadObject(GameLogicSharedObject);
            GameLogicSharedObject = NULL;
        }

        LinkData linkData;
        linkData.HatchFuncs = &HatchFuncs;
        linkData.ServiceFuncs = &ServiceFuncs;
        linkData.CurrentEntityPtr = &CurrentEntity;
        linkData.GameStatePtr = &State;

        LinkExternalGameLogic(&linkData, projectFolder);
    }
}

namespace Classes {
    ClassAttribute ClassAttributes[0x100];
    int            ClassAttributeCount = 0;

    List<LinkedClass*> LinkedClasses;
    LinkedClass* FocusedLinkedClass = NULL;

    ClassAttribute::ClassAttribute(CString name) {
        size_t len = strlen(name);
        NameString = (char*)malloc(len + 1);
        if (NameString) {
            strcpy(NameString, name);
            NameString[len] = 0;
        }

        Name = MD5_HashString(name);

        new (&EnumPairs) List<EnumPair>();
    }

    void Add(CString className, void** staticObjectPtr, size_t entitySize, size_t staticObjectSize, void (*onStageLoad)(), void (*onEditorLoad)(), void (*onStaticUpdate)(), void (*onCreate)(CreateFlag flag), void (*onUpdate)(), void (*onUpdateLate)(), void (*onStageDraw)(), void (*onEditorDraw)(), void (*onSetup)(), void (*onStaticConstructor)(void* staticObject)) {
        if (GameLinker::ClassCount >= 4096)
            return;

        auto objectClassName = &GameLinker::ClassNames[GameLinker::ClassCount];
        Class* objectClass = &GameLinker::ClassList[GameLinker::ClassCount++];

        LinkedClass* linkedClass = new LinkedClass();
        linkedClass->ObjectClass = objectClass;
        LinkedClasses.Add(linkedClass);

        FocusedLinkedClass = linkedClass;

        printf("Interactive Class: %s\n", className);

        *objectClassName = className;

        objectClass->Name = MD5_HashString(className);

        objectClass->StaticObjectPtr = staticObjectPtr;
        objectClass->EntitySize = entitySize;
        objectClass->StaticObjectSize = staticObjectSize;

        objectClass->onStageLoad = onStageLoad;
        objectClass->onEditorLoad = onEditorLoad;
        objectClass->onStaticUpdate = onStaticUpdate;
        objectClass->onCreate = onCreate;
        objectClass->onUpdate = onUpdate;
        objectClass->onUpdateLate = onUpdateLate;
        objectClass->onStageDraw = onStageDraw;
        objectClass->onEditorDraw = onEditorDraw;
        objectClass->onSetup = onSetup;
        objectClass->onStaticConstructor = onStaticConstructor;
    }
    void SetupAttribute(int attributeType, CString name, size_t offset) {
        if (ClassAttributeCount >= 0x100)
            return;

        FocusedLinkedClass->Properties.Add(ClassAttribute {});

        ClassAttribute* attr = new (&FocusedLinkedClass->Properties[FocusedLinkedClass->Properties.Count() - 1]) ClassAttribute(name);
        attr->StructOffset = offset;
        attr->AttributeType = attributeType;
    }
    void AddEnumValue(CString name, int value) {
        ClassAttribute* attr = &FocusedLinkedClass->Properties[FocusedLinkedClass->Properties.Count() - 1];
        attr->EnumPairs.Add({ name, value });
    }
}

namespace Game {
    GameState State;
    bool Running = true;
}

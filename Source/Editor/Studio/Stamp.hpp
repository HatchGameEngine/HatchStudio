#pragma once

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>

struct Stamp {
    int Width;
    int Height;
    Tile Data[];

    static Stamp* FromLayer(Layer* layer, int x, int y, int w, int h) {
        // Bound limits, but don't bound yourself
        if (x < 0) {
            w += x;
            x = 0;
        }
        if (y < 0) {
            h += y;
            y = 0;
        }
        w = M_MIN(w, (int)layer->Width - x);
        h = M_MIN(h, (int)layer->Height - y);

        if (!w || !h)
            return NULL;

        Stamp* stamp = (Stamp*)malloc(sizeof(Stamp) + sizeof(Tile) * w * h);
        if (!stamp)
            return NULL;

        stamp->Width = w;
        stamp->Height = h;

        Tile* tileRow = &layer->Tiles[x + (y << layer->WidthInBits)];
        Tile* tileDst = &stamp->Data[0];

        // Copy
        for (int ty = 0; ty < h; ty++) {
            for (int tx = 0; tx < w; tx++) {
                *(tileDst++) = tileRow[tx];
            }
            tileRow += layer->DataWidth;
        }

        return stamp;
    }
    static void   ToLayer(Stamp* stamp, Layer* layer, int x, int y, bool doEmptyTileWrite) {
        int w = stamp->Width, h = stamp->Height;

        int srcx = 0;
        int srcy = 0;

        // Bound limits
        if (x < 0) {
            srcx = -x;
            w -= srcx;
            x = 0;
        }
        if (y < 0) {
            srcy = -y;
            h -= srcy;
            y = 0;
        }
        w = M_MIN(w, (int)layer->Width - x);
        h = M_MIN(h, (int)layer->Height - y);

        if (!w || !h)
            return;

        Tile* tileRow = &layer->Tiles[x + (y << layer->WidthInBits)];
        Tile* tileSrc = &stamp->Data[srcx + (srcy * stamp->Width)];

        // Copy
        for (int ty = 0; ty < h; ty++) {
            for (int tx = 0; tx < w; tx++) {
                if (*tileSrc != TILE_EMPTY || doEmptyTileWrite)
                    tileRow[tx] = *tileSrc;
                tileSrc++;
            }
            tileSrc += stamp->Width - w;
            tileRow += layer->DataWidth;
        }
    }

    static Stamp* Clone(Stamp* stamp) {
        Stamp* stampNew = (Stamp*)malloc(sizeof(Stamp) + sizeof(Tile) * stamp->Width * stamp->Height);
        if (!stampNew)
            return NULL;

        memcpy(stampNew, stamp, sizeof(Stamp) + sizeof(Tile) * stamp->Width * stamp->Height);
        return stampNew;
    }
    static Stamp* FromRepeatTile(Tile tile, int w, int h) {
        Stamp* stamp = (Stamp*)malloc(sizeof(Stamp) + sizeof(Tile) * w * h);
        if (!stamp)
            return NULL;

        stamp->Width = w;
        stamp->Height = h;

        Tile* tileDst = &stamp->Data[0];

        // Copy
        for (int ty = 0; ty < h; ty++) {
            for (int tx = 0; tx < w; tx++) {
                *(tileDst++) = tile;
            }
        }

        return stamp;
    }
    static Stamp* FromTileArray(Tile* tile, int w, int h) {
        Stamp* stamp = (Stamp*)malloc(sizeof(Stamp) + sizeof(Tile) * w * h);
        if (!stamp)
            return NULL;

        stamp->Width = w;
        stamp->Height = h;

        Tile* tileDst = &stamp->Data[0];

        // Copy
        for (int ty = 0; ty < h; ty++) {
            for (int tx = 0; tx < w; tx++) {
                *(tileDst++) = *(tile++);
            }
        }

        return stamp;
    }
    static Stamp* CreateEmpty(int w, int h) {
        Stamp* stamp = (Stamp*)malloc(sizeof(Stamp) + sizeof(Tile) * w * h);
        if (!stamp)
            return NULL;

        stamp->Width = w;
        stamp->Height = h;

        Tile* tileDst = &stamp->Data[0];

        // Copy
        for (int ty = 0; ty < h; ty++) {
            for (int tx = 0; tx < w; tx++) {
                *(tileDst++) = TILE_EMPTY;
            }
        }

        return stamp;
    }

    static Stamp* FromStampFlipped(Stamp* stamp, bool flipHorizontal, bool flipVertical) {
        Stamp* stampNew = (Stamp*)malloc(sizeof(Stamp) + sizeof(Tile) * stamp->Width * stamp->Height);
        if (!stampNew)
            return NULL;

        Tile* tileDst = &stampNew->Data[0];
        if (flipHorizontal && flipVertical) {
            Tile* tileSrcRow = &stamp->Data[stamp->Width * (stamp->Height - 1)];
            for (int row = 0; row < stamp->Height; row++) {
                Tile* tileSrc = &tileSrcRow[stamp->Width - 1];
                for (int col = 0; col < stamp->Width; col++) {
                    *tileDst = *tileSrc;
                    if (*tileDst != TILE_EMPTY) {
                        tileDst->FlipX ^= 1;
                        tileDst->FlipY ^= 1;
                    }
                    tileDst++;
                    tileSrc--;
                }
                tileSrcRow -= stamp->Width;
            }
        }
        else if (flipHorizontal) {
            Tile* tileSrcRow = &stamp->Data[0];
            for (int row = 0; row < stamp->Height; row++) {
                Tile* tileSrc = &tileSrcRow[stamp->Width - 1];
                for (int col = 0; col < stamp->Width; col++) {
                    *tileDst = *tileSrc;
                    if (*tileDst != TILE_EMPTY) {
                        tileDst->FlipX ^= 1;
                    }
                    tileDst++;
                    tileSrc--;
                }
                tileSrcRow += stamp->Width;
            }
        }
        else if (flipVertical) {
            Tile* tileSrcRow = &stamp->Data[stamp->Width * (stamp->Height - 1)];
            for (int row = 0; row < stamp->Height; row++) {
                Tile* tileSrc = &tileSrcRow[0];
                for (int col = 0; col < stamp->Width; col++) {
                    *tileDst = *tileSrc;
                    if (*tileDst != TILE_EMPTY) {
                        tileDst->FlipY ^= 1;
                    }
                    tileDst++;
                    tileSrc++;
                }
                tileSrcRow -= stamp->Width;
            }
        }

        memcpy(stampNew, stamp, sizeof(Stamp));
        return stampNew;
    }

    static Stamp* FromStreamRead(Stream* stream) {
        // Read size
        int width = stream->ReadUInt16();
        int height = stream->ReadUInt16();

        // Create stamp
        Stamp* Data = Stamp::CreateEmpty(width, height);

        // Read tile data
        // Uint32 dataRead =
        stream->ReadCompressed(&Data->Data[0]);
        /*if (dataRead == width * height * sizeof(Tile))
            printf("perfect stamp tile data read!");
        else
            printf("invalid stamp tile data read!");*/

        return Data;
    }
    void Write(Stream* stream) {
        // Write size
        stream->WriteUInt16(this->Width);
        stream->WriteUInt16(this->Height);

        // Write tile data
        stream->WriteCompressed(&this->Data[0], this->Width * this->Height * sizeof(Tile));
    }
};
struct SavedStamp {
    String Title;
    Stamp* Data;

    void Read(Stream* stream) {
        char title[256];

        // Read magic
        Uint32 magic = stream->ReadUInt32();

        // Read title
        stream->ReadHeaderedString(title);
        Strings::FromCString(&Title, title, 0);

        Data = Stamp::FromStreamRead(stream);
    }
    void Write(Stream* stream) {
        char title[256];

        // Write magic
        stream->WriteUInt32(0x00000000);

        // Write title
        if (Title.Length > 255)
            Title.Length = 255;
        Strings::ToCString(title, &Title);
        stream->WriteHeaderedString(title);

        Data->Write(stream);
    }

    ~SavedStamp() {
        free(Data);
    }
};

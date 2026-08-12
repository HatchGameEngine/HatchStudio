#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Hashing/Murmur.h>
#include <Hatch/ImageFormats/GIF.h>
#include <Hatch/IO/Stream.h>
#include <Hatch/IO/FileStream.h>

#include <Libraries/stb_image.h>
#include <Libraries/stb_image_write.h>

#include <Hatch/Diagnostics.h>

#include <Studio/Impl.hpp>

#include <UI/Filesystem/Paths.hpp>

#include <Studio/Editors/SceneEditor.hpp>
#include <Studio/StageTileset.hpp>

#define DEFAULT_SHEET_WIDTH 1024
#define DEFAULT_SHEET_HEIGHT 1024

#define STAMP_FILENAME_PREFIX "Stamp_"

StageTileset::StageTileset() {
    ClearCollisionData(0, 0x1000);

    memset(TileCollisionTextures, 0, sizeof(TileCollisionTextures));
    memset(TileHashes, 0, sizeof(TileHashes));

    ImageWidth = DEFAULT_SHEET_WIDTH;
    ImageHeight = DEFAULT_SHEET_HEIGHT;

    UpdateTileCollisionTexture_All();
}
StageTileset::~StageTileset() {
    if (TileImageTexture) SDL_DestroyTexture(TileImageTexture);

    for (int p = 0; p < 2; p++) {
        if (TileCollisionTextures[p]) SDL_DestroyTexture(TileCollisionTextures[p]);
    }

    if (TileImagePixelData)
        free(TileImagePixelData);
}

void StageTileset::ClearCollisionData(int start, int end) {
    for (int i = start; i < end; i++) {
        memset(&TileCfg[0][i], 0, sizeof(TileCfg[0][i]));
        memset(&TileCfg[0][i].Collision, 0xFF, sizeof(TileCfg[0][i].Collision));
        memset(&TileCfg[1][i].Collision, 0xFF, sizeof(TileCfg[1][i].Collision));
    }
}

void StageTileset::SetTileCount(int newTileCount) {
    newTileCount = M_CLAMP(newTileCount, 0, 0x1000);

    if (newTileCount == TileCount) {
        return;
    }

    if (newTileCount < TileCount) {
        ClearCollisionData(newTileCount, TileCount);
    }
    else {
        ClearCollisionData(TileCount, newTileCount);
    }

    TileCount = newTileCount;
}

// Simply loads an image from the given filename.
bool StageTileset::Load(CString filename) {
    int tileset_w = 1;
    int tileset_h = 1;
    int tileset_comp;

    unsigned char* tileset_imagedata = stbi_load(filename, &tileset_w, &tileset_h, &tileset_comp, STBI_rgb_alpha);
    if (!tileset_imagedata) {
        Diagnostics::SetError(stbi_failure_reason());
        return false;
    }

    // Temporary until HatchStudio supports other tile sizes.
    if (tileset_w % 16 != 0 || tileset_h % 16 != 0) {
        Diagnostics::SetError("Image \"%s\" dimensions must be a power of 16.", filename);
        return false;
    }

    const int srcRowCount = tileset_h / 16;
    const int srcColumnCount = tileset_w / 16;

    if (srcRowCount == 0 || srcColumnCount == 0) {
        stbi_image_free(tileset_imagedata);
        return false;
    }

    Uint32* newTilesetImageData = (Uint32*)calloc(tileset_w * tileset_h * 4, sizeof(Uint32));
    if (!newTilesetImageData) {
        Diagnostics::SetError("Could not allocate space for tileset image data.");
        return false;
    }

    memcpy(newTilesetImageData, tileset_imagedata, tileset_w * tileset_h * 4);

    stbi_image_free(tileset_imagedata);

    ImageWidth = tileset_w;
    ImageHeight = tileset_h;
    TileWidth = 16;
    TileHeight = 16;
    WidthInTiles = ImageWidth / TileWidth;
    HeightInTiles = ImageHeight / TileHeight;
    ImageTileCount = WidthInTiles * HeightInTiles;

    // Update tile image data
    Studio::Textures::CreateTextureFromSTBI(&TileImageTexture, (Uint8*)newTilesetImageData, tileset_w, tileset_h);

    if (TileImagePixelData)
        free(TileImagePixelData);
    TileImagePixelData = newTilesetImageData;

    return true;
}

bool StageTileset::Import(List<char*>& filenames, ArrayList<SavedStamp*>* stampsList) {
    int maxTileCount = TILE_IDENT_MASK + 1;

    const int MAX_SHEET_HEIGHT = 1024;

    const int dstColumnCount = 64;
    const int dstColumnMask = 63;
    const int dstColumnBitshift = 6;

    int tileset_w = 1;
    int tileset_h = 1;
    int tileset_comp;

    Uint32* tileSrc;
    Uint32* tileDst;

    Uint32* newTilesetImageData = (Uint32*)calloc(DEFAULT_SHEET_WIDTH * DEFAULT_SHEET_HEIGHT * 4, sizeof(Uint32));
    if (!newTilesetImageData) {
        Diagnostics::SetError("Could not allocate space for tileset image data.");
        return false;
    }

    StageTileset::TileImageHash* oldTileHashes = (StageTileset::TileImageHash*)malloc(sizeof(TileHashes));
    if (!oldTileHashes) {
        Diagnostics::SetError("Could not allocate space for old tileset hash data.");
        return false;
    }

    memcpy(oldTileHashes, TileHashes, sizeof(TileHashes));
    memset(TileHashes, 0x00, sizeof(TileHashes));

    Tile* tileArray = (Tile*)malloc(1 * 1 * sizeof(Tile));
    if (!tileArray) {
        Diagnostics::SetError("Could not allocate space for stamp tile data.");
        return false;
    }

    int tile = 0;
    for (int i = 0; i < filenames.Count(); i++) {
        unsigned char* tileset_imagedata = stbi_load(filenames[i], &tileset_w, &tileset_h, &tileset_comp, STBI_rgb_alpha);
        if (!tileset_imagedata) {
            Diagnostics::SetError(stbi_failure_reason());
            return false;
        }

        const int srcRowCount = tileset_h / 16;
        const int srcColumnCount = tileset_w / 16;

        if (srcRowCount == 0 || srcColumnCount == 0) {
            stbi_image_free(tileset_imagedata);
            continue;
        }
        // Temporary until HatchStudio supports other tile sizes.
        else if (tileset_w % 16 != 0 || tileset_h % 16 != 0) {
            Diagnostics::SetError("Image \"%s\" dimensions must be a power of 16.", filenames[i]);
            stbi_image_free(tileset_imagedata);
            continue;
        }

        tileArray = (Tile*)realloc(tileArray, srcRowCount * srcColumnCount * sizeof(Tile));
        if (!tileArray) {
            Diagnostics::SetError("Could not allocate space for stamp tile data.");
            stbi_image_free(tileset_imagedata);
            free(oldTileHashes);
            free(newTilesetImageData);
            return false;
        }

        Pixel emptyTileImageData[TILE_SIZE * TILE_SIZE];
        memset(emptyTileImageData, 0, sizeof(emptyTileImageData));
        Uint32 emptyTileHash = Murmur_HashData(&emptyTileImageData[0], sizeof(emptyTileImageData)).A;

        // for every "tile" in the file's tilesheet
        int tind = 0;
        for (int row = 0; row < srcRowCount; row++) {
            for (int col = 0; col < srcColumnCount; col++) {
                if (tile == maxTileCount)
                    goto TotalTiles;

                // get the hash of every pixel in the tile
                Pixel currentTileImageData[TILE_SIZE * TILE_SIZE];

                Uint32* tileDst = &newTilesetImageData[(tile & dstColumnMask) * TILE_SIZE + (tile & ~dstColumnMask) * TILE_SIZE * TILE_SIZE];
                Uint32* tileSrc = &((Uint32*)tileset_imagedata)[col * TILE_SIZE + row * TILE_SIZE * srcColumnCount * TILE_SIZE];
                for (int pxrow = 0, ySrc = 0, yDst = 0; pxrow < TILE_SIZE; pxrow++) {
                    // Store pixel data for hashing
                    for (int xSrc = 0; xSrc < TILE_SIZE; xSrc++) {
                        Color color = tileSrc[xSrc + ySrc];
                        Pixel* pixel = &currentTileImageData[xSrc + pxrow * TILE_SIZE];

                        *pixel = color;
                        if (color.A == 0x00)
                            pixel->Full = 0;
                    }

                    // Copy tile line anyways, if it does but it's already used, it'll get overwritten
                    memcpy(&tileDst[yDst], &tileSrc[ySrc], TILE_SIZE * sizeof(Uint32));
                    ySrc += srcColumnCount * TILE_SIZE;
                    yDst += dstColumnCount * TILE_SIZE;
                }

                // Check for empty tile image, and if so, write it as empty and go to next source tile
                Uint32 srcHash = Murmur_HashData(&currentTileImageData[0], sizeof(currentTileImageData)).A;
                if (srcHash == emptyTileHash) {
                    tileArray[tind] = TILE_EMPTY;
                    goto NextSourceTile;
                }

                // Check for duplicate tile image, and if so, write it as that tile, and go to next source tile
                for (int t = 0; t < tile; t++) {
                    if (TileHashes[t].FLIP_NONE == srcHash) {
                        tileArray[tind] = Tile(0);
                        tileArray[tind].PlaneA = 3;
                        tileArray[tind].PlaneB = 3;
                        tileArray[tind].ID = t;
                        goto NextSourceTile;
                    }
                }

                // Otherwise, this is a unique tile, write it as itself
                tileArray[tind] = Tile(0);
                tileArray[tind].PlaneA = 3;
                tileArray[tind].PlaneB = 3;
                tileArray[tind].ID = tile;

                // Set the tile hash
                TileHashes[tile].FLIP_NONE = srcHash;
                tile++;
            NextSourceTile:
                tind++;
            }
        }

        if (stampsList) {
            // Create stamp
            char filenameBuffer[256];
            UI::Filesystem::Paths::GetFilenameWithoutExtension(filenameBuffer, filenames[i]);
            if (strncmp(STAMP_FILENAME_PREFIX, filenameBuffer, strlen(STAMP_FILENAME_PREFIX)) == 0) {
                const char* title = filenameBuffer + strlen(STAMP_FILENAME_PREFIX);
                Stamp* stamp = Stamp::FromTileArray(tileArray, srcColumnCount, srcRowCount);

                SavedStamp* savedStamp = new SavedStamp();
                Strings::FromCString(&savedStamp->Title, title, 0);
                savedStamp->Data = stamp;
                stampsList->Add(savedStamp);
            }
        }

        stbi_image_free(tileset_imagedata);
    }

    TotalTiles:
    if (tile == 0)
        goto FreeMemoryAndFail;

    ImageWidth = DEFAULT_SHEET_WIDTH;
    ImageHeight = DEFAULT_SHEET_HEIGHT;
    TileWidth = 16;
    TileHeight = 16;
    WidthInTiles = ImageWidth / TileWidth;
    HeightInTiles = ImageHeight / TileHeight;
    ImageTileCount = tile;

    // Update tile image data
    Studio::Textures::CreateTextureFromSTBI(&TileImageTexture, (Uint8*)newTilesetImageData, 1024, 1024);

    // Create the tile remapping array
    for (int oldID = 0; oldID < 0x1000; oldID++) {
        // Set conversion value to pass-through
        TileRemapArray[oldID] = oldID;

        // Match for any tiles from old to new,
        // and if new tile is an old one, set the conversion value to the new ID.
        for (int newID = 0; newID < TileCount; newID++) {
            if (oldTileHashes[oldID].FLIP_NONE != 0 &&
                oldTileHashes[oldID].FLIP_NONE == TileHashes[newID].FLIP_NONE) {
                TileRemapArray[oldID] = newID;
                break;
            }
        }
    }

FreeMemoryAndSucceed:
    if (TileImagePixelData)
        free(TileImagePixelData);
    TileImagePixelData = newTilesetImageData;

    free(oldTileHashes);
    free(tileArray);

    return true;

FreeMemoryAndFail:
    TileImagePixelData = NULL;

    free(newTilesetImageData);
    free(oldTileHashes);
    free(tileArray);

    return false;
}
bool StageTileset::Import(CString filename, ArrayList<SavedStamp*>* stampsList) {
    List<char*> filenames;
    filenames.Add((char*)filename);
    return Import(filenames, stampsList);
}
bool StageTileset::Save(CString filename) {
    if (TileImagePixelData) {
        stbi_write_png(filename, ImageWidth, ImageHeight, 4, TileImagePixelData, ImageWidth * 4);
        return true;
    }

    return false;
}

bool StageTileset::UpdateTileCollisionTexture_All() {
    Pixel collisionImagePalette[2]; // Don't have to set index 0 because it will just be transparent
    collisionImagePalette[0] = Color(0x000000, 0x00);
    collisionImagePalette[1] = Color(0xFFFFFF, 0xFF);

    const int MAX_TILE_PIXELS = 0x1000 * TileWidth * TileHeight;

    const int dstColumnMask = HATCH_TILESHEET_ROWSIZE - 1;
    const int dstColumnCount = HATCH_TILESHEET_ROWSIZE;

    const int planeCount = 2;

    Uint8* tileCollisionImageData = (Uint8*)calloc(planeCount, MAX_TILE_PIXELS);
    if (!tileCollisionImageData) {
        Diagnostics::SetError("Could not alloc memory for tile collision image data!");
        return false;
    }

    for (size_t p = 0; p < planeCount; p++) {
        Uint8* tileDstStart = &tileCollisionImageData[p * MAX_TILE_PIXELS];

        Uint8* tileDst = tileDstStart;
        for (int tile = 0; tile < 0x1000; ) {
            EditableTileConfig* tileData = &TileCfg[p][tile];

            int yDst = 0;
            for (int row = 0; row < TileHeight; row++) {
                if (tileData->Orientation) {
                    for (int ix = 0; ix < TileWidth; ix++) {
                        auto col = tileData->Collision[ix];
                        if (col != 0xFF && row <= col) {
                            tileDst[yDst + ix] = 1;
                        }
                    }
                }
                else {
                    for (int ix = 0; ix < TileWidth; ix++) {
                        auto col = tileData->Collision[ix];
                        if (col != 0xFF && row >= col) {
                            tileDst[yDst + ix] = 1;
                        }
                    }
                }

                yDst += dstColumnCount * TILE_SIZE;
            }

            // Move to next tile
            tile++;
            tileDst = &tileDstStart[(tile & dstColumnMask) * TILE_SIZE + (tile & ~dstColumnMask) * TILE_SIZE * TILE_SIZE];
        }
    }

    // Convert to textures
    for (int f = 0; f < planeCount; f++) {
        if (!TileCollisionTextures[f])
            Studio::Textures::CreateTextureFromData(&TileCollisionTextures[f], &tileCollisionImageData[f * MAX_TILE_PIXELS], collisionImagePalette, dstColumnCount << 4, dstColumnCount << 4);
        else
            Studio::Textures::UpdateTextureFromData(&TileCollisionTextures[f], &tileCollisionImageData[f * MAX_TILE_PIXELS], collisionImagePalette, dstColumnCount << 4, dstColumnCount << 4);
    }

    free(tileCollisionImageData);

    return true;
}
bool StageTileset::UpdateTileCollisionTexture(int plane, int tileID) {
    Pixel collisionImagePalette[2]; // Don't have to set index 0 because it will just be transparent
    collisionImagePalette[0] = Color(0x000000, 0x00);
    collisionImagePalette[1] = Color(0xFFFFFF, 0xFF);

    const int MAX_TILE_PIXELS = TileWidth * TileHeight;

    Uint8* tileCollisionImageData = (Uint8*)calloc(1, MAX_TILE_PIXELS);
    if (!tileCollisionImageData) {
        Diagnostics::SetError("Could not alloc memory for tile collision image data!");
        return false;
    }

    EditableTileConfig* tileData = &TileCfg[plane][tileID];

    int yDst = 0;
    for (int row = 0; row < TileHeight; row++) {
        if (tileData->Orientation) {
            for (int ix = 0; ix < TileWidth; ix++) {
                auto col = tileData->Collision[ix];
                if (col != 0xFF && row <= col) {
                    tileCollisionImageData[yDst + ix] = 1;
                }
            }
        }
        else {
            for (int ix = 0; ix < TileWidth; ix++) {
                auto col = tileData->Collision[ix];
                if (col != 0xFF && row >= col) {
                    tileCollisionImageData[yDst + ix] = 1;
                }
            }
        }

        yDst += TILE_SIZE;
    }

    // Convert to textures
    SDL_Rect dstRect = {
        (tileID % HATCH_TILESHEET_ROWSIZE) * TileWidth,
        (tileID / HATCH_TILESHEET_ROWSIZE) * TileHeight,
        TileWidth,
        TileHeight
    };

    if (!TileCollisionTextures[plane]) {
        Diagnostics::SetError("Tile Collision Texture must be created prior to updating an individual tile.");
        free(tileCollisionImageData);
        return false;
    }
    else {
        Studio::Textures::UpdateTextureFromData(&TileCollisionTextures[plane],
            tileCollisionImageData, collisionImagePalette, TileWidth, TileHeight, &dstRect);
    }

    free(tileCollisionImageData);
    return true;
}

bool StageTileset::LoadTileset_RSDK(CString filename) {
    Stream* stream = FileStream::New(filename, FileStream::READ_ACCESS);
    if (stream) {
        Image image;
        if (!GIF_Load(stream, &image)) {
            stream->Close();
            return false;
        }
        stream->Close();

        TileCount = 0x400;

        const int srcColumnMask = 0;
        const int srcColumnCount = 1;
        // const int srcColumnBitshift = 0;

        const int dstColumnMask = 63;
        const int dstColumnCount = 64;
        // const int dstColumnBitshift = 6;

        Uint8* tileSrc;
        Uint8* tileDst;
        const int MAX_TILE_PIXELS = 0x1000 * TILE_SIZE * TILE_SIZE;

        Uint8* tileImageData = (Uint8*)malloc(MAX_TILE_PIXELS);
        if (!tileImageData) {
            Diagnostics::SetError("Could not alloc memory for tile image data!");
            return false;
        }

        // Turn tiles into square sheet & set tile hashes
        tileSrc = &image.Data[0];
        tileDst = &tileImageData[0];
        for (int tile = 0; tile < TileCount; ) {
            Pixel currentTileImageData[TILE_SIZE * TILE_SIZE];

            for (int row = 0, ySrc = 0, yDst = 0; row < TILE_SIZE; row++) {
                // Store pixel data for hashing
                for (int xSrc = 0; xSrc < TILE_SIZE; xSrc++)
                    currentTileImageData[xSrc + ySrc] = image.Palette[tileSrc[xSrc + ySrc]];

                // Copy tile line
                memcpy(&tileDst[yDst], &tileSrc[ySrc], TILE_SIZE * sizeof(Uint8));
                ySrc += srcColumnCount * TILE_SIZE;
                yDst += dstColumnCount * TILE_SIZE;
            }

            // Set the tile hash
            TileHashes[tile].FLIP_NONE = Murmur_HashData(&currentTileImageData, sizeof(currentTileImageData)).A;

            // Move to next tile
            tile++;
            tileSrc = &image.Data[(tile & srcColumnMask) * TILE_SIZE + (tile & ~srcColumnMask) * TILE_SIZE * TILE_SIZE];
            tileDst = &tileImageData[(tile & dstColumnMask) * TILE_SIZE + (tile & ~dstColumnMask) * TILE_SIZE * TILE_SIZE];
        }

        // Convert to texture
        if (!TileImageTexture)
            Studio::Textures::CreateTextureFromData(&TileImageTexture, tileImageData, image.Palette, dstColumnCount << 4, dstColumnCount << 4);
        else
            Studio::Textures::UpdateTextureFromData(&TileImageTexture, tileImageData, image.Palette, dstColumnCount << 4, dstColumnCount << 4);
    }
    else {
        return false;
    }

    return true;
}

bool StageTileset::ReadTileConfig_RSDK(Stream* stream) {
    struct RSDKTileConfigPackedData {
        Uint8 Collision[16];
        Uint8 HasCollision[16];
        Uint8 Orientation;
        Uint8 Angle[4];
        Uint8 Behavior;
    };
    static RSDKTileConfigPackedData RSDK_temp[2][0x400];

    Uint32 magic = stream->ReadUInt32();
    if (magic == MAGIC_TILESET_RSDK) {
        stream->ReadCompressed(&RSDK_temp[0][0]);

        TileCount = 0x400;
        ClearCollisionData(0, TileCount);

        for (size_t p = 0; p < 2; p++) {
            for (size_t i = 0; i < (size_t)TileCount; i++) {
                EditableTileConfig* tileConfig = &TileCfg[p][i];
                RSDKTileConfigPackedData* tileConfigData = &RSDK_temp[p][i];

                tileConfig->Behavior = tileConfigData->Behavior;
                tileConfig->Orientation = tileConfigData->Orientation;
                memcpy(&tileConfig->AngleTop, &tileConfigData->Angle, sizeof(tileConfigData->Angle));
                memcpy(&tileConfig->Collision, &tileConfigData->Collision, sizeof(tileConfig->Collision));

                for (int c = 0; c < 16; c++) {
                    if (!tileConfigData->HasCollision[c]) {
                        tileConfig->Collision[c] = 0xFF;
                    }
                }
            }
        }
    }
    else {
        Diagnostics::SetError("Invalid magic!");
        return false;
    }

    return true;
}
bool StageTileset::ReadTileConfig_Hatch(Stream* stream) {
    Uint32 magic = stream->ReadUInt32();
    if (magic != MAGIC_TILESET_HATCH) {
        return false;
    }

    ClearCollisionData(0, 0x1000);

    TileCount = stream->ReadUInt32();
    int tileSize = stream->ReadByte();
    stream->ReadByte();
    stream->ReadByte();
    stream->ReadByte();
    stream->ReadUInt32();

    for (int i = 0; i < TileCount; i++) {
        EditableTileConfig* tileConfig = &TileCfg[0][i];

        tileConfig->Orientation = stream->ReadByte();

        Uint8 angle = stream->ReadByte();

        if (angle == 0xFF) {
            tileConfig->AngleTop = 0x00; // Top
            tileConfig->AngleLeft = 0xC0; // Left
            tileConfig->AngleRight = 0x40; // Right
            tileConfig->AngleBottom = 0x80; // Bottom
        }
        else {
            if (tileConfig->Orientation) {
                tileConfig->AngleTop = 0x00;
                tileConfig->AngleLeft = angle >= 0x81 && angle <= 0xB6 ? angle : 0xC0;
                tileConfig->AngleRight = angle >= 0x4A && angle <= 0x7F ? angle : 0x40;
                tileConfig->AngleBottom = angle;
            }
            else {
                tileConfig->AngleTop = angle;
                tileConfig->AngleLeft = angle >= 0xCA && angle <= 0xF6 ? angle : 0xC0;
                tileConfig->AngleRight = angle >= 0x0A && angle <= 0x36 ? angle : 0x40;
                tileConfig->AngleBottom = 0x80;
            }
        }

        bool hasCollision = stream->ReadByte();

        stream->ReadBytes(tileConfig->Collision, tileSize);
    }

    // Copy over to the other plane
    memcpy(&TileCfg[1][0], &TileCfg[0][0], TileCount * sizeof(EditableTileConfig));

    return true;
}
bool StageTileset::ReadTileConfig_HatchLite(Stream* stream) {
    // .HCOL
    return false;
}
bool StageTileset::OpenTileConfig(CString filename) {
    Stream* stream = FileStream::New(filename, FileStream::READ_ACCESS);
    if (stream) {
        bool result;
        Uint32 magic = stream->ReadUInt32();
        stream->Seek(0);

        switch (magic) {
        case MAGIC_TILESET_HATCHLITE: // HCOL (HatchLite)
            result = ReadTileConfig_HatchLite(stream);
            break;
        case MAGIC_TILESET_HATCH: // TCOL (Hatch Game Engine)
            result = ReadTileConfig_Hatch(stream);
            break;
        case MAGIC_TILESET_RSDK: // TIL0 (RSDKv5)
            result = ReadTileConfig_RSDK(stream);
            break;
        default:
            Diagnostics::SetError("Unknown tile collision format!");
            result = false;
            break;
        }

        stream->Close();
        if (!result) {
            return false;
        }

        result = UpdateTileCollisionTexture_All();
        return result;
    }
    else {
        Diagnostics::SetError("Could not open file: %s", filename);
        return false;
    }

    return true;
}

bool StageTileset::WriteTileConfig_Hatch(Stream* stream) {
    const int tileSize = 16;

    stream->WriteUInt32(MAGIC_TILESET_HATCH);

    stream->WriteUInt32(TileCount);
    stream->WriteByte(tileSize);
    stream->WriteByte(0);
    stream->WriteByte(0);
    stream->WriteByte(0);
    stream->WriteUInt32(0);

    for (size_t p = 0; p < 2; p++) {
        for (int i = 0; i < TileCount; i++) {
            EditableTileConfig* tileConfig = &TileCfg[p][i];
            stream->WriteByte(tileConfig->Orientation);
            stream->WriteByte(tileConfig->AngleTop);

            bool hasCollision = false;
            for (int t = 0; t < tileSize; t++) {
                hasCollision |= tileConfig->Collision[t] != 0xFF;
                if (hasCollision)
                    break;
            }
            stream->WriteByte(hasCollision);

            stream->WriteBytes(tileConfig->Collision, tileSize);
        }
    }

    return true;
}
bool StageTileset::SaveTileConfig(CString filename) {
    Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
    if (stream) {
        bool result = WriteTileConfig_Hatch(stream);
        stream->Close();
        return result;
    }
    else {
        Diagnostics::SetError("Could not open file: %s", filename);
        return false;
    }

    return true;
}

void StageTileset::RemapTileConfig() {
    EditableTileConfig* clone = (EditableTileConfig*)malloc(sizeof(TileCfg));
    if (!clone) {
        return;
    }

    memcpy(clone, TileCfg, sizeof(TileCfg));

    ClearCollisionData(0, 0x1000);

    for (int i = 0; i < 0x1000; i++) {
        int oldID = i;
        int newID = TileRemapArray[i];

        if (newID == oldID || newID == -1) {
            // printf("old %X -> new %X\n", i, TileRemapArray[i]);
            continue;
        }

        for (int p = 0; p < 2; p++) {
            TileCfg[p][newID | (0x1000 * FLIPXY_NONE)] = clone[oldID | (0x1000 * FLIPXY_NONE) | (p * 0x1000 * 4)];
            TileCfg[p][newID | (0x1000 * FLIPXY_XY)] = clone[oldID | (0x1000 * FLIPXY_XY) | (p * 0x1000 * 4)];
            TileCfg[p][newID | (0x1000 * FLIPXY_X)] = clone[oldID | (0x1000 * FLIPXY_X) | (p * 0x1000 * 4)];
            TileCfg[p][newID | (0x1000 * FLIPXY_Y)] = clone[oldID | (0x1000 * FLIPXY_Y) | (p * 0x1000 * 4)];
        }
    }

    free(clone);

    UpdateTileCollisionTexture_All();
}

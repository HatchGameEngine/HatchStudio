#pragma once

#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Hashing/MD5.h>
#include <Hatch/Hashing/Murmur.h>
#include <Hatch/ImageFormats/GIF.h>
#include <Hatch/IO/Stream.h>
#include <Hatch/IO/FileStream.h>

#include <Hatch/Diagnostics.h>
#include <Hatch/GameLinker.h>
#include <Hatch/Graphics.h>
#include <Hatch/Math.h>
#include <Hatch/Memory.h>
#include <Hatch/Scene.h>
#include <Hatch/Strings.h>

#include <vector>

#include <Studio/Impl.hpp>

struct EntityProperty {
    char* Name;
    Hash NameHash;
    int ValueType;
    void* ValueData;
};
struct EntityEditorData {
    Vector2 MinPos;
    Vector2 MaxPos;
    Vector2 StartPos;
    int SelectionType;
    List<EntityProperty>* Properties;
};

struct Stage {
    // Structs
    struct TileImageHash {
        Uint32 FLIP_NONE;
    };
    struct EditableTileConfig {
        Uint8 Collision[16];
        Uint8 Orientation;
        Uint8 AngleTop;
		Uint8 AngleLeft;
		Uint8 AngleRight;
		Uint8 AngleBottom;
        Uint8 Behavior;
    };

    const int HATCH_TILESIZE = TILE_SIZE;
    const int HATCH_TILESHEET_ROWSIZE = 64;
    const int HATCH_TILESHEET_COLSIZE = 64;
    const int HATCH_TILESHEET_WIDTH = HATCH_TILESHEET_ROWSIZE * HATCH_TILESIZE;
    const int HATCH_TILESHEET_HEIGHT = HATCH_TILESHEET_COLSIZE * HATCH_TILESIZE;

    char CurrentStage[16];
    bool UseGlobalClasses = false;

    ConfigPalette StageConfigPalette;

    int TileCount = 0;
    SDL_Texture* TileImageTextures[4];
    SDL_Texture* TileCollisionTextures[2 * 4];

    Uint32*      TileImagePixelData = NULL;

    std::vector<UsedClass*> Classes;
    std::vector<UsedSound> Sounds;

    EditableTileConfig TileCfg[2][0x1000 << 2]; // [planeIndex][FlipXY | TileID]
    TileImageHash TileHashes[0x1000];

    int TileRemapArray[0x1000];

    // RSDK Max Tile Count: 0x400 (1024)
    // HatchLite (Prospective) Max Tile Count: 0x1000 (4096)


    Stage() {
        Classes.clear();
        Sounds.clear();

        memset(TileCfg, 0, sizeof(TileCfg));
        for (int i = 0; i < 0x1000; i++) {
            memset(&TileCfg[0][i].Collision, 0xFF, sizeof(TileCfg[0][i].Collision));
            memset(&TileCfg[1][i].Collision, 0xFF, sizeof(TileCfg[1][i].Collision));
        }

        memset(TileImageTextures, 0, sizeof(TileImageTextures));
        memset(TileCollisionTextures, 0, sizeof(TileCollisionTextures));
        memset(TileHashes, 0, sizeof(TileHashes));

        TileImagePixelData = (Uint32*)calloc(1024 * 1024 * 4, sizeof(Uint32));

        UpdateTileCollisionTexture_All();
    }
    ~Stage() {
        for (int i = 0; i < Classes.size(); i++) {
            delete Classes[i];
        }
        Classes.clear();
        Sounds.clear();

        for (int i = 0; i < 4; i++) {
            if (TileImageTextures[i]) SDL_DestroyTexture(TileImageTextures[i]);

            for (int p = 0; p < 2; p++) {
                if (TileCollisionTextures[i | p << 2]) SDL_DestroyTexture(TileCollisionTextures[i | p << 2]);
            }
        }

        if (TileImagePixelData)
            free(TileImagePixelData);
    }

    void AddClassByName(CString streamStringBuffer) {
        size_t nameLen = strlen(streamStringBuffer);
        char* name = (char*)malloc(nameLen + 1);
        if (!name)
            return;

        memcpy(name, streamStringBuffer, nameLen);
        name[nameLen] = 0;

        UsedClass* usedClass = new UsedClass;
        usedClass->Name = name;
        usedClass->NameHash = MD5_HashString(name);
        usedClass->LinkedClassIndex = -1;

        for (int c = 0; c < GameLinker::ClassCount; c++) {
            if (usedClass->NameHash == GameLinker::ClassList[c].Name) {
                usedClass->LinkedClassIndex = c;
                break;
            }
        }

        Classes.push_back(usedClass);
    }
    int GetClass(Hash hash) {
        for (int i = 0; i < Classes.size(); i++) {
            if (Classes[i]->NameHash == hash)
                return i;
        }
        return -1;
    }
    int GetClass(CString name) {
        Hash hash = GetClassHash(name);
        return GetClass(hash);
    }

    UsedClass* GetUsedClassByClassID(int classID) {
        return Classes[classID];
    }
    Classes::ClassAttribute* GetPropertyDefinitionByHash(int classID, Hash hash) {
        auto usedClass = GetUsedClassByClassID(classID);
        if (!usedClass)
            return NULL;

        // If this Class has a LinkedClass in the DLL, check there first.
        if (usedClass->LinkedClassIndex >= 0) {
            auto linkedClass = Classes::LinkedClasses[usedClass->LinkedClassIndex];
            for (int i = 0; i < linkedClass->Properties.Count(); i++) {
                auto property = &linkedClass->Properties[i];
                if (property->Name == hash)
                    return property;
            }
        }

        for (int i = 0; i < usedClass->Properties.Count(); i++) {
            auto property = &usedClass->Properties[i];
            if (property->Name == hash)
                return property;
        }
        return NULL;
    }

    bool UpdateTileCollisionTexture_All() {
        Pixel collisionImagePalette[2]; // Don't have to set index 0 because it will just be transparent
        collisionImagePalette[0] = Color(0x000000, 0x00);
        collisionImagePalette[1] = Color(0xFFFFFF, 0xFF);

        const int MAX_TILE_PIXELS = 0x1000 * TILE_SIZE * TILE_SIZE;

        const int dstColumnMask = HATCH_TILESHEET_ROWSIZE - 1;
        const int dstColumnCount = HATCH_TILESHEET_ROWSIZE;
        // const int dstColumnBitshift = 6;

        const int planeCount = 2;
        const int orientationCount = 4;

        Uint8* tileCollisionImageData = (Uint8*)calloc(orientationCount * planeCount, MAX_TILE_PIXELS);
        if (!tileCollisionImageData) {
            Diagnostics::SetError("Could not alloc memory for tile collision image data!");
            return false;
        }

        for (size_t p = 0; p < planeCount; p++) {
            Uint8* tileDstStart = &tileCollisionImageData[orientationCount * p * MAX_TILE_PIXELS];

            Uint8* tileDst = tileDstStart;
            const int orientationOffset = MAX_TILE_PIXELS;
            for (int tile = 0; tile < 0x1000; ) {
                EditableTileConfig* tileData = &TileCfg[p][tile];

                int yDst = 0;
                int yDstFY = dstColumnCount * TILE_SIZE * (TILE_SIZE - 1);
                for (int row = 0; row < TILE_SIZE; row++) {
                    if (tileData->Orientation) {
                        for (int ix = 0; ix <= 15; ix++) {
                            auto col = tileData->Collision[ix];
                            if (col != 0xFF && row <= col) {
                                tileDst[yDst + ix] = 1;
                                tileDst[(orientationOffset * FLIPXY_X) + yDst + (ix ^ 15)] = 1;
                                tileDst[(orientationOffset * FLIPXY_Y) + yDstFY + ix] = 1;
                                tileDst[(orientationOffset * FLIPXY_XY) + yDstFY + (ix ^ 15)] = 1;
                            }
                        }
                    }
                    else {
                        for (int ix = 0; ix <= 15; ix++) {
                            auto col = tileData->Collision[ix];
                            if (col != 0xFF && row >= col) {
                                tileDst[yDst + ix] = 1;
                                tileDst[(orientationOffset * FLIPXY_X) + yDst + (ix ^ 15)] = 1;
                                tileDst[(orientationOffset * FLIPXY_Y) + yDstFY + ix] = 1;
                                tileDst[(orientationOffset * FLIPXY_XY) + yDstFY + (ix ^ 15)] = 1;
                            }
                        }
                    }

                    yDst += dstColumnCount * TILE_SIZE;
                    yDstFY -= dstColumnCount * TILE_SIZE;
                }

                // Move to next tile
                tile++;
                tileDst = &tileDstStart[(tile & dstColumnMask) * TILE_SIZE + (tile & ~dstColumnMask) * TILE_SIZE * TILE_SIZE];
            }
        }

        // Convert to textures
        for (int f = 0; f < orientationCount * planeCount; f++) {
            if (!TileCollisionTextures[f])
                Studio::Textures::CreateTextureFromData(&TileCollisionTextures[f], tileCollisionImageData + MAX_TILE_PIXELS * f, collisionImagePalette, dstColumnCount << 4, dstColumnCount << 4);
            else
                Studio::Textures::UpdateTextureFromData(&TileCollisionTextures[f], tileCollisionImageData + MAX_TILE_PIXELS * f, collisionImagePalette, dstColumnCount << 4, dstColumnCount << 4);
        }

        free(tileCollisionImageData);

        return true;
    }
    bool UpdateTileCollisionTexture(int plane, int tileID) {
        Pixel collisionImagePalette[2]; // Don't have to set index 0 because it will just be transparent
        collisionImagePalette[0] = Color(0x000000, 0x00);
        collisionImagePalette[1] = Color(0xFFFFFF, 0xFF);

        // const int dstColumnMask = 0;
        const int dstColumnCount = 1;
        // const int dstColumnBitshift = 6;

        // const int planeCount = 2;
        const int orientationCount = 4;

        const int MAX_TILE_PIXELS = TILE_SIZE * TILE_SIZE;

        Uint8* tileCollisionImageData = (Uint8*)calloc(orientationCount, MAX_TILE_PIXELS);
        if (!tileCollisionImageData) {
            Diagnostics::SetError("Could not alloc memory for tile collision image data!");
            return false;
        }

        size_t p = plane;
        {
            Uint8* tileDstStart = &tileCollisionImageData[0];

            Uint8* tileDst = tileDstStart;
            const int orientationOffset = MAX_TILE_PIXELS;
            int tile = tileID;
            {
                EditableTileConfig* tileData = &TileCfg[p][tile];

                int yDst = 0;
                int yDstFY = dstColumnCount * TILE_SIZE * (TILE_SIZE - 1);
                for (int row = 0; row < TILE_SIZE; row++) {
                    if (tileData->Orientation) {
                        for (int ix = 0; ix <= 15; ix++) {
                            auto col = tileData->Collision[ix];
                            if (col != 0xFF && row <= col) {
                                tileDst[yDst + ix] = 1;
                                tileDst[(orientationOffset * FLIPXY_X) + yDst + (ix ^ 15)] = 1;
                                tileDst[(orientationOffset * FLIPXY_Y) + yDstFY + ix] = 1;
                                tileDst[(orientationOffset * FLIPXY_XY) + yDstFY + (ix ^ 15)] = 1;
                            }
                        }
                    }
                    else {
                        for (int ix = 0; ix <= 15; ix++) {
                            auto col = tileData->Collision[ix];
                            if (col != 0xFF && row >= col) {
                                tileDst[yDst + ix] = 1;
                                tileDst[(orientationOffset * FLIPXY_X) + yDst + (ix ^ 15)] = 1;
                                tileDst[(orientationOffset * FLIPXY_Y) + yDstFY + ix] = 1;
                                tileDst[(orientationOffset * FLIPXY_XY) + yDstFY + (ix ^ 15)] = 1;
                            }
                        }
                    }

                    yDst += dstColumnCount * TILE_SIZE;
                    yDstFY -= dstColumnCount * TILE_SIZE;
                }
            }
        }

        // Convert to textures
        int fStart = (plane) * orientationCount;
        SDL_Rect dstRect = {
            (tileID & (HATCH_TILESHEET_ROWSIZE - 1)) * HATCH_TILESIZE,
            (tileID / HATCH_TILESHEET_ROWSIZE) * HATCH_TILESIZE,
            HATCH_TILESIZE,
            HATCH_TILESIZE,
        };
        for (int f = fStart; f < (plane + 1) * orientationCount; f++) {
            if (!TileCollisionTextures[f]) {
                Diagnostics::SetError("Tile Collision Texture must be created prior to updating an individual tile.");
                free(tileCollisionImageData);
                return false;
            }
            else {
                Studio::Textures::UpdateTextureFromData(&TileCollisionTextures[f],
                    tileCollisionImageData + MAX_TILE_PIXELS * (f - fStart), collisionImagePalette, HATCH_TILESIZE, HATCH_TILESIZE, &dstRect);
            }
        }

        free(tileCollisionImageData);
        return true;
    }

    // "Load" is for the first time the Stage is loaded
    bool LoadTileset_RSDK(CString filename) {
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

            Uint8* tileImageData = (Uint8*)malloc(MAX_TILE_PIXELS * 4);
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

            // Flip tiles horizontally
            tileSrc = &tileImageData[0];
            tileDst = &tileImageData[MAX_TILE_PIXELS];
            for (int line = 0; line < TileCount * TILE_SIZE; line++) {
                // int xSrc = 0;
                // int xDst = TILE_SIZE - 1;
                // for (; xSrc < TILE_SIZE; ) {
                //     tileDst[xDst] = tileSrc[xSrc];
                //     xSrc++;
                //     xDst--;
                // }

                // Loop unrolled:
                tileDst[15] = tileSrc[0];
                tileDst[14] = tileSrc[1];
                tileDst[13] = tileSrc[2];
                tileDst[12] = tileSrc[3];
                tileDst[11] = tileSrc[4];
                tileDst[10] = tileSrc[5];
                tileDst[9] = tileSrc[6];
                tileDst[8] = tileSrc[7];
                tileDst[7] = tileSrc[8];
                tileDst[6] = tileSrc[9];
                tileDst[5] = tileSrc[10];
                tileDst[4] = tileSrc[11];
                tileDst[3] = tileSrc[12];
                tileDst[2] = tileSrc[13];
                tileDst[1] = tileSrc[14];
                tileDst[0] = tileSrc[15];
                tileSrc += TILE_SIZE; // Move to next line
                tileDst += TILE_SIZE; // Move to next line
            }

            // Flip tiles vertically
            tileSrc = &tileImageData[0];
            tileDst = &tileImageData[MAX_TILE_PIXELS << 1];
            for (int tileRow = 0; tileRow < TileCount / dstColumnCount; ) {
                for (int row = 0, ySrc = 0, yDst = dstColumnCount * TILE_SIZE * (TILE_SIZE - 1); row < TILE_SIZE; row++) {
                    // Copy tile line
                    memcpy(&tileDst[yDst], &tileSrc[ySrc], dstColumnCount * TILE_SIZE * sizeof(Uint8));
                    ySrc += dstColumnCount * TILE_SIZE;
                    yDst -= dstColumnCount * TILE_SIZE;
                }

                tileSrc += dstColumnCount * TILE_SIZE * TILE_SIZE;
                tileDst += dstColumnCount * TILE_SIZE * TILE_SIZE;
                tileRow++;
            }

            // Flip tiles horizontally & vertically
            tileSrc = &tileImageData[MAX_TILE_PIXELS << 1];
            tileDst = &tileImageData[MAX_TILE_PIXELS << 1 | MAX_TILE_PIXELS];
            for (int line = 0; line < TileCount * TILE_SIZE; line++) {
                // int xSrc = 0;
                // int xDst = TILE_SIZE - 1;
                // for (; xSrc < TILE_SIZE; ) {
                //     tileDst[xDst] = tileSrc[xSrc];
                //     xSrc++;
                //     xDst--;
                // }

                // Loop unrolled:
                tileDst[15] = tileSrc[0];
                tileDst[14] = tileSrc[1];
                tileDst[13] = tileSrc[2];
                tileDst[12] = tileSrc[3];
                tileDst[11] = tileSrc[4];
                tileDst[10] = tileSrc[5];
                tileDst[9] = tileSrc[6];
                tileDst[8] = tileSrc[7];
                tileDst[7] = tileSrc[8];
                tileDst[6] = tileSrc[9];
                tileDst[5] = tileSrc[10];
                tileDst[4] = tileSrc[11];
                tileDst[3] = tileSrc[12];
                tileDst[2] = tileSrc[13];
                tileDst[1] = tileSrc[14];
                tileDst[0] = tileSrc[15];
                tileSrc += TILE_SIZE; // Move to next line
                tileDst += TILE_SIZE; // Move to next line
            }

            // Convert to textures
            for (int f = 0; f < 4; f++) {
                if (!TileImageTextures[f])
                    Studio::Textures::CreateTextureFromData(&TileImageTextures[f], tileImageData + MAX_TILE_PIXELS * f, image.Palette, dstColumnCount << 4, dstColumnCount << 4);
                else
                    Studio::Textures::UpdateTextureFromData(&TileImageTextures[f], tileImageData + MAX_TILE_PIXELS * f, image.Palette, dstColumnCount << 4, dstColumnCount << 4);
            }
        }
        else {
            return false;
        }

        return true;
    }

    void LoadConfig_RSDK(Stream* stream) {
        // StageConfig.bin
        // Class* objectClass;
        // StaticObject** staticObjectPtr;
        char streamStringBuffer[256];

        Uint32 magic = stream->ReadUInt32();
        if (magic == 0x00474643) {
            // Add global classes (if desired)
            UseGlobalClasses = stream->ReadByte();

            if (UseGlobalClasses) {
                AddClassByName("Zone");
                AddClassByName("TitleCard");
                AddClassByName("ReplayRecorder");
                AddClassByName("Camera");
                AddClassByName("HUD");
                AddClassByName("Soundboard");
                AddClassByName("Player");
                AddClassByName("Music");
                AddClassByName("DebugMode");
                AddClassByName("Ring");
                AddClassByName("ItemBox");
                AddClassByName("Shield");
                AddClassByName("InvincibleStars");
                AddClassByName("ImageTrail");
                AddClassByName("Spring");
                AddClassByName("StarPost");
                AddClassByName("SpeedGate");
                AddClassByName("Spikes");
                AddClassByName("PlaneSwitch");
                AddClassByName("Debris");
                AddClassByName("Explosion");
                AddClassByName("ScoreBonus");
                AddClassByName("Dust");
                AddClassByName("InvisibleBlock");
                AddClassByName("Animals");
                AddClassByName("SignPost");
                AddClassByName("EggPrison");
                AddClassByName("ActClear");
                AddClassByName("GameOver");
                AddClassByName("SpecialRing");
                AddClassByName("BoundsMarker");
                AddClassByName("PauseMenu");
                AddClassByName("COverlay");
                AddClassByName("Competition");
                AddClassByName("TimeAttackGate");
                AddClassByName("UIWidgets");
                AddClassByName("UIControl");
                AddClassByName("UIButton");
                AddClassByName("UIDialog");
                AddClassByName("UIWaitSpinner");
                AddClassByName("Announcer");
                AddClassByName("SuperSparkle");
                AddClassByName("EncoreRoute");
                AddClassByName("NoSwap");
            }

            // Add Stage Classes
            int stageClassCount = stream->ReadByte();
            for (int i = 0; i < stageClassCount; i++) {
                stream->ReadHeaderedString(streamStringBuffer);
                AddClassByName(streamStringBuffer);
            }

            // Load palettes
            Color color;
            for (int i = 0; i < 8; i++) {
                // Palette Set
                StageConfigPalette.UsedLines[i] = stream->ReadUInt16();
                for (int paletteLine = 0; paletteLine < 16; paletteLine++) {
                    if ((StageConfigPalette.UsedLines[i] & (1 << paletteLine)) != 0) {
                        for (int d = 0; d < 16; d++) {
                            color.R = stream->ReadByte();
                            color.G = stream->ReadByte();
                            color.B = stream->ReadByte();

                            StageConfigPalette.Palettes[i][(paletteLine << 4) | d] = color;
                        }
                    }
                }
            }

            // Load sound effects
            int wavConfigCount = stream->ReadByte();
            for (int i = 0; i < wavConfigCount; i++) {
                stream->ReadHeaderedString(streamStringBuffer);
                int maxPlaybacks = stream->ReadByte();

                size_t nameLen = strlen(streamStringBuffer);
                char* name = (char*)malloc(nameLen + 1);
                if (!name)
                    continue;

                memcpy(name, streamStringBuffer, nameLen);
                name[nameLen] = 0;

                Sounds.push_back(UsedSound { name, maxPlaybacks });
            }
        }
        else {
            fprintf(stderr, "Invalid magic for file!\n");
        }
    }
    void LoadConfig_HatchLite(Stream* stream) {
        // .HSTG
    }
    bool LoadConfig(CString filename) {
        Stream* stream = FileStream::New(filename, FileStream::READ_ACCESS);
        if (stream) {
            LoadConfig_RSDK(stream);
            stream->Close();
        }
        else {
            Diagnostics::SetError("Could not open file: %s", filename);
            return false;
        }

        // If all went well, create staticobjects for only Stage.Classes
        return true;
    }

    bool ReadTileConfig_RSDK(Stream* stream) {
        struct RSDKTileConfigPackedData {
            Uint8 Collision[16];
            Uint8 HasCollision[16];
            Uint8 Orientation;
            Uint8 Angle[4];
            Uint8 Behavior;
        };
        static RSDKTileConfigPackedData RSDK_temp[2][0x400];

        Uint32 magic = stream->ReadUInt32();
        if (magic == 0x004C4954) {
            stream->ReadCompressed(&RSDK_temp[0][0]);

            for (size_t p = 0; p < 2; p++) {
                for (size_t i = 0; i < 0x400; i++) {
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
    bool ReadTileConfig_HatchTiled(Stream* stream) {
        Uint32 magic = stream->ReadUInt32();
        if (magic != 0x4C4F4354) {
            return false;
        }

        int tileCount = stream->ReadUInt32();
        int tileSize = stream->ReadByte();
        stream->ReadByte();
        stream->ReadByte();
        stream->ReadByte();
        stream->ReadUInt32();

        for (size_t p = 0; p < 2; p++) {
            for (int i = 0; i < tileCount; i++) {
                EditableTileConfig* tileConfig = &TileCfg[p][i];

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
        }

        return true;
    }
    bool ReadTileConfig_HatchLite(Stream* stream) {
        // .HCOL
        return false;
    }
    bool OpenTileConfig(CString filename) {
        Stream* stream = FileStream::New(filename, FileStream::READ_ACCESS);
        if (stream) {
            bool result;
            Uint32 magic = stream->ReadUInt32();
            stream->Seek(0);

            switch (magic) {
            case 0x4C4F4348: // HCOL (HatchLite)
                result = ReadTileConfig_HatchLite(stream);
                break;
            case 0x4C4F4354: // TCOL (Hatch Game Engine)
                result = ReadTileConfig_HatchTiled(stream);
                break;
            case 0x004C4954: // TIL0 (RSDKv5)
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

    bool WriteTileConfig_HatchTiled(Stream* stream) {
        const int tileSize = 16;

        stream->WriteUInt32(0x4C4F4354);

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
    bool SaveTileConfig(CString filename) {
        Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
        if (stream) {
            bool result = WriteTileConfig_HatchTiled(stream);
            stream->Close();
            return result;
        }
        else {
            Diagnostics::SetError("Could not open file: %s", filename);
            return false;
        }

        return true;
    }

    void RemapTileConfig() {
        EditableTileConfig* clone = (EditableTileConfig*)malloc(sizeof(TileCfg));
        if (!clone) {
            return;
        }

        memcpy(clone, TileCfg, sizeof(TileCfg));

        memset(TileCfg, 0, sizeof(TileCfg));
        for (int i = 0; i < 0x1000; i++) {
            memset(&TileCfg[0][i].Collision, 0xFF, sizeof(TileCfg[0][i].Collision));
            memset(&TileCfg[1][i].Collision, 0xFF, sizeof(TileCfg[1][i].Collision));
        }

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

    void LinkClassData(int linkedClassIndex, int classID) {
        // Setup static object for linked class & do editor load, if not already done
        auto usedClass = Classes[classID];
        auto linkedClass = Classes::LinkedClasses[linkedClassIndex];
        auto objectClass = &GameLinker::ClassList[linkedClassIndex];
        if (*objectClass->StaticObjectPtr == NULL) {
            Memory::Alloc(objectClass->StaticObjectPtr, objectClass->StaticObjectSize, Memory::MEMPOOL_STAGE, false);

            auto staticObjectPtr = (StaticObject**)objectClass->StaticObjectPtr;
            if (*staticObjectPtr) {
                if (objectClass->onStaticConstructor) {
                    objectClass->onStaticConstructor(*staticObjectPtr);
                    (*staticObjectPtr)->StageClassID = classID;
                    (*staticObjectPtr)->UpdateFlag = 0;
                }
            }

            // Setup class properties
            Classes::FocusedLinkedClass = linkedClass;

            // Do editor load for class
            auto onEditorLoad = objectClass->onEditorLoad;
            if (onEditorLoad)
                onEditorLoad();

            // Call the class' Setup function
            auto setupFunction = objectClass->onSetup;
            if (setupFunction)
                setupFunction();

            /*printf("%s: \n", usedClass->Name);
            for (int i = 0; i < linkedClass->Properties.Count(); i++) {
                Classes::ClassAttribute* attr = &linkedClass->Properties[i];
                printf("> %s\n", attr->NameString);
            }*/
        }
    }
    void LinkAllUsedClasses() {
        for (int i = 0; i < (int)Classes.size(); i++) {
            UsedClass* usedClass = Classes[i];

            int linkedClassIndex = usedClass->LinkedClassIndex;
            if (linkedClassIndex > -1)
                LinkClassData(linkedClassIndex, i);
        }
    }

    static Hash GetClassHash(CString name) {
        return MD5_HashString(name);
    }
};

// Networking: ASIO that builds a queue of messages, read after event polling
enum class PacketTypes {
    Error,
    Join,
    TransferCommand,
};
namespace CommandIDs {
    enum {
        Error,
        LayerTileEditCommand,
        LayerTileSelectionEditCommand,
    };
};

enum EditorTypes {
    SCENE,
    SPRITE,
};

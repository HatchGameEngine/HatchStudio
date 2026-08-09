#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Hashing/MD5.h>
#include <Hatch/Hashing/Murmur.h>
#include <Hatch/ImageFormats/GIF.h>
#include <Hatch/IO/Stream.h>
#include <Hatch/IO/FileStream.h>
#include <Hatch/IO/ResourceStream.h>

#define STB_IMAGE_IMPLEMENTATION
#include <Libraries/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Libraries/stb_image_write.h>

#include <Hatch/Diagnostics.h>
#include <Hatch/GameLinker.h>
#include <Hatch/Graphics.h>
#include <Hatch/Math.h>
#include <Hatch/Memory.h>
#include <Hatch/Resources.h>
#include <Hatch/Scene.h>
#include <Hatch/Services.h>
#include <Hatch/Strings.h>

#include <vector>
#include <chrono>

#include <Studio/Impl.hpp>

#include <UI/Graphics/Font.hpp>
#include <UI/Graphics/Renderer.hpp>

// Control imports
#include <UI/Controls/Control.hpp>

#include <UI/Controls/AngleEditor.hpp>
#include <UI/Controls/Button.hpp>
#include <UI/Controls/ComboBox.hpp>
#include <UI/Controls/Container.hpp>
#include <UI/Controls/Form.hpp>
#include <UI/Controls/Label.hpp>
#include <UI/Controls/ListView.hpp>
#include <UI/Controls/MenuBar.hpp>
#include <UI/Controls/NumericUpDownBox.hpp>
#include <UI/Controls/PropertyGrid.hpp>
#include <UI/Controls/ScrollBar.hpp>
#include <UI/Controls/ShowHideTabControl.hpp>
#include <UI/Controls/SplitContainer.hpp>
#include <UI/Controls/TabControls.hpp>
#include <UI/Controls/Textbox.hpp>
#include <UI/Controls/ToolStrip.hpp>
#include <UI/Controls/ToolTip.hpp>
#include <UI/Filesystem/Paths.hpp>
#include <UI/System/Application.hpp>
#include <UI/System/Clipboard.hpp>
#include <UI/System/Menu.hpp>
#include <UI/System/SystemDialog.hpp>

#include <Studio/Editors/ResourceEditor.hpp>
#include <Studio/Project.hpp>

// using UI::Graphics;
using Studio::ResourceEditor;

// The main namespaces
namespace Hatch { }
namespace UI { }
namespace Studio { }

#define TOLOWER(ch) SDL_tolower(ch)

char* stristr(const char* str1, const char* str2) {
    const char* p1 = str1;
    const char* p2 = str2;
    const char* r = *p2 == 0 ? str1 : 0;

    while (*p1 != 0 && *p2 != 0) {
        if (TOLOWER((unsigned char)*p1) == TOLOWER((unsigned char)*p2)) {
            if (r == 0) {
                r = p1;
            }

            p2++;
        }
        else {
            p2 = str2;
            if (r != 0) {
                p1 = r + 1;
            }

            if (TOLOWER((unsigned char)*p1) == TOLOWER((unsigned char)*p2)) {
                r = p1;
                p2++;
            }
            else {
                r = 0;
            }
        }

        p1++;
    }
    return *p2 == 0 ? (char*)r : 0;
}

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

struct SceneEditor : ResourceEditor {
    #pragma region Enums & Constants
    enum {
        LAYER_VISIBILITY,
        LAYER_FOCUS,
    };
    #pragma endregion

    #pragma region Structures
    struct Stamp {
        int Width;
        int Height;
        Tile Data[];

        static Stamp* FromLayer(SceneEditor* scene, int layerIndex, int x, int y, int w, int h) {
            Layer* layer = &scene->Layers[layerIndex];

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
        static void   ToLayer(Stamp* stamp, SceneEditor* scene, int layerIndex, int x, int y, bool doEmptyTileWrite) {
            int w = stamp->Width, h = stamp->Height;
            Layer* layer = &scene->Layers[layerIndex];

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
    struct Version {
        Uint8 major;
        Uint8 minor;
        Uint16 patch;
    };
    #pragma endregion

    #pragma region Commands
    struct LayerTileEditCommand : Command {
        SceneEditor* _scene;
        int _layer;
        int _tileX;
        int _tileY;

        Stamp* _toStamp;
        Stamp* _originalData;
        bool _replace;

        // The command owns "toStamp"
        LayerTileEditCommand(SceneEditor* scene, int layerIndex, int x, int y, Stamp* toStamp, bool replace = false) {
            _scene = scene;
            _layer = layerIndex;
            _tileX = x;
            _tileY = y;
            _toStamp = toStamp;
            _originalData = Stamp::FromLayer(scene, layerIndex, x, y, toStamp->Width, toStamp->Height);
            _replace = replace;

            IsDataChange = true;
        }
        ~LayerTileEditCommand() {
            delete _toStamp;
            delete _originalData;
        }

        void Do() {
            Stamp::ToLayer(_toStamp, _scene, _layer, _tileX, _tileY, _replace);
        }
        void Undo() {
            Stamp::ToLayer(_originalData, _scene, _layer, _tileX, _tileY, true);
        }
        void Read(Stream* stream) {
            // NOTE: values are slightly out of order to promote
            //       better binary packing
            _layer = stream->ReadInt16();
            _tileX = stream->ReadInt16();
            _tileY = stream->ReadInt16();
            _replace = stream->ReadInt16();
            _toStamp = Stamp::FromStreamRead(stream);
            _originalData = Stamp::FromStreamRead(stream);
        }
        void Write(Stream* stream) {
            // NOTE: values are slightly out of order to promote
            //       better binary packing
            stream->WriteInt16(_layer);
            stream->WriteInt16(_tileX);
            stream->WriteInt16(_tileY);
            stream->WriteInt16(_replace);
            _toStamp->Write(stream);
            _originalData->Write(stream);
        }
        Uint32 GetID() { return CommandIDs::LayerTileEditCommand; }
    };
    struct LayerTileSelectionEditCommand : Command {
        SceneEditor* _scene;
        SDL_Rect _dataToSet;
        SDL_Rect _originalData;

        // The command owns "toStamp"
        LayerTileSelectionEditCommand(SceneEditor* scene, SDL_Rect rect) {
            _scene = scene;
            _dataToSet = rect;
            _originalData = scene->tilePlacementField->TileSelectBounds;

            IsDataChange = true;
        }

        void Do() {
            _scene->tilePlacementField->TileSelectBounds = _dataToSet;
        }
        void Undo() {
            _scene->tilePlacementField->TileSelectBounds = _originalData;
            _scene->tilePlacementField->SelectTool(TilePlacementField::TOOL_SELECT);
        }
        void Read(Stream* stream) {
            _dataToSet.x = stream->ReadInt32();
            _dataToSet.y = stream->ReadInt32();
            _dataToSet.w = stream->ReadInt32();
            _dataToSet.h = stream->ReadInt32();
            _originalData.x = stream->ReadInt32();
            _originalData.y = stream->ReadInt32();
            _originalData.w = stream->ReadInt32();
            _originalData.h = stream->ReadInt32();
        }
        void Write(Stream* stream) {
            stream->WriteInt32(_dataToSet.x);
            stream->WriteInt32(_dataToSet.y);
            stream->WriteInt32(_dataToSet.w);
            stream->WriteInt32(_dataToSet.h);
            stream->WriteInt32(_originalData.x);
            stream->WriteInt32(_originalData.y);
            stream->WriteInt32(_originalData.w);
            stream->WriteInt32(_originalData.h);
        }
        Uint32 GetID() { return CommandIDs::LayerTileSelectionEditCommand; }
    };
    struct UndoableToolChangeCommand : Command {
        // Used for actions that change the tool

        // IsDataChange = false;
    };
    struct EntityDataEditCommand : Command {
        // Used for changing a field in the Entity
        // Stores the offset, sizes, and bytes changed, can operate on multiple entities
        // (ex: Position, Add/Remove Entity, changing attribute, re-ordering slots)

        SceneEditor* _scene;
        int _entitySlot;
        void* _writeData;
        size_t _writeLength;
        void* _previousData;
        void* _writeDestination;

        // TODO: Add a constructor for every command here that just needs the SceneEditor as a parameter
        //       This should make Read()'ing into a new command easier to setup
        EntityDataEditCommand(SceneEditor* scene, int entitySlot, void* dstData, void* srcData, size_t length) {
            _scene = scene;
            _entitySlot = entitySlot;
            _writeLength = length;
            if (length) {
                _writeData = new char[_writeLength];
                _previousData = new char[_writeLength];
                memcpy(_writeData, srcData, _writeLength);
                memcpy(_previousData, dstData, _writeLength);
                _writeDestination = dstData;
            }
            else {
                _writeData = NULL;
                _previousData = NULL;
                _writeDestination = NULL;
            }

            IsDataChange = true;
        }
        ~EntityDataEditCommand() {
            delete[] _writeData;
        }

        void Do() {
            memcpy(_writeDestination, _writeData, _writeLength);
        }
        void Undo() {
            memcpy(_writeDestination, _previousData, _writeLength);
        }
        void Read(Stream* stream) {
            _entitySlot = stream->ReadInt32();
            _writeLength = stream->ReadInt32();
            _writeData = new char[_writeLength];
            _previousData = new char[_writeLength];
            stream->ReadBytes(_writeData, _writeLength);
            stream->ReadBytes(_previousData, _writeLength);

            Uint32 offset = stream->ReadUInt32();
            _writeDestination = (Uint8*)&_scene->EntitySlots[_entitySlot] + offset;
        }
        void Write(Stream* stream) {
            stream->WriteInt32(_entitySlot);
            stream->WriteInt32(_writeLength);
            stream->WriteBytes(_writeData, _writeLength);
            stream->WriteBytes(_previousData, _writeLength);
            stream->WriteUInt32(((Uint8*)_writeDestination - (Uint8*)&_scene->EntitySlots[_entitySlot]));
        }
        Uint32 GetID() { return CommandIDs::Error; }
    };
    struct EntityRemoveCommand : Command {
        // Used for changing a field in the Entity
        // Stores the offset, sizes, and bytes changed, can operate on multiple entities
        // (ex: Position, Add/Remove Entity, changing attribute, re-ordering slots)

        SceneEditor* _scene;
        int _entitySlot;
        EntitySlot* _backupEntity;
        EntityEditorData* _backupMetadata;

        // TODO: Add a constructor for every command here that just needs the SceneEditor as a parameter
        //       This should make Read()'ing into a new command easier to setup
        EntityRemoveCommand(SceneEditor* scene, int entitySlot) {
            _scene = scene;
            _entitySlot = entitySlot;

            _backupEntity = new EntitySlot;
            _backupMetadata = new EntityEditorData;
            memcpy(_backupEntity, &scene->EntitySlots[entitySlot], sizeof(EntitySlot));
            memcpy(_backupMetadata, &scene->EntityEditorSlots[entitySlot], sizeof(EntityEditorData));

            _backupMetadata->SelectionType = TilePlacementField::EMS_NONE;

            IsDataChange = true;
        }
        ~EntityRemoveCommand() {
            delete _backupEntity;
            delete _backupMetadata;
        }

        void Do() {
            auto& slot = _entitySlot;
            auto& count = _scene->EntityCount;
            auto& entities = _scene->EntitySlots;
            auto& metadatas = _scene->EntityEditorSlots;

            if (slot + 1 < _scene->EntityCount) {
                memmove(&entities[slot], &entities[slot + 1], sizeof(EntitySlot) * (count - slot - 1));
                memmove(&metadatas[slot], &metadatas[slot + 1], sizeof(EntityEditorData) * (count - slot - 1));
            }
            _scene->EntityCount--;

            _scene->EntityUpdateUI();
        }
        void Undo() {
            auto& slot = _entitySlot;
            auto& count = _scene->EntityCount;
            auto& entities = _scene->EntitySlots;
            auto& metadatas = _scene->EntityEditorSlots;

            if (count + 1 < _scene->EntityCapacity) {
                memmove(&entities[slot + 1], &entities[slot], sizeof(EntitySlot) * (count - slot));
                memmove(&metadatas[slot + 1], &metadatas[slot], sizeof(EntityEditorData) * (count - slot));
            }
            memcpy(&entities[slot], _backupEntity, sizeof(EntitySlot));
            memcpy(&metadatas[slot], _backupMetadata, sizeof(EntityEditorData));
            count++;

            _scene->EntityUpdateUI();
        }
        void Read(Stream* stream) {

        }
        void Write(Stream* stream) {

        }
        Uint32 GetID() { return CommandIDs::Error; }
    };
    #pragma endregion

    /*
    Container that's like Splitter but when one side collaspes, it can be a side-tab or icon

    Google Docs-like Share Button:
    Gets the IP address Hexcode "Room Code", begins broadcasting current stage
    - People added can either View, or Edit (different parts of the stage at once possibly)
    */
    #pragma region Subcontrols
    struct TileSelector : Panel {
        SceneEditor* Editor = NULL;

        int TileSize = 16;
        int TileSpace = 17;
        int Columns = 16;

        int SelectedTileID = 0;
        int SelectedTileRange_Start = 0;
        int SelectedTileRange_End = 0;

        String DefaultTextLine1;
        String DefaultTextLine2;

        Tile StampTileBuffer[256];

        bool ShowTileGraphics = false;
        bool ShowTileCollision = false;
        int TileCollisionPlane = 0;

        DEFINE_SIMPLE_EVENT(SelectedTileIDChanged);
        DEFINE_SIMPLE_EVENT(SelectedTileRangeChanged);

        TileSelector(SceneEditor* editor) : Panel() {
            Editor = editor;

            Margin = 1;
            Padding = 7;

            TileSpace = TileSize + Margin.Left;

            DoHScroll = false;
            DoVScroll = true;

            HideEmptyVScroll = false;

            BackColor = Color(0x282C34, 0xFF);

            Strings::FromCString(&DefaultTextLine1, "Import Tileset Images", 0);
            Strings::FromCString(&DefaultTextLine2, "To Get Started!", 0);
        }

        void OnMouseDown(MouseEventArgs* e) {
            Control::OnMouseDown(e);

            if (e->Button == SDL_BUTTON(SDL_BUTTON_LEFT) && CaptureMouse()) {
                Position windowPos = GetPositionInWindowCoords();

                int mx = e->X, my = e->Y;
                mx -= windowPos.X;
                my -= windowPos.Y;
                mx -= ContentBounds.x + Padding.Left;
                my -= ContentBounds.y + Padding.Top;
                my += VScrollControl->Value;
                if (mx >= 0 &&
                    my >= 0 &&
                    mx < ContentBounds.x + ContentBounds.w - (Padding.Left + Padding.Right) &&
                    my < ContentBounds.y + ContentBounds.h - (Padding.Top + Padding.Bottom)) {
                    int tileIndex = M_MIN((mx / TileSpace) + (my / TileSpace) * Columns, Editor->LinkedStage->TileCount);
                    SelectRange(tileIndex, tileIndex);
                    Select(tileIndex);
                }
            }
        }
        void OnMouseMove(MouseEventArgs* e) {
            Control::OnMouseMove(e);

            if (e->Button == SDL_BUTTON(SDL_BUTTON_LEFT) && MouseCaptured == this) {
                RequestUpdatedBounds();

                Position windowPos = GetPositionInWindowCoords();

                int mx = e->X, my = e->Y;
                mx -= windowPos.X;
                my -= windowPos.Y;
                mx -= ContentBounds.x + Padding.Left;
                my -= ContentBounds.y + Padding.Top;
                my += VScrollControl->Value;
                if (mx >= 0 &&
                    my >= 0 &&
                    mx < ContentBounds.x + ContentBounds.w - (Padding.Left + Padding.Right) &&
                    my < ContentBounds.y + ContentBounds.h - (Padding.Top + Padding.Bottom)) {
                    int tileIndex = M_MIN((mx / TileSpace) + (my / TileSpace) * Columns, Editor->LinkedStage->TileCount);
                    SelectRange(SelectedTileRange_Start, tileIndex);
                    Select(tileIndex);
                }
            }
        }
        void OnMouseUp(MouseEventArgs* e) {
            if (MouseCaptured == this) {
                UncaptureMouse();
            }
        }

        void RequestUpdatedBounds() {
            if (!Editor || !Editor->LinkedStage)
                return;

            auto Bounds = GetScreenRect();

            TileSpace = TileSize + Margin.Left;

            Columns = (Size.Get().W - (Padding.Horizontal() + VScrollControl->Size.Get().W)) / TileSpace;
            Columns = M_MAX(Columns, 1);

            ContentBounds.x = 0;
            ContentBounds.y = 0;
            ContentBounds.w = TileSpace * Columns - Margin.Left + Padding.Horizontal();
            ContentBounds.h = TileSpace * ((Editor->LinkedStage->TileCount + (Columns - 1)) / Columns) - Margin.Left + Padding.Vertical();

            // Bounds.w = ContentBounds.w + VScrollControl->Bounds.w;
        }
        void ResizeChildren() {
            HideEmptyVScroll = !Editor || !Editor->LinkedStage || Editor->LinkedStage->TileCount == 0;

            RequestUpdatedBounds();

            auto size = Size.Get();
            DisplayBounds.w = size.W;
            DisplayBounds.h = size.H;

            bool showHScrollBar = DoHScroll && DisplayBounds.w < ContentBounds.w;
            bool showVScrollBar = DoVScroll;
            ::Size hScrollBarSize = HScrollControl->Size;
            ::Size vScrollBarSize = VScrollControl->Size;

            if (showHScrollBar)
                DisplayBounds.h -= hScrollBarSize.H;
            if (showVScrollBar)
                DisplayBounds.w -= vScrollBarSize.W;

            HScrollControl->Location = { 0, DisplayBounds.h };
            HScrollControl->Size = { DisplayBounds.w, hScrollBarSize.H };

            VScrollControl->Location = { DisplayBounds.w, 0 };
            VScrollControl->Size = { vScrollBarSize.W, DisplayBounds.h };

            VScrollControl->Minimum = 0;
            VScrollControl->Maximum = ContentBounds.h - DisplayBounds.h;

            VScrollControl->SmallChange = TileSpace;
            VScrollControl->LargeChange = TileSpace * 4;

            Control::ResizeChildren();
        }

        void GetHighlightBounds(int* start, int* end) {
            *start = M_MIN(SelectedTileRange_Start, SelectedTileRange_End);
            *end = M_MAX(SelectedTileRange_Start, SelectedTileRange_End);
        }
        bool IsCellWithinHighlight(int x, int y) {
            int tCount = Editor->LinkedStage->TileCount;
            int xCount = Columns;
            int yCount = (tCount + xCount - 1) / xCount;

            if (x < 0 || x >= xCount)
                return false;
            if (y < 0 || y >= yCount)
                return false;

            int index = x + y * xCount;
            if (index < M_MIN(SelectedTileRange_Start, SelectedTileRange_End)|| index > M_MAX(SelectedTileRange_Start, SelectedTileRange_End))
                return false;

            return true;
        }
        void DrawHighlightSection(SDL_Rect* dst, int bitFlag, Color colorInner, Color colorOuter) {
            enum CheckDirs {
                CHK_TOP_LEFT = 1,
                CHK_TOP = 2,
                CHK_TOP_RIGHT = 4,
                CHK_LEFT = 8,
                CHK_RIGHT = 16,
                CHK_BOTTOM_LEFT = 32,
                CHK_BOTTOM = 64,
                CHK_BOTTOM_RIGHT = 128,
            };

            int x1i = dst->x,
                y1i = dst->y,
                x2i = dst->x + dst->w - 1,
                y2i = dst->y + dst->h - 1,
                xwi = dst->w - 2,
                yhi = dst->h - 2;
            int x1o = dst->x - 1,
                y1o = dst->y - 1,
                x2o = dst->x + dst->w,
                y2o = dst->y + dst->h,
                xwo = dst->w,
                yho = dst->h;

            // colorOuter = colorInner;

            #define INNER_TOP UI::Graphics::Renderer::DrawRect(x1i + 1, y1i, xwi, 1, colorInner)
            #define INNER_LEFT UI::Graphics::Renderer::DrawRect(x1i, y1i + 1, 1, yhi, colorInner)
            #define INNER_RIGHT UI::Graphics::Renderer::DrawRect(x2i, y1i + 1, 1, yhi, colorInner)
            #define INNER_BOTTOM UI::Graphics::Renderer::DrawRect(x1i + 1, y2i, xwi, 1, colorInner)
            #define INNER_TOP_LEFT UI::Graphics::Renderer::DrawRect(x1i, y1i, 1, 1, colorInner)
            #define INNER_TOP_RIGHT UI::Graphics::Renderer::DrawRect(x2i, y1i, 1, 1, colorInner)
            #define INNER_BOTTOM_LEFT UI::Graphics::Renderer::DrawRect(x1i, y2i, 1, 1, colorInner)
            #define INNER_BOTTOM_RIGHT UI::Graphics::Renderer::DrawRect(x2i, y2i, 1, 1, colorInner)

            #define OUTER_TOP UI::Graphics::Renderer::DrawRect(x1o + 1, y1o, xwo, 1, colorOuter)
            #define OUTER_LEFT UI::Graphics::Renderer::DrawRect(x1o, y1o + 1, 1, yho, colorOuter)
            #define OUTER_RIGHT UI::Graphics::Renderer::DrawRect(x2o, y1o + 1, 1, yho, colorOuter)
            #define OUTER_BOTTOM UI::Graphics::Renderer::DrawRect(x1o + 1, y2o, xwo, 1, colorOuter)
            #define OUTER_TOP_LEFT UI::Graphics::Renderer::DrawRect(x1o, y1o, 1, 1, colorOuter)
            #define OUTER_TOP_RIGHT UI::Graphics::Renderer::DrawRect(x2o, y1o, 1, 1, colorOuter)
            #define OUTER_BOTTOM_LEFT UI::Graphics::Renderer::DrawRect(x1o, y2o, 1, 1, colorOuter)
            #define OUTER_BOTTOM_RIGHT UI::Graphics::Renderer::DrawRect(x2o, y2o, 1, 1, colorOuter)

            if (!(bitFlag & CHK_TOP)) {
                INNER_TOP; OUTER_TOP;
            }
            if (!(bitFlag & CHK_LEFT)) {
                INNER_LEFT; OUTER_LEFT;
            }
            if (!(bitFlag & CHK_RIGHT)) {
                INNER_RIGHT; OUTER_RIGHT;
            }
            if (!(bitFlag & CHK_BOTTOM)) {
                INNER_BOTTOM; OUTER_BOTTOM;
            }

            // Outside TL Corner (0|0), Inner TL Corner (1|1), L H Connector (0|1), T V Connector (1|0)
            if (!(bitFlag & CHK_TOP_LEFT)) {
                INNER_TOP_LEFT; OUTER_TOP_LEFT;
            }
            if (!(bitFlag & CHK_TOP_RIGHT)) {
                INNER_TOP_RIGHT; OUTER_TOP_RIGHT;
            }
            if (!(bitFlag & CHK_BOTTOM_LEFT)) {
                INNER_BOTTOM_LEFT; OUTER_BOTTOM_LEFT;
            }
            if (!(bitFlag & CHK_BOTTOM_RIGHT)) {
                INNER_BOTTOM_RIGHT; OUTER_BOTTOM_RIGHT;
            }

        }

        inline int TileIndexToColumn(int t) {
            return t % Columns;
        }
        inline int TileIndexToRow(int t) {
            return t / Columns;
        }

        void Select(int id) {
            if (SelectedTileID != id) {
                SelectedTileID = id;
                OnSelectedTileIDChanged(NULL);
            }
        }
        void SelectRange(int start, int end) {
            if (SelectedTileRange_Start != start ||
                SelectedTileRange_End != end) {
                SelectedTileRange_Start = start;
                SelectedTileRange_End = end;
                OnSelectedTileRangeChanged(NULL);
            }
        }

        void Render() {
            Panel::Render();

            if (!Editor || !Editor->LinkedStage)
                return;

            auto Bounds = GetScreenRect();

            if (Editor->LinkedStage->TileCount == 0) {
                ::Size lineSz1, lineSz2;
                UI::Graphics::Font::Face* Typeface = UI::Graphics::Font::Arial[12];
                UI::Graphics::Renderer::MeasureFont(&DefaultTextLine1, Typeface, &lineSz1.W, &lineSz1.H);
                UI::Graphics::Renderer::MeasureFont(&DefaultTextLine2, Typeface, &lineSz2.W, &lineSz2.H);

                UI::Graphics::Renderer::DrawFont(&DefaultTextLine1, Typeface, Bounds.x + Bounds.w / 2, Bounds.y + Bounds.h / 2 - (lineSz1.H + lineSz2.H) / 2, TEXT_ALIGN_CENTER | TEXT_VALIGN_MIDDLE, Color(0xFFFFFF, 0xFF));
                UI::Graphics::Renderer::DrawFont(&DefaultTextLine2, Typeface, Bounds.x + Bounds.w / 2, Bounds.y + Bounds.h / 2 + (lineSz1.H + lineSz2.H) / 2, TEXT_ALIGN_CENTER | TEXT_VALIGN_MIDDLE, Color(0xFFFFFF, 0xFF));
            }
            else {
                SDL_Rect buffer;
                ClipStart(&buffer, &Bounds);

                const int columnMask = 63;
                const int columnCount = 64;
                const int columnBitshift = 6;

                int rows = TileIndexToRow(Editor->LinkedStage->TileCount + Columns - 1);
                if (rows < 1)
                    rows = 1;

                UI::Graphics::Renderer::DrawRect(
                    Bounds.x + Padding.Left - 1,
                    Bounds.y + Padding.Top - 1 - VScrollControl->Value,
                    1 + Columns * TileSpace,
                    1 + rows * TileSpace, Color(0x000000, 0xFF));

                SDL_SetTextureColorMod(Editor->LinkedStage->TileCollisionTextures[4 * TileCollisionPlane], 0xFF, 0xFF, 0xFF);

                int tileSpc = TileSpace;
                for (int t = 0; t < Editor->LinkedStage->TileCount; t++) {
                    int tX = Padding.Left + TileIndexToColumn(t) * tileSpc;
                    int tY = Padding.Top + TileIndexToRow(t) * tileSpc - VScrollControl->Value;
                    SDL_Rect src = { (t & columnMask) << 4, (t >> columnBitshift) << 4, TileSize, TileSize };
                    SDL_Rect dst = { Bounds.x + tX, Bounds.y + tY, TileSize, TileSize };

                    UI::Graphics::Renderer::DstRectAdjustment(&dst);
                    if (ShowTileGraphics) {
                        SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, Editor->LinkedStage->TileImageTextures[0], &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
                    }
                    if (ShowTileCollision) {
                        SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, Editor->LinkedStage->TileCollisionTextures[4 * TileCollisionPlane], &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
                    }
                }

                if (SelectedTileRange_Start != -1) {
                    Color colorInner = Color(0x7F7F7F, 0xFF);
                    Color colorOuter = Color(0xFFFFFF, 0xFF);
                    int indexStart, indexEnd;
                    int s = TileSpace * 2;

                    GetHighlightBounds(&indexStart, &indexEnd);

                    enum CheckDirs {
                        CHK_TOP_LEFT = 1,
                        CHK_TOP = 2,
                        CHK_TOP_RIGHT = 4,
                        CHK_LEFT = 8,
                        CHK_RIGHT = 16,
                        CHK_BOTTOM_LEFT = 32,
                        CHK_BOTTOM = 64,
                        CHK_BOTTOM_RIGHT = 128,
                    };

                    int bitFlag, cx, cy, tX, tY;
                    for (int t = indexStart; t <= indexEnd; t++) {
                        bitFlag = 0;
                        cx = TileIndexToColumn(t);
                        cy = TileIndexToRow(t);
                        tX = Padding.Left + cx * tileSpc;
                        tY = Padding.Top + cy * tileSpc - VScrollControl->Value;

                        // Cardinal directions
                        if (IsCellWithinHighlight(cx, cy - 1))
                            bitFlag |= CHK_TOP;
                        if (IsCellWithinHighlight(cx - 1, cy))
                            bitFlag |= CHK_LEFT;
                        if (IsCellWithinHighlight(cx + 1, cy))
                            bitFlag |= CHK_RIGHT;
                        if (IsCellWithinHighlight(cx, cy + 1))
                            bitFlag |= CHK_BOTTOM;

                        if ((bitFlag & (CHK_TOP | CHK_LEFT)) == (CHK_TOP | CHK_LEFT) && IsCellWithinHighlight(cx - 1, cy - 1))
                            bitFlag |= CHK_TOP_LEFT;
                        if ((bitFlag & (CHK_TOP | CHK_RIGHT)) == (CHK_TOP | CHK_RIGHT) && IsCellWithinHighlight(cx + 1, cy - 1))
                            bitFlag |= CHK_TOP_RIGHT;
                        if ((bitFlag & (CHK_BOTTOM | CHK_LEFT)) == (CHK_BOTTOM | CHK_LEFT) && IsCellWithinHighlight(cx - 1, cy + 1))
                            bitFlag |= CHK_BOTTOM_LEFT;
                        if ((bitFlag & (CHK_BOTTOM | CHK_RIGHT)) == (CHK_BOTTOM | CHK_RIGHT) && IsCellWithinHighlight(cx + 1, cy + 1))
                            bitFlag |= CHK_BOTTOM_RIGHT;

                        SDL_Rect dst = { Bounds.x + tX, Bounds.y + tY, TileSize, TileSize };
                        DrawHighlightSection(&dst, bitFlag, colorInner, colorOuter);
                    }
                }

                ClipEnd(&buffer);
            }
        }
    };
    struct PropertyGrid : Panel {
        SceneEditor* Editor = NULL;

        int LineCount = 0;
        int LineHeight = 23;
        Spacing LinePadding = 1;
        int GridWidth = 4;

        List<SDL_Rect> GridControlBoundDefinitions;

        // Entity* SelectedEntity = NULL;
        DEFINE_PROPERTY_NOSETF(Entity*, SelectedEntity, PropertyGrid);

        PropertyGrid(SceneEditor* editor) : Panel() {
            Editor = editor;

            BackColor = Color(0x16181d, 0xFF);
            ForeColor = Color(0x101114, 0xFF);

            SelectedEntity = NULL;
            new (&GridControlBoundDefinitions) List<SDL_Rect>();

            Padding = 1;
            Padding.Left = 4;

            CanFocus = true;
        }
        ~PropertyGrid() {
            for (int i = 0; i < Controls.Count(); i++) {
                delete Controls.Items[i];
            }
        }

        void UpdateLayout() {
            ::Size size = Size;
            size.W = DisplayBounds.w;

            float cellSize[2] = { (float)(size.W - Padding.Horizontal()) / GridWidth, LineHeight + LinePadding.Vertical() + 1.0f };

            int height = 0;
            for (int i = 0; i < Controls.Count(); i++) {
                Control* control = Controls.Items[i];
                SDL_Rect gridPos = GridControlBoundDefinitions[i];
                control->Location = {
                    (int)(gridPos.x * cellSize[0] + LinePadding.Left + Padding.Left),
                    (int)(gridPos.y * cellSize[1] + LinePadding.Top) + 1
                };
                control->Size = {
                    (int)(gridPos.w * cellSize[0] - LinePadding.Horizontal()),
                    (int)(gridPos.h * cellSize[1] - LinePadding.Vertical() - 1)
                };
            }

            VScrollControl->SmallChange = LineHeight / 4;
        }

        void UpdateTPFieldRender() {
            Editor->tilePlacementField->UpdateRenderTarget = true;
        }

        ::Size GetContentSize() {
            ::Size contentSize = { 0, LineCount * (LineHeight + LinePadding.Vertical() + 1) };
            return contentSize;
        }

        void AddPropertyValueUI(Entity* entity, EntityEditorData* metadata, Classes::ClassAttribute* propertyDefinition) {
            // Add property name as Label
            Label* propertyLabel = new Label(propertyDefinition->NameString);
            Controls.Add(propertyLabel);
            GridControlBoundDefinitions.Add({ 0, LineCount, 2, 1 });

            // Find matching property value in entity editor data
            EntityProperty* propertyValue = NULL;
            for (int p = 0; p < metadata->Properties->Count(); p++) {
                if (metadata->Properties->Items[p].NameHash == propertyDefinition->Name) {
                    propertyValue = &metadata->Properties->Items[p];
                    break;
                }
            }

            // If matching property value cannot be found, add it to entity metadata
            if (propertyValue == NULL) {
                EntityProperty newPropertyValue;
                newPropertyValue.NameHash = propertyDefinition->Name;
                newPropertyValue.ValueType = propertyDefinition->AttributeType;
                newPropertyValue.ValueData = calloc(1, 16);
                metadata->Properties->Add(newPropertyValue);

                propertyValue = &metadata->Properties->Items[metadata->Properties->Count() - 1];
            }

            // Add proper control to edit the value
            switch (propertyDefinition->AttributeType) {
            case VAR_INT8:
            {
                auto valuePtr = (Sint8*)propertyValue->ValueData;

                NumericUpDown* propertyValueEditor = new NumericUpDown();
                propertyValueEditor->Minimum = -128.0;
                propertyValueEditor->Maximum = 127.0;
                propertyValueEditor->Value = (double)*valuePtr;
                propertyValueEditor->DecimalPlaces = 0;
                propertyValueEditor->onValueChanged += [this, propertyDefinition, valuePtr, propertyValueEditor](void* s, EventArgs* e) -> void {
                    *valuePtr = (Sint8)propertyValueEditor->Value;
                    if (propertyDefinition->StructOffset != 0) {
                        *(Sint8*)((Uint8*)internal_SelectedEntity + propertyDefinition->StructOffset) = *valuePtr;
                        UpdateTPFieldRender();
                    }
                };
                GridControlBoundDefinitions.Add({ 2, LineCount, 2, 1 });
                Controls.Add(propertyValueEditor);
                break;
            }
            case VAR_UINT8:
            {
                auto valuePtr = (Uint8*)propertyValue->ValueData;

                NumericUpDown* propertyValueEditor = new NumericUpDown();
                propertyValueEditor->Minimum = 0.0;
                propertyValueEditor->Maximum = 255.0;
                propertyValueEditor->Value = (double)*valuePtr;
                propertyValueEditor->DecimalPlaces = 0;
                propertyValueEditor->onValueChanged += [this, propertyDefinition, valuePtr, propertyValueEditor](void* s, EventArgs* e) -> void {
                    *valuePtr = (Uint8)propertyValueEditor->Value;
                    if (propertyDefinition->StructOffset != 0) {
                        *(Uint8*)((Uint8*)internal_SelectedEntity + propertyDefinition->StructOffset) = *valuePtr;
                        UpdateTPFieldRender();
                    }
                };
                GridControlBoundDefinitions.Add({ 2, LineCount, 2, 1 });
                Controls.Add(propertyValueEditor);
                break;
            }
            case VAR_INT16:
            {
                auto valuePtr = (Sint16*)propertyValue->ValueData;

                NumericUpDown* propertyValueEditor = new NumericUpDown();
                propertyValueEditor->Minimum = -32768.0;
                propertyValueEditor->Maximum = 32767.0;
                propertyValueEditor->Value = (double)*valuePtr;
                propertyValueEditor->DecimalPlaces = 0;
                propertyValueEditor->onValueChanged += [this, propertyDefinition, valuePtr, propertyValueEditor](void* s, EventArgs* e) -> void {
                    *valuePtr = (Sint16)propertyValueEditor->Value;
                    if (propertyDefinition->StructOffset != 0) {
                        *(Sint16*)((Uint8*)internal_SelectedEntity + propertyDefinition->StructOffset) = *valuePtr;
                        UpdateTPFieldRender();
                    }
                };
                GridControlBoundDefinitions.Add({ 2, LineCount, 2, 1 });
                Controls.Add(propertyValueEditor);
                break;
            }
            case VAR_UINT16:
            {
                auto valuePtr = (Uint16*)propertyValue->ValueData;

                NumericUpDown* propertyValueEditor = new NumericUpDown();
                propertyValueEditor->Minimum = 0.0;
                propertyValueEditor->Maximum = 65535.0;
                propertyValueEditor->Value = (double)*valuePtr;
                propertyValueEditor->DecimalPlaces = 0;
                propertyValueEditor->onValueChanged += [this, propertyDefinition, valuePtr, propertyValueEditor](void* s, EventArgs* e) -> void {
                    *valuePtr = (Uint16)propertyValueEditor->Value;
                    if (propertyDefinition->StructOffset != 0) {
                        *(Uint16*)((Uint8*)internal_SelectedEntity + propertyDefinition->StructOffset) = *valuePtr;
                        UpdateTPFieldRender();
                    }
                };
                GridControlBoundDefinitions.Add({ 2, LineCount, 2, 1 });
                Controls.Add(propertyValueEditor);
                break;
            }
            case VAR_INT32:
            {
                auto valuePtr = (Sint32*)propertyValue->ValueData;

                NumericUpDown* propertyValueEditor = new NumericUpDown();
                propertyValueEditor->Minimum = -2147483648.0;
                propertyValueEditor->Maximum = 2147483647.0;
                propertyValueEditor->Value = (double)*valuePtr;
                propertyValueEditor->DecimalPlaces = 0;
                propertyValueEditor->onValueChanged += [this, propertyDefinition, valuePtr, propertyValueEditor](void* s, EventArgs* e) -> void {
                    *valuePtr = (Sint32)propertyValueEditor->Value;
                    if (propertyDefinition->StructOffset != 0) {
                        *(Sint32*)((Uint8*)internal_SelectedEntity + propertyDefinition->StructOffset) = *valuePtr;
                        UpdateTPFieldRender();
                    }
                };
                GridControlBoundDefinitions.Add({ 2, LineCount, 2, 1 });
                Controls.Add(propertyValueEditor);
                break;
            }
            case VAR_UINT32:
            {
                auto valuePtr = (Uint32*)propertyValue->ValueData;

                NumericUpDown* propertyValueEditor = new NumericUpDown();
                propertyValueEditor->Minimum = 0.0;
                propertyValueEditor->Maximum = 4294967295.0;
                propertyValueEditor->Value = (double)*valuePtr;
                propertyValueEditor->DecimalPlaces = 0;
                propertyValueEditor->onValueChanged += [this, propertyDefinition, valuePtr, propertyValueEditor](void* s, EventArgs* e) -> void {
                    *valuePtr = (Uint32)propertyValueEditor->Value;
                    if (propertyDefinition->StructOffset != 0) {
                        *(Uint32*)((Uint8*)internal_SelectedEntity + propertyDefinition->StructOffset) = *valuePtr;
                        UpdateTPFieldRender();
                    }
                };
                GridControlBoundDefinitions.Add({ 2, LineCount, 2, 1 });
                Controls.Add(propertyValueEditor);
                break;
            }
            case VAR_VECTOR2:
            {
                auto valuePtr = (Vector2*)propertyValue->ValueData;

                NumericUpDown* propertyValueEditorX = new NumericUpDown();
                propertyValueEditorX->Minimum = -32768.0;
                propertyValueEditorX->Maximum = 32767.0;
                propertyValueEditorX->Value = valuePtr->X.Full / 65536.0;
                propertyValueEditorX->DecimalPlaces = 2;
                propertyValueEditorX->onValueChanged += [this, propertyDefinition, valuePtr, propertyValueEditorX](void* s, EventArgs* e) -> void {
                    valuePtr->X.Full = propertyValueEditorX->Value * 65536.0;
                    if (propertyDefinition->StructOffset != 0) {
                        *(Vector2*)((Uint8*)internal_SelectedEntity + propertyDefinition->StructOffset) = *valuePtr;
                        UpdateTPFieldRender();
                    }
                };
                GridControlBoundDefinitions.Add({ 2, LineCount, 1, 1 });
                Controls.Add(propertyValueEditorX);

                NumericUpDown* propertyValueEditorY = new NumericUpDown();
                propertyValueEditorY->Minimum = -32768.0;
                propertyValueEditorY->Maximum = 32767.0;
                propertyValueEditorY->Value = valuePtr->Y.Full / 65536.0;
                propertyValueEditorY->DecimalPlaces = 2;
                propertyValueEditorY->onValueChanged += [this, propertyDefinition, valuePtr, propertyValueEditorY](void* s, EventArgs* e) -> void {
                    valuePtr->Y.Full = propertyValueEditorY->Value * 65536.0;
                    if (propertyDefinition->StructOffset != 0) {
                        *(Vector2*)((Uint8*)internal_SelectedEntity + propertyDefinition->StructOffset) = *valuePtr;
                        UpdateTPFieldRender();
                    }
                };
                GridControlBoundDefinitions.Add({ 3, LineCount, 1, 1 });
                Controls.Add(propertyValueEditorY);
                break;
            }
            case VAR_BOOL:
            {
                auto valuePtr = (bool*)propertyValue->ValueData;

                CheckBox* propertyValueEditor = new CheckBox();
                propertyValueEditor->CheckState = *valuePtr ? CheckState::Checked : CheckState::Unchecked;
                propertyValueEditor->onCheckedChanged += [this, propertyDefinition, valuePtr, propertyValueEditor](void* s, EventArgs* e) -> void {
                    *valuePtr = propertyValueEditor->GetChecked();
                    if (propertyDefinition->StructOffset != 0) {
                        *(bool*)((Uint8*)internal_SelectedEntity + propertyDefinition->StructOffset) = *valuePtr;
                        UpdateTPFieldRender();
                    }
                };

                GridControlBoundDefinitions.Add({ 2, LineCount, 2, 1 });
                Controls.Add(propertyValueEditor);
                break;
            }
            case VAR_ENUM:
            {
                auto valuePtr = (Sint32*)propertyValue->ValueData;

                ComboBox* propertyValueEditor = new ComboBox();
                for (int i = 0; i < propertyDefinition->EnumPairs.Count(); i++) {
                    propertyValueEditor->Items.Add(propertyDefinition->EnumPairs.Items[i].name);
                    if (*valuePtr == propertyDefinition->EnumPairs.Items[i].value)
                        propertyValueEditor->Select(i);
                }

                propertyValueEditor->onSelectedIndexChanged += [this, propertyDefinition, valuePtr, propertyValueEditor](void* s, EventArgs* e) -> void {
                    if (propertyValueEditor->SelectedIndex < 0)
                        return;

                    *valuePtr = (Sint32)propertyDefinition->EnumPairs.Items[propertyValueEditor->SelectedIndex].value;
                    *(Sint32*)((Uint8*)internal_SelectedEntity + propertyDefinition->StructOffset) = *valuePtr;
                    UpdateTPFieldRender();
                };

                GridControlBoundDefinitions.Add({ 2, LineCount, 2, 1 });
                Controls.Add(propertyValueEditor);
                break;
            }
            case VAR_STRING:
            {
                auto valuePtr = (String*)propertyValue->ValueData;
                if (valuePtr->Text == NULL)
                    Strings::Init(valuePtr, 1);

                TextboxBase* propertyValueEditor = new TextboxBase(valuePtr);
                propertyValueEditor->onTextChanged += [this, propertyDefinition, valuePtr, propertyValueEditor](void* s, EventArgs* e) -> void {
                    Strings::Copy(valuePtr, propertyValueEditor->TextPtr);
                    UpdateTPFieldRender();
                };

                GridControlBoundDefinitions.Add({ 2, LineCount, 2, 1 });
                Controls.Add(propertyValueEditor);
                break;
            }
            case VAR_COLOR:
                break;

            default:
                break;
            }

            LineCount++;
        }
        void UpdatePropertyUI() {
            for (int i = 0; i < Controls.Count(); i++) {
                delete Controls.Items[i];
            }

            LineCount = 0;
            Controls.Clear();
            GridControlBoundDefinitions.Clear();

            if (!internal_SelectedEntity)
                return;

            ::Size size = Size;

            int slotID = (EntitySlot*)internal_SelectedEntity - Editor->EntitySlots;

            Entity* entity = internal_SelectedEntity;
            EntityEditorData* metadata = &Editor->EntityEditorSlots[slotID];
            if (entity->ClassID < 0)
                return;

            auto usedClass = Editor->LinkedStage->GetUsedClassByClassID(entity->ClassID);
            if (usedClass->LinkedClassIndex >= 0) {
                // populate from linkedclass
                auto linkedClass = Classes::LinkedClasses[usedClass->LinkedClassIndex];
                for (int i = 0; i < linkedClass->Properties.Count(); i++) {
                    auto propertyDefinition = &linkedClass->Properties[i];
                    AddPropertyValueUI(entity, metadata, propertyDefinition);
                }

                // populate from usedclass, ignoring duplicates found in linkedclass (linkedclass is favored, duplicates shall not be edited)
                for (int i = 0; i < usedClass->Properties.Count(); i++) {
                    auto propertyDefinition = &usedClass->Properties[i];

                    for (int l = 0; l < linkedClass->Properties.Count(); l++) {
                        if (linkedClass->Properties[l].Name == propertyDefinition->Name)
                            goto SkipPropertyDef;
                    }

                    AddPropertyValueUI(entity, metadata, propertyDefinition);

                SkipPropertyDef:
                    continue;
                }
            }
            else {
                // populate JUST from usedclass
                for (int i = 0; i < usedClass->Properties.Count(); i++) {
                    auto propertyDefinition = &usedClass->Properties[i];
                    AddPropertyValueUI(entity, metadata, propertyDefinition);
                }
            }

            ContentBounds.h = LineCount * (LineHeight + LinePadding.Vertical() + 1);
            ResizeChildren();
            UpdateLayout();
        }
        void set_SelectedEntity(Entity* value) {
            internal_SelectedEntity = value;
            UpdatePropertyUI();
        }

        void Render() {
            auto bounds = GetScreenRect();

            if (BackColor.A) UI::Graphics::Renderer::DrawRect(&bounds, BackColor);

            ScrollLocation.Y = 0;

            bool showHScrollBar = DoHScroll && (!HideEmptyHScroll || DisplayBounds.w < ContentBounds.w);
            bool showVScrollBar = DoVScroll && (!HideEmptyVScroll || DisplayBounds.h < ContentBounds.h);
            if (showHScrollBar)
                HScrollControl->Render();
            if (showVScrollBar)
                VScrollControl->Render();

            SDL_Rect buffer;
            ClipStart(&buffer, &bounds);

            ScrollLocation.Y = VScrollControl->Value;

            if (DoZSorting)
                Controls.Sort();

            for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
                auto Child = Controls.Items[i];
                Child->Render();
            }

            bounds.y -= ScrollLocation.Y;

            if (LineCount > 0) {
                int lineH = LineHeight + LinePadding.Vertical() + 1;
                for (int i = 0; i < LineCount - 1; i++) {
                    UI::Graphics::Renderer::DrawRect(bounds.x, bounds.y + (i + 1) * lineH, DisplayBounds.w, 1, ForeColor);
                }

                UI::Graphics::Renderer::DrawRect(bounds.x + DisplayBounds.w / 2 - 1, bounds.y, 1, ContentBounds.h, ForeColor);
                UI::Graphics::Renderer::StrokeRect(bounds.x, bounds.y, DisplayBounds.w, ContentBounds.h, ForeColor);
            }

            ClipEnd(&buffer);
        }
    };
    struct StampCollection : FlowLayoutPanel {
        struct StampDisplay : Control {
            SceneEditor* Editor = NULL;
            Stamp* CurrentStamp = NULL;

            StampDisplay(SceneEditor* editor) {
                Editor = editor;
            }

            void Render() {
                Control::Render();

                auto Bounds = GetScreenRect();
                UI::Graphics::Renderer::DrawRect(&Bounds, Color(0xFF00FF, 0xFF));

                if (!Editor || !Editor->LinkedStage || !CurrentStamp)
                    return;

                {
                    SDL_Rect buffer;
                    ClipStart(&buffer, &Bounds);

                    const int columnMask = 63;
                    const int columnCount = 64;
                    const int columnBitshift = 6;

                    for (int t = 0; t < CurrentStamp->Width * CurrentStamp->Height; t++) {
                        Tile* tile = &CurrentStamp->Data[t];
                        int tX = (t % CurrentStamp->Width) * TILE_SIZE;
                        int tY = (t / CurrentStamp->Width) * TILE_SIZE;
                        SDL_Rect src = { (tile->ID & columnMask) << 4, (tile->ID >> columnBitshift) << 4, TILE_SIZE, TILE_SIZE };
                        SDL_Rect dst = {
                            Bounds.x + tX + (Bounds.w - CurrentStamp->Width * TILE_SIZE) / 2,
                            Bounds.y + tY + (Bounds.h - CurrentStamp->Height * TILE_SIZE) / 2, TILE_SIZE, TILE_SIZE };

                        UI::Graphics::Renderer::DstRectAdjustment(&dst);
                        SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, Editor->LinkedStage->TileImageTextures[tile->FlipY << 1 | tile->FlipX], &src, &dst, 0.0, NULL, SDL_FLIP_NONE);
                    }

                    ClipEnd(&buffer);
                }
            }
        };

        SceneEditor* Editor = NULL;

        Label* labelStamps;
        ListView* listViewStamps;
        ToolStrip* toolStripStamps;
        ToolStripButton* toolStripButtonAddStamp;
        ToolStripButton* toolStripButtonRemoveStamp;
        ToolStripButton* toolStripButtonDuplicateStamp;
        ToolStripButton* toolStripButtonMoveStampUp;
        ToolStripButton* toolStripButtonMoveStampDown;
        Button* buttonCreateStampFromSelection;
        Label* labelOptions;
        Button* buttonRenameCurrentStamp;
        Button* buttonSaveStampImageToFile;
        Label* labelInfo;
        Label* labelSizeInfo;
        StampDisplay* stampDisplay;

        struct Form_PromptStampName : Form {
            TextboxBase* textBoxName;
            Button* buttonOK;
            Button* buttonCancel;
            Label* labelName;
            Label* labelNoUndo;

            Form_PromptStampName(CString title) : Form(250, 80, title) {
                labelName = new Label("Name:");
                labelName->Location = { 10, 10 };
                labelName->Location.Y += (25 - labelName->Size.Get().H) / 2;

                textBoxName = new TextboxBase("");
                textBoxName->Location = { 60, 10 };
                textBoxName->Size = { 90, 25 };

                buttonCancel = new Button("Cancel");
                buttonCancel->Result = DialogResult::Cancel;
                buttonCancel->Location = { internal_Size.W - 100 - 10, internal_Size.H - 25 - 10 };
                buttonCancel->Size = { 100, 25 };
                buttonCancel->onClick += [this](auto object, auto e) -> void {
                    this->Result = DialogResult::Cancel;
                    this->Close();
                };

                buttonOK = new Button("OK");
                buttonOK->Result = DialogResult::OK;
                buttonOK->Location = { buttonCancel->Location.X - 100 - 10, buttonCancel->Location.Y };
                buttonOK->Size = { 100, 25 };
                buttonOK->onClick += [this](auto object, auto e) -> void {
                    this->Result = DialogResult::OK;
                    this->Close();
                };

                this->Controls.Add(labelName);
                this->Controls.Add(textBoxName);
                this->Controls.Add(buttonOK);
                this->Controls.Add(buttonCancel);
            }
            ~Form_PromptStampName() {
                delete textBoxName;
                delete buttonOK;
                delete buttonCancel;
                delete labelName;
                delete labelNoUndo;
            }
        };

        StampCollection(SceneEditor* editor) : FlowLayoutPanel() {
            Editor = editor;

            Dock = DOCK_FILL;
            BackColor = Color(0x282C34, 0xFF);
            ForeColor = Color(0xFFFFFF, 0xFF);
            WrapContents = false;

            Padding = 8;

            // labelStamps
            labelStamps = new Label("Stamps");
            labelStamps->Anchor = ANCHOR_LEFT;

            // listViewStamps
            listViewStamps = new ListView();
            listViewStamps->LayoutType = ListViewLayout::List;
            listViewStamps->Columns.Add(new ColumnHeader("L", 20, 1));
            listViewStamps->Columns.Add(new ColumnHeader("V", 20, 2));
            listViewStamps->Columns.Add(new ColumnHeader("Name", -1, 0));
            listViewStamps->onSelectedIndexChanged += std::bind(&StampCollection::listView1_onSelectedIndexChanged, this, std::placeholders::_1, std::placeholders::_2);

            listViewStamps->Dock = DOCK_FILL;
            listViewStamps->Margin.Top = 4;
            listViewStamps->Size = { 0, listViewStamps->ItemSize * 10 };

            // toolStripStamps
            toolStripButtonAddStamp = new ToolStripButton();
            toolStripButtonAddStamp->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonAddStamp->Icon, "Resources_Editor/ICON_ADD.png");
            toolStripButtonAddStamp->onMouseClick += std::bind(&StampCollection::toolStripButtonAddStamp_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            toolStripButtonAddStamp->SetToolTipText("Add Stamp From Tile Selection");

            toolStripButtonRemoveStamp = new ToolStripButton();
            toolStripButtonRemoveStamp->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonRemoveStamp->Icon, "Resources_Editor/ICON_DELETE.png");
            toolStripButtonRemoveStamp->onMouseClick += std::bind(&StampCollection::toolStripButtonRemoveStamp_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);

            toolStripButtonDuplicateStamp = new ToolStripButton();
            toolStripButtonDuplicateStamp->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonDuplicateStamp->Icon, "Resources_Editor/ICON_DUPLICATE.png");
            toolStripButtonDuplicateStamp->onMouseClick += std::bind(&StampCollection::toolStripButtonDuplicateStamp_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);

            toolStripButtonMoveStampUp = new ToolStripButton();
            toolStripButtonMoveStampUp->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonMoveStampUp->Icon, "Resources_Editor/ICON_MOVE_UP.png");
            toolStripButtonMoveStampUp->onMouseClick += std::bind(&StampCollection::toolStripButtonMoveStampUp_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);

            toolStripButtonMoveStampDown = new ToolStripButton();
            toolStripButtonMoveStampDown->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonMoveStampDown->Icon, "Resources_Editor/ICON_MOVE_DOWN.png");
            toolStripButtonMoveStampDown->onMouseClick += std::bind(&StampCollection::toolStripButtonMoveStampDown_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);

            toolStripStamps = new ToolStrip();
            toolStripStamps->BackColor = BackColor;
            toolStripStamps->Controls.Add(toolStripButtonAddStamp);
            toolStripStamps->Controls.Add(toolStripButtonRemoveStamp);
            toolStripStamps->Controls.Add(toolStripButtonDuplicateStamp);
            toolStripStamps->Controls.Add(toolStripButtonMoveStampUp);
            toolStripStamps->Controls.Add(toolStripButtonMoveStampDown);
            toolStripStamps->Size = { 200, 20 };

            // buttonCreateStampFromSelection
            buttonCreateStampFromSelection = new Button("Create Stamp From Selection...");
            buttonCreateStampFromSelection->Anchor = ANCHOR_LEFT;
            buttonCreateStampFromSelection->Margin.Top = 8;
            buttonCreateStampFromSelection->Size = { 200, 25 };

            // labelOptions
            labelOptions = new Label("Options");
            labelOptions->Anchor = ANCHOR_LEFT;
            labelOptions->Margin.Top = 8;

            // buttonRenameCurrentStamp
            buttonRenameCurrentStamp = new Button("Rename Current Stamp...");
            // buttonRenameCurrentStamp->Anchor = ANCHOR_LEFT;
            buttonRenameCurrentStamp->Margin.Top = 4;
            buttonRenameCurrentStamp->Size = { 200, 25 };
            buttonRenameCurrentStamp->onMouseClick += std::bind(&StampCollection::buttonRenameCurrentStamp_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);

            // buttonSaveStampImageToFile
            buttonSaveStampImageToFile = new Button("Save Stamp Image to File...");
            // buttonSaveStampImageToFile->Anchor = ANCHOR_LEFT;
            buttonSaveStampImageToFile->Margin.Top = 4;
            buttonSaveStampImageToFile->Size = { 200, 25 };
            buttonSaveStampImageToFile->Enabled = false;

            // labelInfo
            labelInfo = new Label("Info");
            labelInfo->Anchor = ANCHOR_LEFT;
            labelInfo->Margin.Top = 8;

            // labelSizeInfo
            labelSizeInfo = new Label("Size: ? x ? tiles");
            labelSizeInfo->Anchor = ANCHOR_LEFT;
            labelSizeInfo->Margin.Top = 4;

            // stampDisplay
            stampDisplay = new StampDisplay(editor);
            stampDisplay->BackColor = Color(0xFF00FF, 0xFF);
            stampDisplay->Margin.Top = 4;
            stampDisplay->Size = { 200, 200 };

            Controls.Add(labelStamps);
            Controls.Add(listViewStamps);
            Controls.Add(toolStripStamps);
            // Controls.Add(buttonCreateStampFromSelection);
            Controls.Add(labelOptions);
            Controls.Add(buttonRenameCurrentStamp);
            Controls.Add(buttonSaveStampImageToFile);
            Controls.Add(labelInfo);
            Controls.Add(labelSizeInfo);
            Controls.Add(stampDisplay);

            UpdateList();
        }
        ~StampCollection() {
            delete labelStamps;
            delete listViewStamps;
            delete toolStripStamps;
            delete toolStripButtonAddStamp;
            delete toolStripButtonRemoveStamp;
            delete toolStripButtonDuplicateStamp;
            delete toolStripButtonMoveStampUp;
            delete toolStripButtonMoveStampDown;
            delete buttonCreateStampFromSelection;
            delete labelOptions;
            delete buttonRenameCurrentStamp;
            delete buttonSaveStampImageToFile;
            delete labelInfo;
            delete labelSizeInfo;
            delete stampDisplay;
        }


        void listView1_onSelectedIndexChanged(void* sender, EventArgs* args) {
            UpdateSelectedStampUI();
        }
        void buttonRenameCurrentStamp_onMouseClick(void* sender, MouseEventArgs* args) {
            char stringBuffer[256];
            int index = listViewStamps->SelectedIndex;
            if (index < 0)
                return;

            Strings::ToCString(stringBuffer, &Editor->Stamps[index]->Title);

            Form_PromptStampName* dialog = new Form_PromptStampName("Rename Stamp");
            dialog->BackColor = BackColor;

            dialog->textBoxName->InsertText(0, stringBuffer, strlen(stringBuffer));

            UI::System::Application::ShowDialog(dialog, [this, dialog, index](DialogResult result) -> void {
                if (result == DialogResult::OK) {
                    Strings::Copy(&Editor->Stamps[index]->Title, &dialog->textBoxName->Text);
                    UpdateList();
                    listViewStamps->Select(index);
                }
            });
        }
        void toolStripButtonAddStamp_onMouseClick(void* sender, MouseEventArgs* args) {
            auto tileSelBounds = Editor->tilePlacementField->TileSelectBounds;
            if (tileSelBounds.w <= 0 || tileSelBounds.h <= 0)
                return;

            Form_PromptStampName* dialog = new Form_PromptStampName("Add New Stamp From Selection");
            dialog->BackColor = BackColor;

            dialog->textBoxName->InsertText(0, "New Stamp", strlen("New Stamp"));

            UI::System::Application::ShowDialog(dialog, [this, dialog, tileSelBounds](DialogResult result) -> void {
                if (result == DialogResult::OK) {
                    char stringBuffer[256];
                    Strings::ToCString(stringBuffer, &dialog->textBoxName->Text);
                    Editor->StampCollectionAdd(stringBuffer, Stamp::FromLayer(Editor,
                        Editor->tilePlacementField->CurrentLayer, tileSelBounds.x, tileSelBounds.y, tileSelBounds.w, tileSelBounds.h));
                }
            });
        }
        void toolStripButtonRemoveStamp_onMouseClick(void* sender, MouseEventArgs* args) {
            int index = listViewStamps->SelectedIndex;
            if (index >= 0) {
                SavedStamp* temp = Editor->Stamps[index];
                Editor->Stamps.RemoveAt(index);
                // delete temp;
                // NOTE: if this is the current stamp for placement it should be removed from there

                UpdateList();
                listViewStamps->SelectedIndex = M_MIN(index, Editor->Stamps.Count() - 1);
            }
        }
        void toolStripButtonDuplicateStamp_onMouseClick(void* sender, MouseEventArgs* args) {
            int index = listViewStamps->SelectedIndex;
            if (index >= 0) {
                Editor->StampCollectionDuplicate(index);
                UpdateList();
            }
        }
        void toolStripButtonMoveStampUp_onMouseClick(void* sender, MouseEventArgs* args) {
            int index = listViewStamps->SelectedIndex;
            if (index >= 0) {
                if (index > 0) {
                    SavedStamp* temp = Editor->Stamps[index];
                    Editor->Stamps.RemoveAt(index);
                    Editor->Stamps.Insert(index - 1, temp);

                    UpdateList();
                    listViewStamps->SelectedIndex = index - 1;
                }
            }
        }
        void toolStripButtonMoveStampDown_onMouseClick(void* sender, MouseEventArgs* args) {
            int index = listViewStamps->SelectedIndex;
            if (index >= 0) {
                if (index < Editor->Stamps.Count() - 1) {
                    SavedStamp* temp = Editor->Stamps[index];
                    Editor->Stamps.RemoveAt(index);
                    Editor->Stamps.Insert(index + 1, temp);

                    UpdateList();
                    listViewStamps->SelectedIndex = index + 1;
                }
            }

        }

        void UpdateSelectedStampUI() {
            int index = listViewStamps->SelectedIndex;
            if (index >= 0) {
                delete Editor->tilePlacementField->StampDataToBePlaced;
                Editor->tilePlacementField->StampDataToBePlaced = Stamp::Clone(Editor->Stamps[index]->Data);
                Editor->tilePlacementField->SelectTool(TilePlacementField::TOOL_TILE_STAMP);

                stampDisplay->CurrentStamp = Editor->Stamps[index]->Data;

                char bufferString[256];
                sprintf(bufferString, "Size: %d x %d tiles", Editor->Stamps[index]->Data->Width, Editor->Stamps[index]->Data->Height);
                labelSizeInfo->SetText(bufferString);
            }

            Editor->tilePlacementField->UpdateRenderTarget = true;
        }
        void UpdateList() {
            for (int i = 0; i < listViewStamps->Items.Count(); i++)
                delete listViewStamps->Items[i];

            listViewStamps->Items.Clear();
            for (int i = 0; i < Editor->Stamps.Count(); i++)
                listViewStamps->Items.Add(new ListViewItem(&Editor->Stamps[i]->Title));

            listViewStamps->SelectedIndex = -1;
        }

        void Render() {
            Panel::Render();
        }
    };
    struct TileCollisionEditor : Panel {
        struct TileDrawingWidget : Control {
            enum class EditMode {
                Collision,
                Angle,
            };

            TileCollisionEditor* tileCollisionEditor = NULL;
            EditMode editMode = EditMode::Collision;

            MouseEventArgs dragStart = { };
            Vector2 dragPxStart;
            Vector2 dragPxEnd;

            TileDrawingWidget(TileCollisionEditor* tileCollisionEditor) : Control() {
                this->tileCollisionEditor = tileCollisionEditor;
            }

            int GetPlane() {
                if (tileCollisionEditor->radioButtonShowA->Checked)
                    return 0;
                if (tileCollisionEditor->radioButtonShowB->Checked)
                    return 1;

                return 0;
            }

            void MouseSelect(MouseEventArgs* e) {
                auto bounds = GetScreenRect();
                TileSelector* tileSelector = tileCollisionEditor->tileSelector;
                SceneEditor* editor = tileCollisionEditor->Editor;
                int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
                int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

                int p = GetPlane();

                int column = (e->X - bounds.x) * 16 / bounds.w;
                int row = (e->Y - bounds.y) * 16 / bounds.h;
                if (column < 0 || column >= 16)
                    return;
                if (row < 0 || row >= 16)
                    return;

                dragPxEnd = { (e->X - bounds.x), (e->Y - bounds.y) };

                if (editor->LinkedStage) {
                    if (tileSelector->SelectedTileID >= 0) {
                        if (editMode == EditMode::Collision) {
                            if (e->Button == SDL_BUTTON(SDL_BUTTON_LEFT)) {
                                for (int t = tS; t <= tE; t++) {
                                    Stage::EditableTileConfig* tileData = &editor->LinkedStage->TileCfg[p][t];
                                    tileData->Collision[column] = row;
                                }
                            }
                            else if (e->Button == SDL_BUTTON(SDL_BUTTON_RIGHT)) {
                                for (int t = tS; t <= tE; t++) {
                                    Stage::EditableTileConfig* tileData = &editor->LinkedStage->TileCfg[p][t];
                                    tileData->Collision[column] = 0xFF;
                                }
                            }
                        }
                        else if (editMode == EditMode::Angle) {
                            int columnS = (dragStart.X - bounds.x) * 16 / bounds.w;
                            int rowS = (dragStart.Y - bounds.y) * 16 / bounds.h;
                            if (columnS < 0 || columnS >= 16)
                                return;
                            if (rowS < 0 || rowS >= 16)
                                return;

                            for (int t = tS; t <= tE; t++) {
                                Stage::EditableTileConfig* tileData = &editor->LinkedStage->TileCfg[p][t];
                                int newAngle = Math::ATan(dragPxEnd.X - dragPxStart.X, dragPxEnd.Y - dragPxStart.Y);

                                switch (editor->tileCollisionEditor->GetAngleEditSide()) {
                                case 0: tileData->AngleTop = newAngle; break;
                                case 1: tileData->AngleLeft = newAngle; break;
                                case 2: tileData->AngleRight = newAngle; break;
                                case 3: tileData->AngleBottom = newAngle; break;
                                }
                            }
                        }
                    }
                }
            }

            void OnMouseDown(MouseEventArgs* e) {
                const Uint8* state = SDL_GetKeyboardState(NULL);
                if (state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT])
                    editMode = EditMode::Angle;
                else
                    editMode = EditMode::Collision;

                if (CaptureMouse()) {
                    dragStart = *e;

                    MouseSelect(e);

                    auto bounds = GetScreenRect();
                    dragPxStart = { (e->X - bounds.x), (e->Y - bounds.y) };
                }
            }
            void OnMouseMove(MouseEventArgs* e) {
                if (MouseCaptured == this) {
                    MouseSelect(e);
                }
            }
            void OnMouseUp(MouseEventArgs* e) {
                if (MouseCaptured == this) {
                    TileSelector* tileSelector = tileCollisionEditor->tileSelector;
                    SceneEditor* editor = tileCollisionEditor->Editor;
                    int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
                    int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

                    int p = GetPlane();

                    if (editMode == EditMode::Collision) {
                        for (int t = tS; t <= tE; t++)
                            editor->LinkedStage->UpdateTileCollisionTexture(p, t);

                        editor->tilePlacementField->UpdateRenderTarget = true;
                    }

                    UncaptureMouse();

                    editMode = EditMode::Collision;
                }
            }

            void DrawCheckedRect(int x, int y, int w, int h, int oddMod) {
                const Color white = Color(0xFFFFFF, 0x80);
                const Color black = Color(0x000000, 0x80);

                // for (int xx = x; xx < x + w; xx++) {
                //     for (int yy = y; yy < y + h; yy++) {
                //         UI::Graphics::Renderer::DrawRect(xx, yy, 1, 1, ((xx + yy) & 1) ? black : white);
                //     }
                // }
                UI::Graphics::Renderer::DrawRect(x, y, w, h, white);
            }

            void Render() {
                auto bounds = GetScreenRect();

                const Color gridColor = Color(0x808080, 0x80);

                const Color greay = Color(0x808080, 0xFF);
                const Color white = Color(0xFFFFFF, 0xFF);
                TileSelector* tileSelector = tileCollisionEditor->tileSelector;
                SceneEditor* editor = tileCollisionEditor->Editor;
                bool showGrid = tileCollisionEditor->checkBoxShowGrid->GetChecked();

                int p = GetPlane();

                UI::Graphics::Renderer::DrawRect(&bounds, Color(0x000000, 0xFF));

                if (editor->LinkedStage) {
                    if (tileSelector->SelectedTileID >= 0) {
                        const int TileSize = 16;
                        const int columnMask = 63;
                        const int columnCount = 64;
                        const int columnBitshift = 6;
                        int t = tileSelector->SelectedTileID;

                        int pxSz = bounds.w / TileSize;

                        SDL_Rect src = { (t & columnMask) << 4, (t >> columnBitshift) << 4, TileSize, TileSize };
                        SDL_Rect dst = bounds;
                        UI::Graphics::Renderer::DstRectAdjustment(&dst);
                        SDL_RenderCopyEx(UI::Graphics::Renderer::Renderer, editor->LinkedStage->TileImageTextures[0], &src, &dst, 0.0, NULL, SDL_FLIP_NONE);

                        Stage::EditableTileConfig* tileData = &editor->LinkedStage->TileCfg[p][t];

                        if (editMode == EditMode::Collision) {
                            if (tileData->Orientation) {
                                // Top anchored
                                for (int column = 0; column < TILE_SIZE; column++) {
                                    auto col = tileData->Collision[column];
                                    auto colh = col * pxSz + pxSz;
                                    if (col != 0xFF)
                                        DrawCheckedRect(bounds.x + column * pxSz, bounds.y, pxSz, colh, 0);
                                }
                            }
                            else {
                                // Bottom anchored
                                for (int column = 0; column < TILE_SIZE; column++) {
                                    auto col = tileData->Collision[column];
                                    auto colh = col * pxSz;
                                    if (col != 0xFF)
                                        DrawCheckedRect(bounds.x + column * pxSz, bounds.y + colh, pxSz, bounds.h - colh, 0);
                                }
                            }

                            if (showGrid) {
                                for (int column = 0; column < TILE_SIZE; column++) {
                                    UI::Graphics::Renderer::StrokeRect(bounds.x, bounds.y + column * pxSz - 1, bounds.w, 1, gridColor);
                                    UI::Graphics::Renderer::StrokeRect(bounds.x + column * pxSz - 1, bounds.y, 1, bounds.h, gridColor);
                                }
                            }
                        }
                        else {
                            UI::Graphics::Renderer::DrawLine(
                                bounds.x + dragPxStart.X, bounds.y + dragPxStart.Y,
                                bounds.x + dragPxEnd.X, bounds.y + dragPxEnd.Y, Color(0xFF0000, 0xFF));
                        }
                    }
                }

                UI::Graphics::Renderer::StrokeRect(bounds.x - 1, bounds.y - 1, bounds.w + 2, bounds.h + 2, greay);
            }
        };

        SceneEditor* Editor = NULL;

        TileSelector* tileSelector = NULL;
        SplitContainer* splitter = NULL;
        TileDrawingWidget* tilePreviewWindow = NULL;
        CheckBox* checkBoxShowTile = NULL;
        CheckBox* checkBoxShowCollision = NULL;
        Label* labelShowPlane = NULL;
        RadioButton* radioButtonShowA = NULL;
        RadioButton* radioButtonShowB = NULL;
        Label* labelEditAngle = NULL;
        RadioButton* radioButtonAngleTop = NULL;
        RadioButton* radioButtonAngleBottom = NULL;
        RadioButton* radioButtonAngleLeft = NULL;
        RadioButton* radioButtonAngleRight = NULL;
		RadialKnob* radialKnobAngle = NULL;
        Label* labelRawAngleValue = NULL;
        Label* labelAutoCollision = NULL;
        Button* buttonSetCollisionForSelectedRange = NULL;
        Label* labelAutoCollisionNote = NULL;
        Label* labelManualSettings = NULL;
        Label* labelOrientation = NULL;
        ComboBox* comboboxOrientation = NULL;
        Label* labelBehaviorFlag = NULL;
        NumericUpDown* numericUpDownBoxBehaviorFlag = NULL;
        CheckBox* checkBoxShowGrid = NULL;

        // These are not allocated, do not 'delete'!
        RadioButton* SelectionGroup1 = NULL;
        RadioButton* SelectionGroup2 = NULL;

        TileCollisionEditor(SceneEditor* editor) : Panel() {
            Editor = editor;

            DoHScroll = false;
            DoVScroll = true;

            BackColor = Color(0x282C34, 0xFF);

            splitter = new SplitContainer();
            splitter->Dock = DOCK_FILL;
            splitter->BackColor = Color(0x000000, 0xFF);
            splitter->Panel1->BackColor = Color(0x000000, 0x00);
            splitter->Panel2->BackColor = BackColor;
            splitter->Orientation = SplitOrientation::Vertical;
            splitter->IsSplitterFixed = true;
            splitter->FixedPanel = SplitPanelFix::Panel2;
            splitter->SplitterWidth = 1;
            splitter->SplitterDistance = 2000;
            Controls.Add(splitter);

            tileSelector = new TileSelector(editor);
            tileSelector->ShowTileCollision = true;
            tileSelector->onSelectedTileIDChanged += std::bind(&TileCollisionEditor::tileSelector_onSelectedTileIDChanged, this, std::placeholders::_1, std::placeholders::_2);
            splitter->Panel1->Controls.Add(tileSelector);

            checkBoxShowTile = new CheckBox("Show Tile:");
            checkBoxShowTile->CheckState = CheckState::Unchecked;
            checkBoxShowTile->Location = { 8, 8 };
            checkBoxShowTile->onCheckedChanged += std::bind(&TileCollisionEditor::checkBoxShowTile_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
            checkBoxShowTile->CheckAlign = TEXT_ALIGN_RIGHT | TEXT_VALIGN_MIDDLE;
            checkBoxShowTile->Padding = 0;
            checkBoxShowTile->Padding.Left = 8;
            splitter->Panel2->Controls.Add(checkBoxShowTile);

            checkBoxShowCollision = new CheckBox("Show Collision:");
            checkBoxShowCollision->CheckState = CheckState::Checked;
            checkBoxShowCollision->Location = { checkBoxShowTile->Location.X + 100, checkBoxShowTile->Location.Y };
            checkBoxShowCollision->onCheckedChanged += std::bind(&TileCollisionEditor::checkBoxShowTile_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
            checkBoxShowCollision->CheckAlign = TEXT_ALIGN_RIGHT | TEXT_VALIGN_MIDDLE;
            checkBoxShowCollision->Padding = 0;
            checkBoxShowCollision->Padding.Left = 8;
            splitter->Panel2->Controls.Add(checkBoxShowCollision);

            labelShowPlane = new Label("Show Plane:");
            labelShowPlane->Location = { 8, 8 + 28 };
            splitter->Panel2->Controls.Add(labelShowPlane);

            radioButtonShowA = new RadioButton("A");
            radioButtonShowA->Checked = false;
            radioButtonShowA->Location = { 8 + 80, 8 + 28 };
            radioButtonShowA->onCheckedChanged += std::bind(&TileCollisionEditor::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
            radioButtonShowA->SelectionGroup = &SelectionGroup1;
            splitter->Panel2->Controls.Add(radioButtonShowA);

            radioButtonShowB = new RadioButton("B");
            radioButtonShowB->Checked = false;
            radioButtonShowB->Location = { 8 + 80 + 50, 8 + 28 };
            radioButtonShowB->onCheckedChanged += std::bind(&TileCollisionEditor::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
            radioButtonShowB->SelectionGroup = &SelectionGroup1;
            splitter->Panel2->Controls.Add(radioButtonShowB);

            labelEditAngle = new Label("Edit Angle:");
            labelEditAngle->Location = { 8, 8 + 28 + 28 + 14 };
            splitter->Panel2->Controls.Add(labelEditAngle);

            radioButtonAngleTop = new RadioButton("Main"); // "Top"
            radioButtonAngleTop->Checked = false;
            radioButtonAngleTop->Location = { 8 + 65, 8 + 28 + 28 };
            radioButtonAngleTop->onCheckedChanged += std::bind(&TileCollisionEditor::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
            radioButtonAngleTop->SelectionGroup = &SelectionGroup2;
            splitter->Panel2->Controls.Add(radioButtonAngleTop);

            radioButtonAngleBottom = new RadioButton("Unused"); // "Bottom"
            radioButtonAngleBottom->Checked = false;
            radioButtonAngleBottom->Location = { 8 + 65 + 80, 8 + 28 + 28 };
            radioButtonAngleBottom->onCheckedChanged += std::bind(&TileCollisionEditor::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
            radioButtonAngleBottom->SelectionGroup = &SelectionGroup2;
            splitter->Panel2->Controls.Add(radioButtonAngleBottom);

            radioButtonAngleLeft = new RadioButton("Unused"); // "Left"
            radioButtonAngleLeft->Checked = false;
            radioButtonAngleLeft->Location = { 8 + 65, 8 + 28 + 28 + 28 };
            radioButtonAngleLeft->onCheckedChanged += std::bind(&TileCollisionEditor::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
            radioButtonAngleLeft->SelectionGroup = &SelectionGroup2;
            splitter->Panel2->Controls.Add(radioButtonAngleLeft);

            radioButtonAngleRight = new RadioButton("Unused"); // "Right"
            radioButtonAngleRight->Checked = false;
            radioButtonAngleRight->Location = { 8 + 65 + 80, 8 + 28 + 28 + 28 };
            radioButtonAngleRight->onCheckedChanged += std::bind(&TileCollisionEditor::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
            radioButtonAngleRight->SelectionGroup = &SelectionGroup2;
            splitter->Panel2->Controls.Add(radioButtonAngleRight);

            radioButtonAngleBottom->Enabled = false;
            radioButtonAngleLeft->Enabled = false;
            radioButtonAngleRight->Enabled = false;

            radialKnobAngle = new RadialKnob();
            radialKnobAngle->Location = { radioButtonAngleBottom->Location.X + 80, radioButtonAngleBottom->Location.Y };
			radialKnobAngle->Size = { 48, 48 };
			radialKnobAngle->MaxAngle = 256.0;
            radialKnobAngle->Bias = 64.0;
			radialKnobAngle->onDialTurn += std::bind(&TileCollisionEditor::radialKnobAngle_onDialTurn, this, std::placeholders::_1, std::placeholders::_2);
            radialKnobAngle->onValueChanged += std::bind(&TileCollisionEditor::radialKnobAngle_onValueChanged, this, std::placeholders::_1, std::placeholders::_2);
            splitter->Panel2->Controls.Add(radialKnobAngle);

            labelRawAngleValue = new Label("Angle:  0 degrees (0x00)");
			labelRawAngleValue->ForeColor = Color(0xFFFFFF, 0x7F);
            labelRawAngleValue->Location = { 8 + 65, 8 + 28 + 28 + 28 + 28 };
            splitter->Panel2->Controls.Add(labelRawAngleValue);

            labelAutoCollision = new Label("AutoCollision (Loose Fit)");
            labelAutoCollision->Location = { 8, labelRawAngleValue->Location.Y + 28 };
            splitter->Panel2->Controls.Add(labelAutoCollision);

            buttonSetCollisionForSelectedRange = new Button("Set Collision For Selected Range");
            // buttonSetCollisionForSelectedRange->Anchor = ANCHOR_LEFT;
            buttonSetCollisionForSelectedRange->Location = { 8, labelAutoCollision->Location.Y + 25 };
            buttonSetCollisionForSelectedRange->Margin.Top = 4;
            buttonSetCollisionForSelectedRange->Size = { 200, 25 };
            buttonSetCollisionForSelectedRange->onMouseClick += std::bind(&TileCollisionEditor::buttonSetCollisionForSelectedRange_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            splitter->Panel2->Controls.Add(buttonSetCollisionForSelectedRange);

            labelAutoCollisionNote = new Label("*sets the values based on the tile image.");
            labelAutoCollisionNote->ForeColor = Color(0xFFFFFF, 0x7F);
            labelAutoCollisionNote->Location = { 8, buttonSetCollisionForSelectedRange->Location.Y + 29 };
            splitter->Panel2->Controls.Add(labelAutoCollisionNote);

            labelManualSettings = new Label("Manual Settings");
            labelManualSettings->Location = { 8, labelAutoCollisionNote->Location.Y + 28 };
            splitter->Panel2->Controls.Add(labelManualSettings);

            //*
            tilePreviewWindow = new TileDrawingWidget(this);
            tilePreviewWindow->Dock = DOCK_NONE;
            tilePreviewWindow->Location = { 8, labelManualSettings->Location.Y + 28 };
            tilePreviewWindow->Size = { 176, 176 };
            splitter->Panel2->Controls.Add(tilePreviewWindow);
            //*/

            labelOrientation = new Label("Orientation:");
            labelOrientation->Location = { tilePreviewWindow->Location.X + tilePreviewWindow->Size.Get().W + 8, tilePreviewWindow->Location.Y };
            splitter->Panel2->Controls.Add(labelOrientation);

            comboboxOrientation = new ComboBox();
            comboboxOrientation->Location = { labelOrientation->Location.X, labelOrientation->Location.Y + 20 };
            comboboxOrientation->Size = { 100, 25 };
            comboboxOrientation->Items.Add("FLOOR");
            comboboxOrientation->Items.Add("CEILING");
            comboboxOrientation->Select(0);
            comboboxOrientation->onSelectedIndexChanged += std::bind(&TileCollisionEditor::comboboxOrientation_onSelectedIndexChanged, this, std::placeholders::_1, std::placeholders::_2);
            splitter->Panel2->Controls.Add(comboboxOrientation);

            labelBehaviorFlag = new Label("Behavior Flag:");
            labelBehaviorFlag->Location = { comboboxOrientation->Location.X, comboboxOrientation->Location.Y + 28 };
            splitter->Panel2->Controls.Add(labelBehaviorFlag);

            numericUpDownBoxBehaviorFlag = new NumericUpDown();
            numericUpDownBoxBehaviorFlag->Hexadecimal = true;
            numericUpDownBoxBehaviorFlag->Minimum = 0.0f;
            numericUpDownBoxBehaviorFlag->Maximum = 255.0f;
            numericUpDownBoxBehaviorFlag->Location = { labelBehaviorFlag->Location.X, labelBehaviorFlag->Location.Y + 20 };
            numericUpDownBoxBehaviorFlag->Size = { 100, 25 };
            numericUpDownBoxBehaviorFlag->onValueChanged += std::bind(&TileCollisionEditor::numericUpDownBoxBehaviorFlag_onValueChanged, this, std::placeholders::_1, std::placeholders::_2);
            splitter->Panel2->Controls.Add(numericUpDownBoxBehaviorFlag);

            checkBoxShowGrid = new CheckBox("Show Grid:");
            checkBoxShowGrid->CheckState = CheckState::Unchecked;
            checkBoxShowGrid->Location = { numericUpDownBoxBehaviorFlag->Location.X, numericUpDownBoxBehaviorFlag->Location.Y + 28 };
            // checkBoxShowGrid->onCheckedChanged += std::bind(&TileCollisionEditor::checkBoxShowTile_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
            checkBoxShowGrid->CheckAlign = TEXT_ALIGN_RIGHT | TEXT_VALIGN_MIDDLE;
            checkBoxShowGrid->Padding = 0;
            checkBoxShowGrid->Padding.Left = 8;
            splitter->Panel2->Controls.Add(checkBoxShowGrid);

            ///
            splitter->Panel2MinSize = tilePreviewWindow->Location.Y + tilePreviewWindow->Size.Get().H + 8;

            ///
            radioButtonShowA->Check();
            radioButtonAngleTop->Check();
        }
        ~TileCollisionEditor() {
            delete tileSelector;
            delete splitter;
            delete tilePreviewWindow;
            delete checkBoxShowTile;
            delete checkBoxShowCollision;
            delete labelShowPlane;
            delete radioButtonShowA;
            delete radioButtonShowB;
            delete labelEditAngle;
            delete radioButtonAngleTop;
            delete radioButtonAngleBottom;
            delete radioButtonAngleLeft;
            delete radioButtonAngleRight;
    		delete radialKnobAngle;
            delete labelRawAngleValue;
            delete labelAutoCollision;
            delete buttonSetCollisionForSelectedRange;
            delete labelAutoCollisionNote;
            delete labelManualSettings;
            delete labelOrientation;
            delete comboboxOrientation;
            delete labelBehaviorFlag;
            delete numericUpDownBoxBehaviorFlag;
            delete checkBoxShowGrid;
        }

        void UpdateAngleLabel(int newAngle) {
            char textBuffer[256];
            int newAngleDeg = (int)(newAngle * 360.0 / radialKnobAngle->MaxAngle);
            snprintf(textBuffer, 255, "Angle:  %d degrees (0x%02X)", newAngleDeg, newAngle);
            labelRawAngleValue->SetText(textBuffer);
        }
		void UpdateTileInfoUI() {
			if (tileSelector->SelectedTileID < 0)
				return;
            if (!Editor->LinkedStage)
                return;

			int p = tilePreviewWindow->GetPlane();
			int t = tileSelector->SelectedTileID;
            Stage::EditableTileConfig* tileData = &Editor->LinkedStage->TileCfg[p][t];

			int newAngle = -1;
            switch (GetAngleEditSide()) {
            case 0: newAngle = tileData->AngleTop; break;
            case 1: newAngle = tileData->AngleLeft; break;
            case 2: newAngle = tileData->AngleRight; break;
            case 3: newAngle = tileData->AngleBottom; break;
            }

            comboboxOrientation->CanRaiseEvents = false;
            comboboxOrientation->Select(tileData->Orientation);
            comboboxOrientation->CanRaiseEvents = true;

            radialKnobAngle->CanRaiseEvents = false;
			radialKnobAngle->Angle = newAngle;
            radialKnobAngle->CanRaiseEvents = true;

            numericUpDownBoxBehaviorFlag->CanRaiseEvents = false;
            numericUpDownBoxBehaviorFlag->Value = tileData->Behavior;
            numericUpDownBoxBehaviorFlag->CanRaiseEvents = true;

            UpdateAngleLabel(newAngle);
		}

        int GetAngleEditSide() {
            if (radioButtonAngleTop->Checked)
                return 0;
            if (radioButtonAngleLeft->Checked)
                return 1;
            if (radioButtonAngleRight->Checked)
                return 2;
            if (radioButtonAngleBottom->Checked)
                return 3;
            return -1;
        }

        void DoAutoTile(int i) {
            if (Editor->LinkedStage == NULL)
                return;
            if (Editor->LinkedStage->TileImagePixelData == NULL)
                return;

            int p = tilePreviewWindow->GetPlane();
            Stage::EditableTileConfig* tileData = &Editor->LinkedStage->TileCfg[p][i];

            int tileSize = 16;
            int columnCount = 64;
            int tileSheetWidth = tileSize * columnCount;

            int tileImageX = (i % columnCount) * tileSize;
            int tileImageY = (i / columnCount) * tileSize;
            bool isCeiling = false;
            bool hasCollision = true;
            Uint32* pxData = Editor->LinkedStage->TileImagePixelData;

            // Determine whether is ceiling or not.
            int topCount = 0;
            int bottomCount = 0;
            for (int p = tileImageX; p < tileImageX + tileSize; p++) {
                int px;

                px = (p + (tileImageY) * tileSheetWidth);
                if ((pxData[px] & 0xFF000000) > 0)
                    topCount++;

                px = (p + (tileImageY + tileSize - 1) * tileSheetWidth);
                if ((pxData[px] & 0xFF000000) > 0)
                    bottomCount++;
            }
            if (topCount > bottomCount)
                isCeiling = true;

            tileData->Orientation = isCeiling;
            tileData->AngleTop = 0x00;

            int firstValue = -1;
            int lastValue = -1;
            int slopeRun = 0;

            // Get space lengths.
            if (isCeiling) {
                // If ceiling, start checking from bottom and vice-versa
                int fx = 0;
                for (int p = tileImageX; p < tileImageX + tileSize; p++) {
                    int value = 0xFF;
                    for (int c = 0, pY = (p + (tileImageY + tileSize - 1) * tileSheetWidth);
                        c < tileSize;
                        c++, pY -= tileSheetWidth) {
                        if ((pxData[pY] & 0xFF000000) > 0) {
                            value = tileSize - 1 - c;
                            break;
                        }
                    }

                    tileData->Collision[fx] = value;

                    if (value != 0xFF && value != tileSize - 1) {
                        if (firstValue == -1)
                            firstValue = value + 1;

                        lastValue = value + 1;
                        slopeRun++;
                    }
                    fx++;
                }
            }
            else {
                int fx = 0;
                for (int p = tileImageX; p < tileImageX + tileSize; p++) {
                    int value = 0xFF;
                    for (int c = 0, pY = (p + (tileImageY) * tileSheetWidth);
                        c < tileSize;
                        c++, pY += tileSheetWidth) {
                        if ((pxData[pY] & 0xFF000000) > 0) {
                            value = c;
                            break;
                        }
                    }

                    tileData->Collision[fx] = value;

                    if (value != 0xFF && value != 0) {
                        if (firstValue == -1)
                            firstValue = value - 1;

                        lastValue = value - 1;
                        slopeRun++;
                    }
                    fx++;
                }
            }

            if (firstValue > -1 && lastValue > -1) {
                int slopeRise = lastValue - firstValue;
                if (slopeRise < 0) slopeRise--; else slopeRise++;

                int angle = Math::ATan(slopeRun, slopeRise);
                // printf("Tile %d: F %d L %d Run %d > 0x%02X -> 0x%02X\n", i, firstValue, lastValue, slopeRun, angle, (angle + 2) & 0xFC);
                tileData->AngleTop = (angle + 2) & 0xFC;
            }

            Editor->LinkedStage->UpdateTileCollisionTexture(p, i);
            Editor->tilePlacementField->UpdateRenderTarget = true;
        }

        void buttonSetCollisionForSelectedRange_onMouseClick(void* sender, MouseEventArgs* e) {
            if (tileSelector->SelectedTileID < 0)
                return;
            if (!Editor->LinkedStage)
                return;

            int p = tilePreviewWindow->GetPlane();
            int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
            int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

            for (int t = tS; t <= tE; t++) {
                DoAutoTile(t);
            }

            UpdateTileInfoUI();
        }
        void numericUpDownBoxBehaviorFlag_onValueChanged(void* sender, EventArgs* e) {
            if (tileSelector->SelectedTileID < 0)
                return;
            if (!Editor->LinkedStage)
                return;

            int p = tilePreviewWindow->GetPlane();
            int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
            int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

            for (int t = tS; t <= tE; t++) {
                Stage::EditableTileConfig* tileData = &Editor->LinkedStage->TileCfg[p][t];
                tileData->Behavior = (int)numericUpDownBoxBehaviorFlag->Value;
            }

            UpdateTileInfoUI();
        }
        void comboboxOrientation_onSelectedIndexChanged(void* sender, EventArgs* args) {
            if (tileSelector->SelectedTileID < 0)
                return;
            if (!Editor->LinkedStage)
                return;
            if (comboboxOrientation->SelectedIndex < 0)
                return;

            int p = tilePreviewWindow->GetPlane();
            int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
            int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

            for (int t = tS; t <= tE; t++) {
                Stage::EditableTileConfig* tileData = &Editor->LinkedStage->TileCfg[p][t];
                tileData->Orientation = comboboxOrientation->SelectedIndex;
            }

            UpdateTileInfoUI();
        }
        void tileSelector_onSelectedTileIDChanged(void* sender, EventArgs* args) {
            UpdateTileInfoUI();
        }
        void checkBoxShowTile_onCheckedChanged(void* sender, EventArgs* args) {
            tileSelector->ShowTileGraphics = checkBoxShowTile->GetChecked();
            tileSelector->ShowTileCollision = checkBoxShowCollision->GetChecked();
        }
        void radioButtonShow_onCheckedChanged(void* sender, EventArgs* args) {
            tileSelector->TileCollisionPlane = tilePreviewWindow->GetPlane();

			UpdateTileInfoUI();
        }
		void radialKnobAngle_onValueChanged(void* sender, DialValueChangedArgs* args) {
            if (tileSelector->SelectedTileID < 0)
                return;
            if (!Editor->LinkedStage)
                return;

            int newAngle = (int)args->Value;

            int p = tilePreviewWindow->GetPlane();
            int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
            int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

            for (int t = tS; t <= tE; t++) {
                Stage::EditableTileConfig* tileData = &Editor->LinkedStage->TileCfg[p][t];

                switch (GetAngleEditSide()) {
                case 0: tileData->AngleTop = newAngle; break;
                case 1: tileData->AngleLeft = newAngle; break;
                case 2: tileData->AngleRight = newAngle; break;
                case 3: tileData->AngleBottom = newAngle; break;
                }
            }

			UpdateTileInfoUI();
		}
        void radialKnobAngle_onDialTurn(void* sender, DialTurnedArgs* args) {
            UpdateAngleLabel(args->Value);
        }
    };
    struct TilePlacementToolbar : ToolStrip {
        SceneEditor* Editor = NULL;

        ToolStripButton* toolStripButtonSelect = NULL;
        ToolStripButton* toolStripButtonTileStamp = NULL;
        ToolStripButton* toolStripButtonErase = NULL;
        ToolStripButton* toolStripButtonTileEyedropper = NULL;
        ToolStripButton* toolStripButtonTileBucketFill = NULL;
        ToolStripButton* toolStripButtonTileCollisionBrush = NULL;
        ToolStripSeparator* toolStripSeparator2 = NULL;
        ToolStripButton* toolStripButtonParallaxTool = NULL;
        ToolStripSeparator* toolStripSeparator3 = NULL;
        ToolStripButton* toolStripButtonEntityTool = NULL;

        TilePlacementToolbar(SceneEditor* editor) : ToolStrip() {
            Editor = editor;

            Dock = DOCK_TOP;
            Size = { 32, 32 };

            BackColor = Color(0x282C34, 0xFF);

            Add(toolStripButtonSelect = new ToolStripButton());
            Add(toolStripButtonTileStamp = new ToolStripButton());
            Add(toolStripButtonErase = new ToolStripButton());
            Add(toolStripButtonTileEyedropper = new ToolStripButton());
            toolStripButtonTileBucketFill = new ToolStripButton(); // Add(toolStripButtonTileBucketFill = new ToolStripButton());
            Add(toolStripButtonTileCollisionBrush = new ToolStripButton());
            Add(toolStripSeparator2 = new ToolStripSeparator());
            Add(toolStripButtonParallaxTool = new ToolStripButton());
            Add(toolStripSeparator3 = new ToolStripSeparator());
            Add(toolStripButtonEntityTool = new ToolStripButton());

            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonSelect->Icon, "Resources_Editor/TOOL_SELECT.png");
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonErase->Icon, "Resources_Editor/TOOL_ERASE.png");
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonTileStamp->Icon, "Resources_Editor/TOOL_TILE_STAMP.png");
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

            toolStripButtonTileEyedropper->SetText("Eyedropper");
            toolStripButtonTileBucketFill->SetText("Bucket Fill");
            toolStripButtonTileCollisionBrush->SetText("Collision Brush");
            toolStripButtonParallaxTool->SetText("Parallax Tool");
            toolStripButtonEntityTool->SetText("Entity Tool");

            toolStripButtonSelect->SetToolTipText("Tile Selection Tool (R)");
            toolStripButtonErase->SetToolTipText("Tile Erase Tool (E)");
            toolStripButtonTileStamp->SetToolTipText("Tile Stamp Tool (S)");
            toolStripButtonTileEyedropper->SetToolTipText("Eyedropper");
            toolStripButtonTileBucketFill->SetToolTipText("Bucket Fill");
            toolStripButtonTileCollisionBrush->SetToolTipText("Collision Brush");
            toolStripButtonParallaxTool->SetToolTipText("Parallax Tool");
            toolStripButtonEntityTool->SetToolTipText("Entity Tool");
        }
        ~TilePlacementToolbar() {
	        delete toolStripButtonSelect;
	        delete toolStripButtonTileStamp;
	        delete toolStripButtonErase;
	        delete toolStripButtonTileEyedropper;
	        delete toolStripButtonTileBucketFill;
	        delete toolStripButtonTileCollisionBrush;
	        delete toolStripSeparator2;
	        delete toolStripButtonParallaxTool;
	        delete toolStripSeparator3;
	        delete toolStripButtonEntityTool;
        }
    };
    struct ObjectClasses : FlowLayoutPanel {
        SceneEditor* Editor = NULL;

        // Contains info on currently active filters

        Label* labelObjectList;
        ListView* listViewClasses;
        ToolStrip* toolStripClasses;
        ToolStripButton* toolStripButtonAddClass;
        ToolStripButton* toolStripButtonRemoveClass;
        ToolStripButton* toolStripButtonRenameClass;

        Label* labelPropertySetter;
        ListView* listViewProperties;
        ToolStrip* toolStripProperties;
        ToolStripButton* toolStripButtonAddProperty;
        ToolStripButton* toolStripButtonRemoveProperty;

        Label* labelEntitySettings;
        Button* buttonAddEntity;
        Button* buttonSelectAllEntitiesOfClass;

        struct Form_EditClass : Form {
            Label* labelName;
            TextboxBase* textBoxName;
            Button* buttonOK;
            Button* buttonCancel;

            FlowLayoutPanel* mainPanel;

            Form_EditClass(CString title, CString className) : Form(250, 140, title) {
                mainPanel = new FlowLayoutPanel();
                mainPanel->BackColor = Color(0x000000, 0x00);
                mainPanel->Dock = DOCK_FILL;
                mainPanel->FlowDirection = FlowDirection::LEFT_TO_RIGHT;
                mainPanel->Padding = 10;
                mainPanel->WrapContents = false;


                labelName = new Label("Name:");
                labelName->Anchor = ANCHOR_TOP;
                labelName->Margin.Top = 5;
                labelName->Margin.Right = 10;
                mainPanel->Controls.Add(labelName);

                if (className)
                    textBoxName = new TextboxBase(className);
                else
                    textBoxName = new TextboxBase("ClassName");
                textBoxName->Size = { 90, 25 };
                textBoxName->LineBreak = true;
                mainPanel->Controls.Add(textBoxName);

                buttonOK = new Button("OK");
                buttonOK->Anchor = ANCHOR_TOP;
                buttonOK->Size = { 100, 25 };
                buttonOK->Margin.Right = 5;
                buttonOK->onClick += [this](auto object, auto e) -> void {
                    if (this->textBoxName->Text.Length == 0)
                        return;

                    this->Result = DialogResult::OK;
                    this->Close();
                };
                mainPanel->Controls.Add(buttonOK);

                buttonCancel = new Button("Cancel");
                buttonCancel->Anchor = ANCHOR_TOP;
                buttonCancel->Size = { 100, 25 };
                buttonCancel->onClick += [this](auto object, auto e) -> void {
                    this->Result = DialogResult::Cancel;
                    this->Close();
                };
                mainPanel->Controls.Add(buttonCancel);


                this->Controls.Add(mainPanel);
                this->UpdateLayout(); // This should theoretically happen during Controls.Add

                this->Size = {
                    buttonCancel->Location.X + buttonCancel->Size.Get().W + mainPanel->Padding.Right,
                    buttonCancel->Location.Y + buttonCancel->Size.Get().H + mainPanel->Padding.Bottom
                };
            }
            ~Form_EditClass() {
                delete labelName;
                delete textBoxName;
                delete buttonOK;
                delete buttonCancel;

                delete mainPanel;
            }
        };
        struct Form_EditProperty : Form {
            Label* labelName;
            TextboxBase* textBoxName;
            Label* labelType;
            ComboBox* comboBoxType;
            Button* buttonOK;
            Button* buttonCancel;

            FlowLayoutPanel* mainPanel;

            const int AvailableTypes[8] = {
                VAR_INT32,
                VAR_FLOAT,
                VAR_VECTOR2,
                VAR_BOOL,
                VAR_COLOR,
                VAR_STRING,
            };
            const int AvailableTypeCount = 6;

            Form_EditProperty(CString title, CString propertyName, int type) : Form(250, 140, title) {
                mainPanel = new FlowLayoutPanel();
                mainPanel->BackColor = Color(0x000000, 0x00);
                mainPanel->Dock = DOCK_FILL;
                mainPanel->FlowDirection = FlowDirection::LEFT_TO_RIGHT;
                mainPanel->Padding = 10;
                mainPanel->WrapContents = false;


                labelName = new Label("Name:");
                labelName->Anchor = ANCHOR_TOP;
                labelName->Margin.Top = 5;
                labelName->Margin.Right = 10;
                mainPanel->Controls.Add(labelName);

                if (propertyName)
                    textBoxName = new TextboxBase(propertyName);
                else
                    textBoxName = new TextboxBase("propertyName");
                textBoxName->Size = { 90, 25 };
                textBoxName->LineBreak = true;
                mainPanel->Controls.Add(textBoxName);


                labelType = new Label("Type:");
                labelType->Anchor = ANCHOR_TOP;
                labelType->Margin.Top = 5;
                labelType->Margin.Right = 10;
                mainPanel->Controls.Add(labelType);

                int defaultSelection = 0;
                comboBoxType = new ComboBox();
                for (int i = 0; i < AvailableTypeCount; i++) {
                    comboBoxType->Items.Add(Hatch::GetPropertyTypeString(AvailableTypes[i]));
                    if (AvailableTypes[i] == type)
                        defaultSelection = i;
                }
                comboBoxType->Size = { 90, 25 };
                comboBoxType->LineBreak = true;
                comboBoxType->Select(defaultSelection);
                mainPanel->Controls.Add(comboBoxType);


                buttonOK = new Button("OK");
                buttonOK->Anchor = ANCHOR_TOP;
                buttonOK->Size = { 100, 25 };
                buttonOK->Margin.Right = 5;
                buttonOK->onClick += [this](auto object, auto e) -> void {
                    if (this->textBoxName->Text.Length == 0)
                        return;

                    this->Result = DialogResult::OK;
                    this->Close();
                };
                mainPanel->Controls.Add(buttonOK);

                buttonCancel = new Button("Cancel");
                buttonCancel->Anchor = ANCHOR_TOP;
                buttonCancel->Size = { 100, 25 };
                buttonCancel->onClick += [this](auto object, auto e) -> void {
                    this->Result = DialogResult::Cancel;
                    this->Close();
                };
                mainPanel->Controls.Add(buttonCancel);


                this->Controls.Add(mainPanel);
                this->UpdateLayout();

                this->Size = {
                    buttonCancel->Location.X + buttonCancel->Size.Get().W + mainPanel->Padding.Right,
                    buttonCancel->Location.Y + buttonCancel->Size.Get().H + mainPanel->Padding.Bottom
                };
            }
            ~Form_EditProperty() {
                delete labelName;
                delete textBoxName;
                delete labelType;
                delete comboBoxType;
                delete buttonOK;
                delete buttonCancel;

                delete mainPanel;
            }
        };

        ObjectClasses(SceneEditor* editor) : FlowLayoutPanel() {
            Editor = editor;

            Dock = DOCK_FILL;
            Size = { 32, 32 };
            Padding = 6;

            FlowDirection = FlowDirection::TOP_TO_BOTTOM;

            BackColor = Color(0x282C34, 0xFF);

            // labelObjectList
            labelObjectList = new Label("Class List");
            labelObjectList->Anchor = ANCHOR_LEFT;
            Controls.Add(labelObjectList);

            // listViewObjects
            listViewClasses = new ListView();
            listViewClasses->Margin.Top = 4;
            listViewClasses->LayoutType = ListViewLayout::List;
            listViewClasses->Columns.Add(new ColumnHeader("L", 20, 1));
            listViewClasses->Columns.Add(new ColumnHeader("V", 20, 2));
            listViewClasses->Columns.Add(new ColumnHeader("Name", -1, 0));
            listViewClasses->Size = { 160, listViewClasses->ItemSize * 10 + listViewClasses->HeaderSize };
            listViewClasses->onSelectedIndexChanged += std::bind(&ObjectClasses::listViewClasses_onSelectedIndexChanged, this, std::placeholders::_1, std::placeholders::_2);
            Controls.Add(listViewClasses);

            // toolStripClasses
            toolStripClasses = new ToolStrip();
            toolStripClasses->BackColor = BackColor;
            toolStripClasses->Size = { 200, 20 };
            Controls.Add(toolStripClasses);

            toolStripButtonAddClass = new ToolStripButton();
            toolStripButtonAddClass->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonAddClass->Icon, "Resources_Editor/ICON_ADD.png");
            toolStripButtonAddClass->onMouseClick += std::bind(&ObjectClasses::toolStripButtonAddClass_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            toolStripClasses->Controls.Add(toolStripButtonAddClass);

            toolStripButtonRemoveClass = new ToolStripButton();
            toolStripButtonRemoveClass->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonRemoveClass->Icon, "Resources_Editor/ICON_DELETE.png");
            toolStripButtonRemoveClass->onMouseClick += std::bind(&ObjectClasses::toolStripButtonRemoveClass_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            toolStripClasses->Controls.Add(toolStripButtonRemoveClass);

            toolStripButtonRenameClass = new ToolStripButton();
            toolStripButtonRenameClass->SetText("Rename...");
            // toolStripButtonRenameClass->IconSize = { 11, 11 };
            // Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonRenameClass->Icon, "Resources_Editor/ICON_DELETE.png");
            toolStripButtonRenameClass->onMouseClick += std::bind(&ObjectClasses::toolStripButtonRenameClass_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            // toolStripClasses->Controls.Add(toolStripButtonRenameClass);

            #pragma region "Class Properties" controls
            labelPropertySetter = new Label("Class Properties");
            labelPropertySetter->Anchor = ANCHOR_LEFT;
            labelPropertySetter->Margin.Top = 4;
            Controls.Add(labelPropertySetter);

            listViewProperties = new ListView();
            listViewProperties->Margin.Top = 4;
            listViewProperties->LayoutType = ListViewLayout::Details;
            listViewProperties->Columns.Add(new ColumnHeader("Name", -1, 0));
            listViewProperties->Columns.Add(new ColumnHeader("Type", 60, 1));
            listViewProperties->Size = { 160, listViewProperties->ItemSize * 8 + listViewProperties->HeaderSize };
            Controls.Add(listViewProperties);

            toolStripProperties = new ToolStrip();
            toolStripProperties->BackColor = Color(0x000000, 0x00);
            toolStripProperties->Size = { 200, 20 };
            Controls.Add(toolStripProperties);

            toolStripButtonAddProperty = new ToolStripButton();
            toolStripButtonAddProperty->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonAddProperty->Icon, "Resources_Editor/ICON_ADD.png");
            toolStripButtonAddProperty->onMouseClick += std::bind(&ObjectClasses::toolStripButtonAddProperty_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            toolStripProperties->Controls.Add(toolStripButtonAddProperty);

            toolStripButtonRemoveProperty = new ToolStripButton();
            toolStripButtonRemoveProperty->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonRemoveProperty->Icon, "Resources_Editor/ICON_DELETE.png");
            toolStripButtonRemoveProperty->onMouseClick += std::bind(&ObjectClasses::toolStripButtonRemoveProperty_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            toolStripProperties->Controls.Add(toolStripButtonRemoveProperty);
            #pragma endregion

            // labelEntitySettings
            labelEntitySettings = new Label("Entity Settings");
            labelEntitySettings->Anchor = ANCHOR_LEFT;
            labelEntitySettings->Margin.Top = 4;
            Controls.Add(labelEntitySettings);

            // buttonAddEntity
            buttonAddEntity = new Button("Add Entity");
            buttonAddEntity->Size = { 200, 25 };
            buttonAddEntity->Margin.Top = 4;
            buttonAddEntity->Enabled = false;
            buttonAddEntity->onMouseClick += std::bind(&ObjectClasses::buttonAddEntity_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            Controls.Add(buttonAddEntity);

            // buttonSelectAllEntitiesOfClass
            buttonSelectAllEntitiesOfClass = new Button("Select All Entities");
            buttonSelectAllEntitiesOfClass->Size = { 200, 25 };
            buttonSelectAllEntitiesOfClass->Margin.Top = 4;
            buttonSelectAllEntitiesOfClass->Enabled = false;
            buttonSelectAllEntitiesOfClass->onMouseClick += std::bind(&ObjectClasses::buttonSelectAllEntitiesOfClass_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            Controls.Add(buttonSelectAllEntitiesOfClass);

            UpdateClassList();
        }
        ~ObjectClasses() {
            delete labelObjectList;
            delete listViewClasses;
            delete toolStripClasses;
            delete toolStripButtonAddClass;
            delete toolStripButtonRemoveClass;
            delete toolStripButtonRenameClass;

            delete labelPropertySetter;
            delete listViewProperties;
            delete toolStripProperties;
            delete toolStripButtonAddProperty;
            delete toolStripButtonRemoveProperty;

            delete labelEntitySettings;
            delete buttonAddEntity;
            delete buttonSelectAllEntitiesOfClass;
        }

        void RelinkUsedClasses(int start, int end) {
            for (int classID = M_MAX(0, start); classID <= end && classID < Editor->LinkedStage->Classes.size(); classID++) {
                UsedClass* usedClass = Editor->LinkedStage->Classes[classID];
                usedClass->LinkedClassIndex = -1;

                for (int linkedClassIndex = 1; linkedClassIndex < GameLinker::ClassCount; linkedClassIndex++) {
                    if (usedClass->NameHash == GameLinker::ClassList[linkedClassIndex].Name) {
                        usedClass->LinkedClassIndex = linkedClassIndex;
                        Editor->LinkedStage->LinkClassData(linkedClassIndex, classID);
                        break;
                    }
                }
            }

            // TODO: This needs to also remap class indexes and linked class indexes
            // TODO: We need to make sure that we are freeing the staticobjects of unused classes
        }
        void RemapEntityClasses() {

        }

        void OnRelinkGameDLL() {
            // Free all resources involving linkedClasses (static objects), Sprites, Images, etc.
            // For every old linkedClass, get the new linkedClassIndex of the class, or -1 if it no longer has code
            // Build a linkedClass matrix that converts old linkedClassIndex to new
            // For every UsedClass, run the old linkedClassIndex if >= 0 through the matrix
            // Run the static constructor and editor loads
        }

        void listViewClasses_onSelectedIndexChanged(void* sender, EventArgs* e) {
            UpdatePropertyList();

            int classID = listViewClasses->SelectedIndex;
            if (classID >= 0) {
                char stringBuffer[256];

                buttonAddEntity->Enabled = true;
                buttonSelectAllEntitiesOfClass->Enabled = true;

                UsedClass* usedClass = Editor->LinkedStage->GetUsedClassByClassID(classID);

                sprintf(stringBuffer, "Add '%s' Entity", usedClass->Name);
                buttonAddEntity->SetText(stringBuffer);

                sprintf(stringBuffer, "Select All '%s' Entities", usedClass->Name);
                buttonSelectAllEntitiesOfClass->SetText(stringBuffer);
            }
            else {
                buttonAddEntity->Enabled = false;
                buttonSelectAllEntitiesOfClass->Enabled = false;

                buttonAddEntity->SetText("Add Entity");
                buttonSelectAllEntitiesOfClass->SetText("Select All Entities");
            }
        }
        void toolStripButtonAddClass_onMouseClick(void* sender, MouseEventArgs* e) {
            Form_EditClass* dialog = new Form_EditClass("Add New Class", NULL);
            dialog->BackColor = BackColor;

            UI::System::Application::ShowDialog(dialog, [this, dialog](DialogResult result) -> void {
                if (result == DialogResult::OK) {
                    char stringBuffer[256];
                    Strings::ToCString(stringBuffer, &dialog->textBoxName->Text);

                    if (Editor->LinkedStage->GetClass(stringBuffer) >= 0)
                        return;

                    int newIndex = Editor->LinkedStage->Classes.size();
                    Editor->LinkedStage->AddClassByName(stringBuffer);
                    RelinkUsedClasses(newIndex, newIndex);

                    UpdateClassList();
                }
            });
        }
        void toolStripButtonRemoveClass_onMouseClick(void* sender, MouseEventArgs* e) {
            int classID = listViewClasses->SelectedIndex;
            if (classID < 0)
                return;

            // TODO: Prompt user if this is what they truly want to do (only if there's any entities that use this class)
            /* "This will also remove every Entity of this Class! Do you want to continue?" */
            /* [*] Don't show again for this session */

            Editor->ClassRemove(classID);
        }
        void toolStripButtonRenameClass_onMouseClick(void* sender, MouseEventArgs* e) {
            // NOTE: Make sure to inform user when changing the name of a class that's Linked
            /* "This class is linked to its Live Template counterpart! Changing the name will
            unlink it, and the Live Template will no longer appear. Do you want to continue?" */
        }
        void buttonAddEntity_onMouseClick(void* sender, MouseEventArgs* e) {
            int classID = listViewClasses->SelectedIndex;
            if (classID < 0)
                return;

            Editor->EntityAdd(classID);
        }
        void buttonSelectAllEntitiesOfClass_onMouseClick(void* sender, MouseEventArgs* e) {
            int classID = listViewClasses->SelectedIndex;
            if (classID < 0)
                return;

            Editor->EntitySelectAllOfClass(classID);
        }
        void toolStripButtonAddProperty_onMouseClick(void* sender, MouseEventArgs* e) {
            int classID = listViewClasses->SelectedIndex;
            if (classID < 0)
                return;

            Form_EditProperty* dialog = new Form_EditProperty("Add New Property", NULL, VAR_INT32);
            dialog->BackColor = BackColor;

            UI::System::Application::ShowDialog(dialog, [this, dialog, classID](DialogResult result) -> void {
                if (result == DialogResult::OK) {
                    char stringBuffer[256];
                    Strings::ToCString(stringBuffer, &dialog->textBoxName->Text);

                    int typeIndex = dialog->comboBoxType->SelectedIndex;

                    if (Editor->ClassHasProperty(classID, stringBuffer) ||
                        typeIndex < 0)
                        return;

                    Editor->ClassAddProperty(classID, stringBuffer, dialog->AvailableTypes[typeIndex]);
                }
            });
        }
        void toolStripButtonRemoveProperty_onMouseClick(void* sender, MouseEventArgs* e) {
            /* "This will remove the property data for every Entity of this Class! Do you want to continue?" */
            /* [*] Don't show again for this session */

            int classID = listViewClasses->SelectedIndex;
            if (classID < 0)
                return;

            int propertyID = listViewProperties->SelectedIndex;
            if (propertyID < 0)
                return;

            UsedClass* usedClass = Editor->LinkedStage->GetUsedClassByClassID(classID);
            Editor->ClassRemoveProperty(classID, usedClass->Properties[propertyID].NameString);
        }

        void UpdateClassList() {
            for (int i = 0; i < listViewClasses->Items.Count(); i++)
                delete listViewClasses->Items[i];

            listViewClasses->Items.Clear();

            if (!Editor || !Editor->LinkedStage)
                return;

            for (int i = 0; i < Editor->LinkedStage->Classes.size(); i++)
                listViewClasses->Items.Add(new ListViewItem(Editor->LinkedStage->Classes[i]->Name));

            listViewClasses->ResizeChildren();
        }
        void UpdatePropertyList() {
            for (int i = 0; i < listViewProperties->Items.Count(); i++) {
                auto& item = listViewProperties->Items[i];
                for (int s = 0; s < item->SubItems.Count(); s++) {
                    delete item->SubItems[i];
                }
                item->SubItems.Clear();

                delete listViewProperties->Items[i];
            }

            listViewProperties->Items.Clear();

            int classID = listViewClasses->SelectedIndex;
            if (classID < 0)
                return;

            if (!Editor || !Editor->LinkedStage)
                return;

            UsedClass* usedClass = Editor->LinkedStage->Classes[classID];
            for (int i = 0; i < usedClass->Properties.Count(); i++) {
                auto item = new ListViewItem(usedClass->Properties[i].NameString);
                item->SubItems.Add(new ListViewSubItem(Hatch::GetPropertyTypeString(usedClass->Properties[i].AttributeType)));
                listViewProperties->Items.Add(item);
            }

            listViewProperties->ResizeChildren();
        }
    };
    struct EntityProperties : FlowLayoutPanel {
        SceneEditor* Editor = NULL;

        Label* labelEntityList;
        ListView* listViewEntityList;
        ToolStrip* toolStripEntityList;
        ToolStripButton* toolStripButtonAddEntity;
        ToolStripButton* toolStripButtonRemoveEntity;
        ToolStripButton* toolStripButtonDuplicateEntity;
        ToolStripButton* toolStripButtonMoveEntityUp;
        ToolStripButton* toolStripButtonMoveEntityDown;
        Label* labelProperties;
        PropertyGrid* propertyGridEntity;
        ToolStrip* toolStripProperties;
        ToolStripButton* toolStripButtonAddProperty;
        ToolStripButton* toolStripButtonRemoveProperty;
        ToolStripButton* toolStripButtonEditProperty;
        Label* labelOptions;
        Label* labelTotalEntities;
        Button* buttonJumpToEntityPosition;

        EntityProperties(SceneEditor* editor) : FlowLayoutPanel() {
            Editor = editor;

            Dock = DOCK_FILL;
            Size = { 32, 32 };

            BackColor = Color(0x282C34, 0xFF);

            Padding = 6;
            FlowDirection = FlowDirection::TOP_TO_BOTTOM;

            labelEntityList = new Label("Entity List:");
            labelEntityList->Anchor = ANCHOR_LEFT;
            Controls.Add(labelEntityList);

            listViewEntityList = new ListView();
            listViewEntityList->Margin.Top = 4;
            listViewEntityList->Dock = DOCK_TOP;
            listViewEntityList->Size = { 30, 200 };
            listViewEntityList->LayoutType = ListViewLayout::List;
            listViewEntityList->Columns.Add(new ColumnHeader("L", 20, 1));
            listViewEntityList->Columns.Add(new ColumnHeader("V", 20, 2));
            listViewEntityList->Columns.Add(new ColumnHeader("Name", -1, 0));
            listViewEntityList->onSelectedIndexChanged += std::bind(&EntityProperties::listViewEntityList_onSelectedIndexChanged, this, std::placeholders::_1, std::placeholders::_2);
            Controls.Add(listViewEntityList);

            labelProperties = new Label("Properties:");
            labelProperties->Anchor = ANCHOR_LEFT;
            labelProperties->Margin.Top = 8;
            Controls.Add(labelProperties);

            propertyGridEntity = new PropertyGrid(editor);
            propertyGridEntity->Margin.Top = 4;
            propertyGridEntity->Dock = DOCK_TOP;
            propertyGridEntity->Size = { 0, 200 };
            propertyGridEntity->DoVScroll = true;
            propertyGridEntity->HideEmptyVScroll = true;
            Controls.Add(propertyGridEntity);

            labelOptions = new Label("Options:");
            labelOptions->Anchor = ANCHOR_LEFT;
            labelOptions->Margin.Top = 8;
            Controls.Add(labelOptions);

            buttonJumpToEntityPosition = new Button("Jump To Entity Position");
            buttonJumpToEntityPosition->Margin.Top = 4;
            buttonJumpToEntityPosition->Size = { 200, 25 };
            buttonJumpToEntityPosition->onMouseClick += std::bind(&EntityProperties::buttonJumpToEntityPosition_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            Controls.Add(buttonJumpToEntityPosition);

            labelTotalEntities = new Label();
            labelTotalEntities->Margin.Top = 4;
            Controls.Add(labelTotalEntities);

            UpdateEntityList();
        }
        ~EntityProperties() {
            delete labelEntityList;
            delete listViewEntityList;
            // delete toolStripEntityList;
            // delete toolStripButtonAddEntity;
            // delete toolStripButtonRemoveEntity;
            // delete toolStripButtonDuplicateEntity;
            // delete toolStripButtonMoveEntityUp;
            // delete toolStripButtonMoveEntityDown;
            delete labelProperties;
            delete propertyGridEntity;
            // delete toolStripProperties;
            // delete toolStripButtonAddProperty;
            // delete toolStripButtonRemoveProperty;
            // delete toolStripButtonEditProperty;
            delete labelOptions;
            delete labelTotalEntities;
            delete buttonJumpToEntityPosition;
        }

        void listViewEntityList_onSelectedIndexChanged(void* sender, EventArgs* e) {
            if (listViewEntityList->SelectedIndex >= 0) {
                Editor->tilePlacementField->Action_DeselectAllEntities();
                Editor->tilePlacementField->Action_SelectSingularEntity(listViewEntityList->SelectedIndex);
            }
        }
        void buttonJumpToEntityPosition_onMouseClick(void* sender, MouseEventArgs* e) {
            auto enti = Editor->entityProperties->propertyGridEntity->SelectedEntity.Get();
            if (enti != NULL) {
                Editor->tilePlacementField->ViewX = enti->Position.X.Whole - Graphics::Views->WidthHalf;
                Editor->tilePlacementField->ViewY = enti->Position.Y.Whole - Graphics::Views->HeightHalf;
                Editor->tilePlacementField->UpdateRenderTarget = true;
            }
        }

        void UpdateEntityList() {
            char stringBuffer[256];
            for (int i = 0; i < listViewEntityList->Items.Count(); i++)
                delete listViewEntityList->Items[i];

            listViewEntityList->Items.Clear();

            for (int i = 0; i < Editor->EntityCount; i++) {
                snprintf(stringBuffer, 255, "%d: %s", i, Editor->LinkedStage->Classes[Editor->EntitySlots[i].ClassID]->Name);
                listViewEntityList->Items.Add(new ListViewItem(stringBuffer));
            }

            listViewEntityList->ResizeChildren();

            snprintf(stringBuffer, 255, "Total Entities: %d", Editor->EntityCount);
            labelTotalEntities->SetText(stringBuffer);
        }
    };
    struct TilePlacementField : Control {
        SceneEditor* Editor = NULL;

        // Enums & Constants
        enum ClickDragTypes {
            CLICKDRAG_NONE,
            CLICKDRAG_VIEW_PAN,
            CLICKDRAG_HIGHLIGHT,
            CLICKDRAG_MOVE,
        };
        enum SceneFilter {
            FILTER_COMMON = 1,
            FILTER_MODE_1 = 2,
            FILTER_MODE_2 = 4,
            FILTER_ALL = 0xFF,
        };
        enum SelectTypes {
            SELECTTYPE_TILES,
            SELECTTYPE_PARALLAX,
            SELECTTYPE_ENTITIES,
        };
        enum ToolTypes {
            // Common
            TOOL_SELECT,
            TOOL_ERASE,

            // Tile Layers
            TOOL_TILE_STAMP,
            TOOL_TILE_EYEDROPPER,
            TOOL_TILE_BUCKET_FILL,
            TOOL_TILE_COLLISION_BRUSH,

			// Parallax lines
            TOOL_PARALLAX_RESIZER,

            // Entity Layers
            TOOL_ENTITY_TOOL,
        };
        enum EntityEditorState {
            EMS_NONE,
            EMS_HOVERING,
            EMS_SELECTED,
            EMS_CLICKSTARTED,
        };

        const Color TileHighlightHover = Color(0xBFBFBF, 0x40);
        const Color TileHighlightSelected = Color(0xFFFFFF, 0x80);

        // Class variables
        float     ZoomScales[11] = {
            0.33f,
            0.50f,
            1.00f,
            2.00f,
            3.00f,
            4.00f,
            5.00f,
            6.00f,
            8.00f,
            12.00f,
            16.00f,
        };
        int       ZoomIndex = 2;
        int       ZoomViewStoredWidth = 0;
        int       ZoomViewStoredHeight = 0;
        float     ZoomViewMouseX = 0.0f;
        float     ZoomViewMouseY = 0.0f;
        bool      UpdateZoomViewCoords = false;

        float     ViewX;
        float     ViewY;
        float     ViewScale = ZoomScales[ZoomIndex];
        float     ViewScaleNext = 1.0f;

        int       ClickDragType = 0;
        int       ClickDragStartX = 0;
        int       ClickDragStartY = 0;
        int       ClickDragStartViewX = 0;
        int       ClickDragStartViewY = 0;
        int       MouseWorldX;
        int       MouseWorldY;
        SDL_Rect  TileSelectBounds { 0, 0, 0, 0 };

        int       CurrentFilter = FILTER_MODE_1 | FILTER_COMMON;

        int       CurrentLayer = 0;
        bool      ShowLikeLayers = true;

        bool      UpdateRenderTarget = true;

        int       SelectionType = SELECTTYPE_TILES;
        int       ToolType = TOOL_SELECT;
        int       SnapX = 8;
        int       SnapY = 8;
        bool      SnappingEnabled = true;

        Stamp*    StampDataToBePlaced = NULL;
        ArrayList<Entity*> SelectedEntities;
        // ArrayList<Entity*> DraggingEntities;

        // This should increment at the end of a group of actions (ex: end of mouse click, release of key, etc.)
        int       ActionSiblingID = 0;
        int       ActionSiblingKeyID = 0;

        SDL_Texture* FrameBufferTexture = NULL;

        // Constructor
        TilePlacementField(SceneEditor* editor) : Control() {
            Editor = editor;

            CanFocus = true;

            ViewX = 0;
            ViewY = 0;
            MouseWorldX =
            MouseWorldY = -1;
            Dock = DOCK_FILL;

            StampDataToBePlaced = Stamp::FromRepeatTile(0, 1, 1);

            SelectedEntities.Clear();

            BackColor = Color(0x404040, 0xFF);

            const auto& c_this = this;

            // Copy (Ctrl+C)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_CTRL, SDLK_c, this, true, [c_this]() -> void {
                if (c_this->ToolType == TOOL_SELECT) {
                    if (c_this->SelectionType == SELECTTYPE_TILES) {
                        if (c_this->TileSelectBounds.w > 0) {
                            c_this->ActionSiblingKeyID++;

                            c_this->Action_SetStampDataFromHighlight();

                            c_this->Editor->ActionStack_Do(
                                new LayerTileSelectionEditCommand(c_this->Editor, { 0, 0, 0, 0 }),
                                c_this->ActionSiblingKeyID << 8);

                            c_this->SelectTool(TOOL_TILE_STAMP);
                        }
                    }
                }
            });
            // Cut (Ctrl+X)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_CTRL, SDLK_x, this, true, [c_this]() -> void {
                if (c_this->ToolType == TOOL_SELECT) {
                    if (c_this->SelectionType == SELECTTYPE_TILES) {
                        if (c_this->TileSelectBounds.w > 0) {
                            c_this->ActionSiblingKeyID++;

                            c_this->Action_SetStampDataFromHighlight();

                            int x = c_this->TileSelectBounds.x;
                            int y = c_this->TileSelectBounds.y;
                            int w = c_this->TileSelectBounds.w;
                            int h = c_this->TileSelectBounds.h;

                            c_this->Editor->ActionStack_Do(
                                new LayerTileEditCommand(c_this->Editor, c_this->CurrentLayer, x, y, Stamp::FromRepeatTile(TILE_EMPTY, w, h), true),
                                c_this->ActionSiblingKeyID << 8);
                            c_this->Editor->ActionStack_Do(
                                new LayerTileSelectionEditCommand(c_this->Editor, { 0, 0, 0, 0 }),
                                c_this->ActionSiblingKeyID << 8);

                            c_this->SelectTool(TOOL_TILE_STAMP);
                        }
                    }
                }
            });

            // Undo (Ctrl+Z)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_CTRL, SDLK_z, this, true, [c_this]() -> void {
                c_this->Editor->ActionStack_Undo();
            });
            // Redo (Ctrl+Shift+Z, Ctrl+Y)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_CTRL | KMOD_SHIFT, SDLK_z, this, true, [c_this]() -> void {
                // c_this->Editor->ActionStack_Redo();
            });
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_CTRL, SDLK_y, this, true, [c_this]() -> void {
                c_this->Editor->ActionStack_Redo();
            });

            // Tool Change: Select (r)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_NONE, SDLK_r, this, true, [c_this]() -> void {
                c_this->SelectTool(TOOL_SELECT);
            });
            // Tool Change: Erase (e)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_NONE, SDLK_e, this, true, [c_this]() -> void {
                c_this->SelectTool(TOOL_ERASE);
            });
            // Tool Change: Tile Stamp (s)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_NONE, SDLK_s, this, true, [c_this]() -> void {
                c_this->SelectTool(TOOL_TILE_STAMP);
            });

            // Delete Tiles (Delete)
            int deleteKey = SDLK_DELETE;
            #ifdef _MACOS
                deleteKey = SDLK_BACKSPACE;
            #endif
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_NONE, deleteKey, this, true, [c_this, this]() -> void {
                if (SelectionType == SELECTTYPE_ENTITIES) {
                    c_this->ActionSiblingKeyID++;

                    auto compareFunc = [this](Entity* a, Entity* b) -> bool {
                        int slotIDa = Editor->EntityGetSlot(a);
                        int slotIDb = Editor->EntityGetSlot(b);
                        return slotIDa > slotIDb;
                    };

                    // Sort selected entities by slotID descending order
                    Entity* key;
                    for (int i = 1, j; i < SelectedEntities.Count(); i++) {
                        key = SelectedEntities[i];
                        j = i - 1;

                        /* Move elements of arr[0..i-1], that are
                        greater than key, to one position ahead
                        of their current position */
                        while (j >= 0 && !compareFunc(SelectedEntities[j], key)) {
                            SelectedEntities[j + 1] = SelectedEntities[j];
                            j = j - 1;
                        }
                        SelectedEntities[j + 1] = key;
                    }

                    // Then delete them
                    for (int i = 0; i < SelectedEntities.Count(); i++) {
                        if (Editor->entityProperties->propertyGridEntity->SelectedEntity == SelectedEntities[i])
                            Editor->entityProperties->propertyGridEntity->SelectedEntity = NULL;

                        int slotID = Editor->EntityGetSlot(SelectedEntities[i]);
                        auto metadata = &Editor->EntityEditorSlots[slotID];
                        c_this->Editor->EntityRemove(slotID);
                    }
                    SelectedEntities.Clear();
                }
                else {
                    if (c_this->TileSelectBounds.w > 0) {
                        int _layer = c_this->CurrentLayer;

                        int x = c_this->TileSelectBounds.x;
                        int y = c_this->TileSelectBounds.y;
                        int w = c_this->TileSelectBounds.w;
                        int h = c_this->TileSelectBounds.h;

                        c_this->ActionSiblingKeyID++;

                        c_this->Editor->ActionStack_Do(
                            new LayerTileEditCommand(c_this->Editor, c_this->CurrentLayer, x, y, Stamp::FromRepeatTile(TILE_EMPTY, w, h), true),
                            c_this->ActionSiblingKeyID << 8);
                        c_this->Editor->ActionStack_Do(
                            new LayerTileSelectionEditCommand(c_this->Editor, { 0, 0, 0, 0 }),
                            c_this->ActionSiblingKeyID << 8);
                    }
                }
            });

            // Zoom In (Ctrl+Plus)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_CTRL, SDLK_EQUALS, this, true, [c_this]() -> void {
                c_this->Action_ZoomIn();
            });
            // Zoom Out (Ctrl+Minus)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_CTRL, SDLK_MINUS, this, true, [c_this]() -> void {
                c_this->Action_ZoomOut();
            });
            // Zoom 100% (Ctrl+0)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_CTRL, SDLK_0, this, true, [c_this]() -> void {
                c_this->Action_Zoom100();
            });

            // Stamp: Flip Horizontal (H)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_NONE, SDLK_h, this, true, [c_this]() -> void {
                auto oldStampData = c_this->StampDataToBePlaced;

                c_this->StampDataToBePlaced = Stamp::FromStampFlipped(oldStampData, true, false);

                delete oldStampData;
            });
            // Stamp: Flip Vertical (V)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_NONE, SDLK_v, this, true, [c_this]() -> void {
                auto oldStampData = c_this->StampDataToBePlaced;

                c_this->StampDataToBePlaced = Stamp::FromStampFlipped(oldStampData, false, true);

                delete oldStampData;
            });

            // Change Visible Collision: None
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_SHIFT, SDLK_q, this, false, [c_this]() -> void {
                Graphics::DrawCollision = 0;
                c_this->UpdateRenderTarget = true;
            });
            // Change Visible Collision: Plane A
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_SHIFT, SDLK_w, this, false, [c_this]() -> void {
                Graphics::DrawCollision = 1;
                c_this->UpdateRenderTarget = true;
            });
            // Change Visible Collision: Plane B
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_SHIFT, SDLK_e, this, false, [c_this]() -> void {
                Graphics::DrawCollision = 2;
                c_this->UpdateRenderTarget = true;
            });

            // Layer: New Layer (Shift+N)
            // Layer: Toggle Visibility (Shift+X)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_SHIFT, SDLK_x, this, false, [c_this]() -> void {
                c_this->UpdateRenderTarget = true;
            });
            // Layer: Open Properties (Shift+P)
            // Layer: Select Layer Index-- ()
            // Layer: Select Layer Index++ ()
            // Layer: Toggle Show Like Layers (Shift+L)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_SHIFT, SDLK_l, this, false, [c_this]() -> void {
                c_this->ShowLikeLayers = !c_this->ShowLikeLayers;
                c_this->UpdateRenderTarget = true;
            });

            // View: Toggle Grid (Shift+G)
            // View: Snap To Grid (Shift+S)

            // File: Import Tilesets... (Ctrl+I)
            UI::System::Application::BaseForm->RegisterShortcut(KMOD_CTRL, SDLK_i, this, false, [c_this]() -> void {
                if (c_this->Editor->PromptImportTileset()) {
                    // Prompt to "Remap All Tiles?" "Remap all tiles in every layer?\n\nThis action cannot be undone. (Don't show me this again.)"
                    c_this->Editor->tilePlacementField->RemapStampDataToBePlaced();
                    c_this->Editor->LinkedStage->RemapTileConfig();
                    c_this->Editor->LayerRemapAllTiles();
                }
            });

            UI::System::Application::BaseForm->RegisterShortcut(KMOD_CTRL, SDLK_AT, this, false, [c_this]() -> void {

            });



            SDL_RendererInfo info;
            SDL_GetRendererInfo(UI::Graphics::Renderer::Renderer, &info);

            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

			info.max_texture_width = M_MIN(info.max_texture_width, 2048);
			info.max_texture_height = M_MIN(info.max_texture_height, 2048);

            FrameBufferTexture = SDL_CreateTexture(
                UI::Graphics::Renderer::Renderer, SDL_GetWindowPixelFormat(UI::Graphics::Renderer::Window),
                SDL_TEXTUREACCESS_TARGET, info.max_texture_width, info.max_texture_height);
            if (!FrameBufferTexture) {
                Diagnostics::SetError("SDL_CreateTexture failed with error: %s", SDL_GetError());
            }
        }
        ~TilePlacementField() {
            // TODO: maybe this guy too: StampDataToBePlaced
            SDL_DestroyTexture(FrameBufferTexture);
        }

        void WindowToWorldCoords(int* x, int* y) {
            auto screenPos = GetPositionInWindowCoords();
            *x = (int)((*x - screenPos.X) / ViewScale + ViewX);
            *y = (int)((*y - screenPos.Y) / ViewScale + ViewY);
        }

        // Actions
        void Action_ZoomIn() {
            if (ZoomIndex < sizeof(ZoomScales) / sizeof(float) - 1) {
                ZoomIndex++;

                ZoomViewStoredWidth = Graphics::CurrentView->Width;
                ZoomViewStoredHeight = Graphics::CurrentView->Height;
                ViewScaleNext = ZoomScales[ZoomIndex];

                Position screenPos = GetPositionInWindowCoords();
                ::Size screenSize = Size;

                int mx, my;
                SDL_GetMouseState(&mx, &my);
                ZoomViewMouseX = (mx - screenPos.X) / (float)screenSize.W;
                ZoomViewMouseY = (my - screenPos.Y) / (float)screenSize.H;

                UpdateZoomViewCoords = true;

                UpdateRenderTarget = true;
            }
        }
        void Action_ZoomOut() {
            if (ZoomIndex > 0) {
                ZoomIndex--;

                ZoomViewStoredWidth = Graphics::CurrentView->Width;
                ZoomViewStoredHeight = Graphics::CurrentView->Height;
                ViewScaleNext = ZoomScales[ZoomIndex];

                Position screenPos = GetPositionInWindowCoords();
                ::Size screenSize = Size;

                int mx, my;
                SDL_GetMouseState(&mx, &my);
                ZoomViewMouseX = (mx - screenPos.X) / (float)screenSize.W;
                ZoomViewMouseY = (my - screenPos.Y) / (float)screenSize.H;

                UpdateZoomViewCoords = true;

                UpdateRenderTarget = true;
            }
        }
        void Action_Zoom100() {
            for (int i = 0; i < sizeof(ZoomScales) / sizeof(ZoomScales[0]); i++) {
                if (ZoomScales[i] == 1.0f) {
                    ZoomIndex = i;
                    break;
                }
            }

            ZoomViewStoredWidth = Graphics::CurrentView->Width;
            ZoomViewStoredHeight = Graphics::CurrentView->Height;
            ViewScaleNext = ZoomScales[ZoomIndex];

            Position screenPos = GetPositionInWindowCoords();
            ::Size screenSize = Size;

            int mx, my;
            SDL_GetMouseState(&mx, &my);
            ZoomViewMouseX = (mx - screenPos.X) / (float)screenSize.W;
            ZoomViewMouseY = (my - screenPos.Y) / (float)screenSize.H;

            UpdateZoomViewCoords = true;

            UpdateRenderTarget = true;
        }

        void Action_SetStampData(int w, int h, Tile* source) {
            if (w > 0 && h > 0) {
                delete StampDataToBePlaced;
                StampDataToBePlaced = Stamp::FromTileArray(source, w, h);
            }
        }
        void Action_SetStampDataFromHighlight() {
            int w = TileSelectBounds.w;
            int h = TileSelectBounds.h;

            if (w > 0 && h > 0) {
                delete StampDataToBePlaced;
                StampDataToBePlaced = Stamp::FromLayer(Editor, CurrentLayer, TileSelectBounds.x, TileSelectBounds.y, w, h);
            }
        }
        void Action_SelectSingularEntity(int slot) {
            auto enti = &Editor->EntitySlots[slot];
            auto meta = &Editor->EntityEditorSlots[slot];

            SelectedEntity_Clear();
            SelectedEntity_Add(enti);
            Editor->entityProperties->propertyGridEntity->SelectedEntity = enti;

            Editor->entityProperties->listViewEntityList->CanRaiseEvents = false;
            Editor->entityProperties->listViewEntityList->Select(slot);
            Editor->entityProperties->listViewEntityList->CanRaiseEvents = true;

            UpdateRenderTarget = true;
        }
        void Action_SelectEntityForMultiselect(int slot) {
            auto enti = &Editor->EntitySlots[slot];
            auto meta = &Editor->EntityEditorSlots[slot];

            SelectedEntity_Add(enti);

            UpdateRenderTarget = true;
        }
        void Action_DeselectAllEntities() {
            for (int i = 0; i < Editor->EntityCount; i++) {
                auto enti = &Editor->EntitySlots[i];
                auto meta = &Editor->EntityEditorSlots[i];
                if (!(enti->Filter & CurrentFilter))
                    continue;

                meta->SelectionType = EMS_NONE;
            }

            UpdateRenderTarget = true;
        }

        void SelectedEntity_Add(Entity* entity) {
            SelectedEntities.Add(entity);

            int slotID = Editor->EntityGetSlot(entity);
            auto metadata = &Editor->EntityEditorSlots[slotID];

            metadata->SelectionType = EMS_SELECTED;
        }
        void SelectedEntity_Clear() {
            for (int i = 0; i < SelectedEntities.Count(); i++) {
                int slotID = Editor->EntityGetSlot(SelectedEntities[i]);
                auto metadata = &Editor->EntityEditorSlots[slotID];
                if (metadata->SelectionType == EMS_SELECTED)
                    metadata->SelectionType = EMS_NONE;
            }
            SelectedEntities.Clear();
        }

        void MouseTileErase(MouseEventArgs* e) {
            Tile destTile = TILE_EMPTY;

            int mx = e->X, my = e->Y;
            WindowToWorldCoords(&mx, &my);

            // This should erase in a line from mX, mY to MouseWorldX,Y
            int layerIndex = CurrentLayer;
            int x = mx >> 4;
            int y = my >> 4;
            Layer* layer = &Editor->Layers[layerIndex];
            Tile* tileSrc = &layer->Tiles[x + (y << layer->WidthInBits)];
            if (*tileSrc != destTile) {
                Editor->ActionStack_Do(new LayerTileEditCommand(Editor, layerIndex, x, y, Stamp::FromRepeatTile(destTile, 1, 1), true), ActionSiblingID);
            }
        }
        void MouseTileStamp(MouseEventArgs* e) {
            int mx = e->X, my = e->Y;
            WindowToWorldCoords(&mx, &my);

            int layerIndex = CurrentLayer;
            int x = mx >> 4;
            int y = my >> 4;

            Editor->ActionStack_Do(new LayerTileEditCommand(Editor, layerIndex, x, y, Stamp::Clone(StampDataToBePlaced)), ActionSiblingID);
        }
        void MouseTileSelectDown(MouseEventArgs* e) {
            SDL_Point pos1 = { e->X, e->Y };
            WindowToWorldCoords(&pos1.x, &pos1.y);

            pos1.x >>= 4;
            pos1.y >>= 4;
            // if (!SDL_PointInRect(&pos1, &TileSelectBounds))

            Editor->ActionStack_Do(
                new LayerTileSelectionEditCommand(Editor, { 0, 0, 0, 0 }),
                ActionSiblingID);
        }
        void MouseTileSelectBegin(MouseEventArgs* e) {
            if (SDL_CaptureMouse(SDL_TRUE) < 0) {
                fprintf(stderr, "SDL_CaptureMouse failed: %s\n", SDL_GetError());
                return;
            }

            MouseCaptured = this;

            ClickDragType = CLICKDRAG_HIGHLIGHT;
            ClickDragStartX = e->X;
            ClickDragStartY = e->Y;
        }
        void MouseTileSelectEnd(MouseEventArgs* e) {
            int x1 = ClickDragStartX;
            int y1 = ClickDragStartY;
            int x2 = MouseWorldX;
            int y2 = MouseWorldY;
            WindowToWorldCoords(&x1, &y1);

            int x = M_MIN(x1, x2) >> 4;
            int y = M_MIN(y1, y2) >> 4;
            int w = (M_MAX(x1, x2) >> 4) - x + 1;
            int h = (M_MAX(y1, y2) >> 4) - y + 1;

            Editor->ActionStack_Do(
                new LayerTileSelectionEditCommand(Editor, { x, y, w, h }),
                ActionSiblingID);

            MouseCaptured = NULL;
            SDL_CaptureMouse(SDL_FALSE);
        }
        void MouseTileEyedropper(MouseEventArgs* e) {
            int mx = e->X, my = e->Y;
            WindowToWorldCoords(&mx, &my);

            int layerIndex = CurrentLayer;
            int x = mx >> 4;
            int y = my >> 4;
            Layer* layer = &Editor->Layers[layerIndex];
            Tile* tileSrc = &layer->Tiles[x + (y << layer->WidthInBits)];
            if (*tileSrc != TILE_EMPTY) {
                int id = tileSrc->ID;
                Editor->tileSelector->Select(id);
                Editor->tileSelector->SelectRange(id, id);
                Editor->tileCollisionEditor->tileSelector->Select(id);
                Editor->tileCollisionEditor->tileSelector->SelectRange(id, id);
            }
        }
        void MouseTileCollisionBrush(MouseEventArgs* e, bool clear) {
            if (Graphics::DrawCollision == 0)
                return;

            int mx = e->X, my = e->Y;
            WindowToWorldCoords(&mx, &my);

            int layerIndex = CurrentLayer;
            int x = mx >> 4;
            int y = my >> 4;

            int collisionValue = Graphics::SOLID_FULL;
            if ((e->Modifier & KMOD_ALT))
                collisionValue = Graphics::SOLID_PLATFORM;
            else if ((e->Modifier & KMOD_CTRL))
                collisionValue = Graphics::SOLID_FALLTHROUGH;

            Stamp* stamp = Stamp::FromLayer(Editor, layerIndex, x, y, 1, 1);
            if (clear) {
                for (int i = 0; i < stamp->Width * stamp->Height; i++) {
                    if (stamp->Data[i] == TILE_EMPTY) continue;

                    stamp->Data[i].PlaneA = Graphics::SOLID_NONE;
                    stamp->Data[i].PlaneB = Graphics::SOLID_NONE;
                }
            }
            else {
                if (Graphics::DrawCollision == 1) {
                    for (int i = 0; i < stamp->Width * stamp->Height; i++) {
                        if (stamp->Data[i] == TILE_EMPTY) continue;

                        stamp->Data[i].PlaneA = collisionValue;
                    }
                }
                else if (Graphics::DrawCollision == 2) {
                    for (int i = 0; i < stamp->Width * stamp->Height; i++) {
                        if (stamp->Data[i] == TILE_EMPTY) continue;

                        stamp->Data[i].PlaneB = collisionValue;
                    }
                }
            }
            Editor->ActionStack_Do(new LayerTileEditCommand(Editor, layerIndex, x, y, stamp), ActionSiblingID);
        }
        void MouseTileCollisionBrushSelectEnd(MouseEventArgs* e, bool clear) {
            if (Graphics::DrawCollision == 0)
                return;

            int x1 = ClickDragStartX;
            int y1 = ClickDragStartY;
            int x2 = MouseWorldX;
            int y2 = MouseWorldY;
            WindowToWorldCoords(&x1, &y1);

            int layerIndex = CurrentLayer;
            int x = M_MIN(x1, x2) >> 4;
            int y = M_MIN(y1, y2) >> 4;
            int w = (M_MAX(x1, x2) >> 4) - x + 1;
            int h = (M_MAX(y1, y2) >> 4) - y + 1;

            int collisionValue = Graphics::SOLID_FULL;
            if ((e->Modifier & KMOD_ALT))
                collisionValue = Graphics::SOLID_PLATFORM;
            else if ((e->Modifier & KMOD_CTRL))
                collisionValue = Graphics::SOLID_FALLTHROUGH;

            Stamp* stamp = Stamp::FromLayer(Editor, layerIndex, x, y, w, h);
            if (clear) {
                for (int i = 0; i < stamp->Width * stamp->Height; i++) {
                    if (stamp->Data[i] == TILE_EMPTY) continue;

                    stamp->Data[i].PlaneA = Graphics::SOLID_NONE;
                    stamp->Data[i].PlaneB = Graphics::SOLID_NONE;
                }
            }
            else {
                if (Graphics::DrawCollision == 1) {
                    for (int i = 0; i < stamp->Width * stamp->Height; i++) {
                        if (stamp->Data[i] == TILE_EMPTY) continue;

                        stamp->Data[i].PlaneA = collisionValue;
                    }
                }
                else if (Graphics::DrawCollision == 2) {
                    for (int i = 0; i < stamp->Width * stamp->Height; i++) {
                        if (stamp->Data[i] == TILE_EMPTY) continue;

                        stamp->Data[i].PlaneB = collisionValue;
                    }
                }
            }
            Editor->ActionStack_Do(new LayerTileEditCommand(Editor, layerIndex, x, y, stamp), ActionSiblingID);

            MouseCaptured = NULL;
            SDL_CaptureMouse(SDL_FALSE);
        }
        void MouseEntityToolHover(MouseEventArgs* e) {
            int mx = e->X, my = e->Y;
            WindowToWorldCoords(&mx, &my);

            bool foundHovering = false;
            for (int i = 0; i < Editor->EntityCount; i++) {
                auto ent = &Editor->EntitySlots[i];
                auto entEd = &Editor->EntityEditorSlots[i];
                if (!(ent->Filter & CurrentFilter))
                    continue;

                if (entEd->SelectionType >= EMS_SELECTED)
                    continue;

                if (mx >= entEd->MinPos.X && my >= entEd->MinPos.Y && mx < entEd->MaxPos.X && my < entEd->MaxPos.Y && !foundHovering)
                    foundHovering = entEd->SelectionType = EMS_HOVERING;
                else
                    entEd->SelectionType = EMS_NONE;
            }

            UpdateRenderTarget = true;
        }
        void MouseEntityToolSelectBegin(MouseEventArgs* e) {
            if (SDL_CaptureMouse(SDL_TRUE) < 0) {
                fprintf(stderr, "SDL_CaptureMouse failed: %s\n", SDL_GetError());
                return;
            }

            MouseCaptured = this;

            ClickDragType = CLICKDRAG_HIGHLIGHT;
            ClickDragStartX = e->X;
            ClickDragStartY = e->Y;
        }
        void MouseEntityToolSelectEnd(MouseEventArgs* e) {
            int x1 = ClickDragStartX;
            int y1 = ClickDragStartY;
            int x2 = MouseWorldX;
            int y2 = MouseWorldY;
            WindowToWorldCoords(&x1, &y1);

            int _x1 = M_MIN(x1, x2);
            int _y1 = M_MIN(y1, y2);
            int _x2 = M_MAX(x1, x2);
            int _y2 = M_MAX(y1, y2);

            // If not holding modifer for "Add", clear previous selections
            if (!(e->Modifier & KMOD_SHIFT)) {
                SelectedEntity_Clear();
            }

            // Select entities touching this area
            for (int i = 0; i < Editor->EntityCount; i++) {
                auto ent = &Editor->EntitySlots[i];
                auto entEd = &Editor->EntityEditorSlots[i];
                if (!(ent->Filter & CurrentFilter))
                    continue;

                if (_x2 >= entEd->MinPos.X &&
                    _y2 >= entEd->MinPos.Y &&
                    _x1 < entEd->MaxPos.X &&
                    _y1 < entEd->MaxPos.Y)
                    SelectedEntity_Add(ent);
            }

            MouseCaptured = NULL;
            SDL_CaptureMouse(SDL_FALSE);
        }
        void MouseEntityToolDragBegin(MouseEventArgs* e) {
            if (SDL_CaptureMouse(SDL_TRUE) < 0) {
                fprintf(stderr, "SDL_CaptureMouse failed: %s\n", SDL_GetError());
                return;
            }

            MouseCaptured = this;

            ClickDragType = CLICKDRAG_MOVE;
            ClickDragStartX = e->X;
            ClickDragStartY = e->Y;
        }
        void MouseEntityToolDragMove(MouseEventArgs* e) {
            int x1 = ClickDragStartX;
            int y1 = ClickDragStartY;
            int x2 = e->X;
            int y2 = e->Y;
            WindowToWorldCoords(&x1, &y1);
            WindowToWorldCoords(&x2, &y2);

            for (int i = 0; i < SelectedEntities.Count(); i++) {
                auto ent = SelectedEntities[i];
                auto entEd = &Editor->EntityEditorSlots[(EntitySlot*)ent - Editor->EntitySlots];

                ent->Position.X.Whole = entEd->StartPos.X.Whole + x2 - x1;
                ent->Position.Y.Whole = entEd->StartPos.Y.Whole + y2 - y1;

                // Snapping
                if (SnappingEnabled) {
                    ent->Position.X.Whole /= SnapX;
                    ent->Position.X.Whole *= SnapX;

                    ent->Position.Y.Whole /= SnapY;
                    ent->Position.Y.Whole *= SnapY;
                }
            }
        }
        void MouseEntityToolDragEnd(MouseEventArgs* e) {
            // TODO: Add an action here that sets the new position (even if it's already there) and stores the old position
            //       so that the entity drag can be Undone

            MouseCaptured = NULL;
            SDL_CaptureMouse(SDL_FALSE);
        }
        void MouseEntityToolDown(MouseEventArgs* e) {
            int mx = e->X, my = e->Y;
            WindowToWorldCoords(&mx, &my);

            bool foundSelectable = !!(e->Modifier & KMOD_SHIFT);
            for (int i = 0; i < Editor->EntityCount; i++) {
                auto ent = &Editor->EntitySlots[i];
                auto entEd = &Editor->EntityEditorSlots[i];
                if (!(ent->Filter & CurrentFilter))
                    continue;

                if (mx >= entEd->MinPos.X && my >= entEd->MinPos.Y && mx < entEd->MaxPos.X && my < entEd->MaxPos.Y) {
                    if (entEd->SelectionType == EMS_SELECTED)
                        return;

                    if (!(e->Modifier & KMOD_SHIFT)) {
                        Action_SelectSingularEntity(i);
                    }
                    else
                        Action_SelectEntityForMultiselect(i);

                    foundSelectable = true;
                    break;
                }
            }

            if (!foundSelectable)
                SelectedEntity_Clear();

            UpdateRenderTarget = true;
        }
        void MouseEntityToolMove(MouseEventArgs* e) {
            int mx = e->X, my = e->Y;
            WindowToWorldCoords(&mx, &my);

            bool tryingToDragEntity = false;
            for (int i = 0; i < Editor->EntityCount; i++) {
                auto ent = &Editor->EntitySlots[i];
                auto entEd = &Editor->EntityEditorSlots[i];
                if (!(ent->Filter & CurrentFilter))
                    continue;

                if (entEd->SelectionType == EMS_SELECTED) {
                    if (mx >= entEd->MinPos.X && my >= entEd->MinPos.Y && mx < entEd->MaxPos.X && my < entEd->MaxPos.Y) {
                        tryingToDragEntity = true;
                    }

                    // Store the start location
                    entEd->StartPos = ent->Position;
                }
            }

            if (tryingToDragEntity)
                MouseEntityToolDragBegin(e);
            else
                MouseEntityToolSelectBegin(e);

            UpdateRenderTarget = true;
        }
        void MouseEntityToolUp(MouseEventArgs* e) {
            int mx = e->X, my = e->Y;
            WindowToWorldCoords(&mx, &my);

            for (int i = 0; i < Editor->EntityCount; i++) {
                auto ent = &Editor->EntitySlots[i];
                auto entEd = &Editor->EntityEditorSlots[i];
                if (!(ent->Filter & CurrentFilter))
                    continue;

                if (entEd->SelectionType == EMS_CLICKSTARTED) {
                    if (mx >= entEd->MinPos.X && my >= entEd->MinPos.Y && mx < entEd->MaxPos.X && my < entEd->MaxPos.Y)
                        entEd->SelectionType = EMS_SELECTED;
                    else
                        entEd->SelectionType = EMS_NONE;
                }
            }

            UpdateRenderTarget = true;
        }

        void SelectTool(int tool) {
            // Update UI
            Editor->tilePlacementToolbar->toolStripButtonSelect->Checked = false;
            Editor->tilePlacementToolbar->toolStripButtonErase->Checked = false;
            Editor->tilePlacementToolbar->toolStripButtonTileStamp->Checked = false;
            Editor->tilePlacementToolbar->toolStripButtonTileEyedropper->Checked = false;
            Editor->tilePlacementToolbar->toolStripButtonTileBucketFill->Checked = false;
            Editor->tilePlacementToolbar->toolStripButtonTileCollisionBrush->Checked = false;
            Editor->tilePlacementToolbar->toolStripButtonParallaxTool->Checked = false;
            Editor->tilePlacementToolbar->toolStripButtonEntityTool->Checked = false;

            ToolType = tool;
            switch (ToolType) {
            case TOOL_SELECT:
                SelectionType = SELECTTYPE_TILES;
                Editor->tilePlacementToolbar->toolStripButtonSelect->Checked = true;
                break;
            case TOOL_TILE_STAMP:
                SelectionType = SELECTTYPE_TILES;
                Editor->tilePlacementToolbar->toolStripButtonTileStamp->Checked = true;
                break;
            case TOOL_ERASE:
                SelectionType = SELECTTYPE_TILES;
                Editor->tilePlacementToolbar->toolStripButtonErase->Checked = true;
                break;
			case TOOL_TILE_EYEDROPPER:
                SelectionType = SELECTTYPE_TILES;
				Editor->tilePlacementToolbar->toolStripButtonTileEyedropper->Checked = true;
				break;
        	case TOOL_TILE_BUCKET_FILL:
                SelectionType = SELECTTYPE_TILES;
				Editor->tilePlacementToolbar->toolStripButtonTileBucketFill->Checked = true;
				break;
        	case TOOL_TILE_COLLISION_BRUSH:
                SelectionType = SELECTTYPE_TILES;
				Editor->tilePlacementToolbar->toolStripButtonTileCollisionBrush->Checked = true;
				break;
        	case TOOL_PARALLAX_RESIZER:
                SelectionType = SELECTTYPE_PARALLAX;
				Editor->tilePlacementToolbar->toolStripButtonParallaxTool->Checked = true;
				break;
        	case TOOL_ENTITY_TOOL:
                SelectionType = SELECTTYPE_ENTITIES;
				Editor->tilePlacementToolbar->toolStripButtonEntityTool->Checked = true;
				break;
            }
        }

        // Events
        void OnMouseDown(MouseEventArgs* e) {
            Control::OnMouseDown(e);

            auto shortcutModifier = 0;
            if (!!(e->Modifier & KMOD_ALT)) shortcutModifier |= SMOD_ALT;
            if (!!(e->Modifier & KMOD_CTRL)) shortcutModifier |= SMOD_CTRL;
            if (!!(e->Modifier & KMOD_SHIFT)) shortcutModifier |= SMOD_SHIFT;

            switch (e->Button) {
            case SDL_BUTTON(SDL_BUTTON_LEFT):
                switch (ToolType) {
                    case TOOL_SELECT:
                        MouseTileSelectDown(e);
                        break;
                    case TOOL_ERASE:
                        MouseTileErase(e);
                        break;
                    case TOOL_TILE_STAMP:
                        MouseTileStamp(e);
                        break;
                    case TOOL_TILE_EYEDROPPER:
                        MouseTileEyedropper(e);
                        break;
                    case TOOL_TILE_COLLISION_BRUSH:
                        if (shortcutModifier == 0)
                            MouseTileCollisionBrush(e, false);
                        break;
                    case TOOL_ENTITY_TOOL:
                        MouseEntityToolDown(e);
                        break;
                }
                break;
            case SDL_BUTTON(SDL_BUTTON_RIGHT):
                switch (ToolType) {
                case TOOL_TILE_COLLISION_BRUSH:
                    if (shortcutModifier == 0)
                        MouseTileCollisionBrush(e, true);
                    break;
                }
                break;
            case SDL_BUTTON(SDL_BUTTON_MIDDLE):
                if (SDL_CaptureMouse(SDL_TRUE) < 0) {
                    fprintf(stderr, "SDL_CaptureMouse failed: %s\n", SDL_GetError());
                    break;
                }
                /*if (SDL_SetRelativeMouseMode(SDL_TRUE) < 0) {
                    fprintf(stderr, "SDL_SetRelativeMouseMode failed: %s\n", SDL_GetError());
                    break;
                }*/

                MouseCaptured = this;
                SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL));

                ClickDragType = CLICKDRAG_VIEW_PAN;
                ClickDragStartX = e->X;
                ClickDragStartY = e->Y;
                ClickDragStartViewX = (int)ViewX;
                ClickDragStartViewY = (int)ViewY;
                break;
            }
        }
        void OnMouseMove(MouseEventArgs* e) {
            auto shortcutModifier = 0;
            if (!!(e->Modifier & KMOD_ALT)) shortcutModifier |= SMOD_ALT;
            if (!!(e->Modifier & KMOD_CTRL)) shortcutModifier |= SMOD_CTRL;
            if (!!(e->Modifier & KMOD_SHIFT)) shortcutModifier |= SMOD_SHIFT;

            if (ClickDragType == CLICKDRAG_NONE) {
                switch (e->Button) {
                case 0:
                    switch (ToolType) {
                    case TOOL_ENTITY_TOOL:
                        MouseEntityToolHover(e);
                        break;
                    }
                    break;
                case SDL_BUTTON(SDL_BUTTON_LEFT):
                    switch (ToolType) {
                    case TOOL_SELECT:
                        MouseTileSelectBegin(e);
                        break;
                    case TOOL_ERASE:
                        MouseTileErase(e);
                        break;
                    case TOOL_TILE_STAMP:
                        MouseTileStamp(e);
                        break;
                    case TOOL_TILE_COLLISION_BRUSH:
                        if (shortcutModifier == SMOD_SHIFT)
                            MouseTileSelectBegin(e);
                        else
                            MouseTileCollisionBrush(e, false);
                        break;
                    case TOOL_ENTITY_TOOL:
                        MouseEntityToolMove(e);
                        break;
                    }
                    break;
                case SDL_BUTTON(SDL_BUTTON_RIGHT):
                    switch (ToolType) {
                    case TOOL_TILE_COLLISION_BRUSH:
                        if (shortcutModifier == SMOD_SHIFT)
                            MouseTileSelectBegin(e);
                        else
                            MouseTileCollisionBrush(e, true);
                        break;
                    }
                    break;
                }
            }
            else if (ClickDragType == CLICKDRAG_MOVE) {
                switch (e->Button) {
                case SDL_BUTTON(SDL_BUTTON_LEFT):
                    switch (ToolType) {
                    case TOOL_ENTITY_TOOL:
                        MouseEntityToolDragMove(e);
                        break;
                    }
                    break;
                }
            }
            else if (ClickDragType == CLICKDRAG_VIEW_PAN) {
                int deltaX = -(e->X - ClickDragStartX) * Graphics::Views->Width / Graphics::ViewOutputs->Width;
                int deltaY = -(e->Y - ClickDragStartY) * Graphics::Views->Height / Graphics::ViewOutputs->Height;
                ViewX = (float)(deltaX + ClickDragStartViewX);
                ViewY = (float)(deltaY + ClickDragStartViewY);
            }

            MouseWorldX = e->X;
            MouseWorldY = e->Y;
            WindowToWorldCoords(&MouseWorldX, &MouseWorldY);

            UpdateRenderTarget = true;
            Control::OnMouseMove(e);
        }
        void OnMouseUp(MouseEventArgs* e) {
            ActionSiblingID++;
            ActionSiblingID &= 0xFF;

            if (ClickDragType == CLICKDRAG_NONE) {
                switch (e->Button) {
                case SDL_BUTTON(SDL_BUTTON_LEFT):
                    switch (ToolType) {
                    case TOOL_ENTITY_TOOL:
                        MouseEntityToolUp(e);
                        break;
                    }

                    UpdateRenderTarget = true;
                    break;
                }
            }
            else if (ClickDragType == CLICKDRAG_MOVE) {
                switch (e->Button) {
                case SDL_BUTTON(SDL_BUTTON_LEFT):
                    switch (ToolType) {
                    case TOOL_ENTITY_TOOL:
                        MouseEntityToolDragEnd(e);
                        break;
                    }

                    ClickDragType = CLICKDRAG_NONE;
                    UpdateRenderTarget = true;
                    break;
                }
            }
            else if (ClickDragType == CLICKDRAG_VIEW_PAN) {
                MouseCaptured = NULL;
                SDL_CaptureMouse(SDL_FALSE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));

                // Return mouse back to position
                // SDL_WarpMouseInWindow(NULL, ClickDragStartX, ClickDragStartY);

                ClickDragType = CLICKDRAG_NONE;
                UpdateRenderTarget = true;
            }
            else if (ClickDragType == CLICKDRAG_HIGHLIGHT) {
                switch (e->Button) {
                case SDL_BUTTON(SDL_BUTTON_LEFT):
                    switch (ToolType) {
                    case TOOL_SELECT:
                        MouseTileSelectEnd(e);
                        break;
                    case TOOL_TILE_COLLISION_BRUSH:
                        MouseTileCollisionBrushSelectEnd(e, e->Button == SDL_BUTTON(SDL_BUTTON_RIGHT));
                        ActionSiblingID++;
                        ActionSiblingID &= 0xFF;
                        break;
                    case TOOL_ENTITY_TOOL:
                        MouseEntityToolSelectEnd(e);
                        break;
                    }

                    ClickDragType = CLICKDRAG_NONE;
                    UpdateRenderTarget = true;
                    break;
                }
            }
            Control::OnMouseUp(e);
        }
        void OnMouseWheel(MouseEventArgs* e) {
            UpdateRenderTarget = true;

            // Ignore mouse wheel if click-dragging
            if (MouseCaptured == this)
                return;

            const Uint8* state = SDL_GetKeyboardState(NULL);
            if (e->Delta > 0)
                Action_ZoomIn();
            else if (e->Delta < 0)
                Action_ZoomOut();
        }

        void OnKeyDown(KeyEventArgs* e) {
            UpdateRenderTarget = true;
        }

        virtual void set_Size(::Size value) {
            Control::set_Size(value);

            UpdateRenderTarget = true;

            ::Size containerSize = value;
            Position containerWinPos = GetPositionInWindowCoords();

            if (Graphics::ViewOutputs->X != containerWinPos.X ||
                Graphics::ViewOutputs->Y != containerWinPos.Y ||
                Graphics::ViewOutputs->Width != containerSize.W ||
                Graphics::ViewOutputs->Height != containerSize.H) {

                Graphics::View_SetSize(0, (int)(containerSize.W / ViewScale), (int)(containerSize.H / ViewScale));
                Graphics::ViewOutputs->ScaleType = -1; // Custom scale
                Graphics::ViewOutputs->X = containerWinPos.X;
                Graphics::ViewOutputs->Y = containerWinPos.Y;
                Graphics::ViewOutputs->Width = containerSize.W;
                Graphics::ViewOutputs->Height = containerSize.H;
            }
        }

        void RemapStampDataToBePlaced() {
            if (StampDataToBePlaced == NULL)
                return;

            // for (int l = 0; l < LayerCount; l++) {
                // Layer* layer = &Layers[l];
                // int rowLength = layer->Width;
            Stamp* stamp = StampDataToBePlaced;
            for (int row = 0; row < stamp->Height; row++) {
                Tile* tileRow = &stamp->Data[row * stamp->Width];
                for (int col = 0; col < stamp->Width; col++) {
                    if (tileRow[col] == TILE_EMPTY)
                        continue;

                    int newID = Editor->LinkedStage->TileRemapArray[tileRow[col].ID];
                    if (newID == -1)
                        tileRow[col] = TILE_EMPTY;
                    else
                        tileRow[col].ID = newID;
                }
            }
            // }

            UpdateRenderTarget = true;
        }

        void DrawBG() {
            int bgTileSize = 128;
            int bgTileCountX = Graphics::CurrentView->Width / bgTileSize + 3;
            int bgTileCountY = Graphics::CurrentView->Height / bgTileSize + 3;

            int bgTileStartX = (int)0;
            int bgTileStartY = (int)0;
            int bgTileEndX = bgTileStartX + Editor->Layers[CurrentLayer].Width * 16;
            int bgTileEndY = bgTileStartY + Editor->Layers[CurrentLayer].Height * 16;

            for (int yInd = 0, y = bgTileStartY; y < bgTileEndY; y += bgTileSize) {
                int tileRemainingY = M_MIN(bgTileEndY - y, bgTileSize);

                for (int xInd = 0, x = bgTileStartX; x < bgTileEndX; x += bgTileSize) {
                    int tileRemainingX = M_MIN(bgTileEndX - x, bgTileSize);

                    if (((xInd + yInd) & 1) == 0)
                        GameLinker::HatchFuncs.Draw.Rectangle(x << 16, y << 16, tileRemainingX << 16, tileRemainingY << 16, Editor->BGColor1, BLEND_NONE);
                    else
                        GameLinker::HatchFuncs.Draw.Rectangle(x << 16, y << 16, tileRemainingX << 16, tileRemainingY << 16, Editor->BGColor2, BLEND_NONE);

                    xInd++;
                }

                yInd++;
            }
        }
        void DrawHighlightedRect(int x, int y, int w, int h) {
            x <<= 16;
            y <<= 16;
            w <<= 16;
            h <<= 16;
            int borderSize = 4 << 16;
            int borderSizeHalf = borderSize >> 1;
            Graphics::DrawRectangle(x, y, w, h, TileHighlightSelected, BLEND_TRANSPARENT);

            Graphics::DrawRectangle(x - borderSizeHalf, y - borderSizeHalf, borderSize, h + borderSize, TileHighlightSelected, BLEND_NONE);
            Graphics::DrawRectangle(x + w - borderSizeHalf, y - borderSizeHalf, borderSize, h + borderSize, TileHighlightSelected, BLEND_NONE);
            Graphics::DrawRectangle(x - borderSizeHalf, y - borderSizeHalf, w + borderSize, borderSize, TileHighlightSelected, BLEND_NONE);
            Graphics::DrawRectangle(x - borderSizeHalf, y + h - borderSizeHalf, w + borderSize, borderSize, TileHighlightSelected, BLEND_NONE);
        }
        void Update() {
        }
        void Render() {
            Graphics::Views->X = (int)ViewX;
            Graphics::Views->Y = (int)ViewY;
            Graphics::CurrentView = Graphics::Views;

            auto Bounds = GetScreenRect();

            if (UpdateRenderTarget) {
                UpdateRenderTarget = false;

                SDL_SetRenderTarget(UI::Graphics::Renderer::Renderer, FrameBufferTexture);
                {
                    SDL_RenderSetScale(UI::Graphics::Renderer::Renderer, ViewScale, ViewScale);
                    SDL_SetRenderDrawColor(UI::Graphics::Renderer::Renderer, BackColor.R, BackColor.G, BackColor.B, 0xFF);
                    SDL_RenderClear(UI::Graphics::Renderer::Renderer);

                    DrawBG();

                    int sizeMatchW = Scene::Layers[CurrentLayer].Width,
                        sizeMatchH = Scene::Layers[CurrentLayer].Height;
                    for (int layerIndex = 0; layerIndex < Editor->LayerCount; layerIndex++) {
                        Layer* layer = &Scene::Layers[layerIndex];
                        layer->Hidden[0] = CurrentLayer != layerIndex && (!ShowLikeLayers || (layer->Width != sizeMatchW || layer->Height != sizeMatchH));
                    }

                    Graphics::DrawAll_Editor(Editor->LayerCount);

                    for (int i = 0; i < Editor->EntityCount; i++) {
                        auto ent = &Editor->EntitySlots[i];
                        auto entEd = &Editor->EntityEditorSlots[i];

                        if (!(ent->Filter & CurrentFilter))
                            continue;

                        GameLinker::CurrentEntity = ent;
                        GameLinker::State.CurrentEntityIndex = i;

                        Graphics::ResetHighlightBounds(Vector2(ent->Position.X.Whole, ent->Position.Y.Whole));

                        int classIndex = GameLinker::CurrentEntity->ClassID;
                        int linkedClassIndex = classIndex == -1 ? -1 : Editor->LinkedStage->Classes[classIndex]->LinkedClassIndex;
                        if (linkedClassIndex > -1) {
                            auto onEditorDraw = GameLinker::ClassList[linkedClassIndex].onEditorDraw;
                            if (onEditorDraw)
                                onEditorDraw();
                        }

                        if (linkedClassIndex == -1 || 
                            (Graphics::DrawMinPos.X == Graphics::DrawMaxPos.X && Graphics::DrawMinPos.Y == Graphics::DrawMaxPos.Y)) {
                            Graphics::DrawRectangle(ent->Position.X - 0x100000, ent->Position.Y - 0x100000, 0x200000, 0x200000, Color(0x000000, 0xFF), BLEND_NONE);
                            // For making a circle graphic:
                            // bool Studio::Textures::CreateTextureFromData(SDL_Texture** texture, Uint8* data, Pixel* palette, int width, int height)

                            SDL_Rect highBounds = { ent->Position.X.Whole - 16, ent->Position.Y.Whole - 16, 32, 32 };
                            Graphics::SetHighlightBounds(highBounds);
                        }

                        entEd->MinPos = Graphics::DrawMinPos;
                        entEd->MaxPos = Graphics::DrawMaxPos;
                        switch (entEd->SelectionType) {
                        case EMS_HOVERING:
                        case EMS_CLICKSTARTED:
                            Graphics::DrawRectangle(
                                (Graphics::DrawMinPos.X.Full) << 16,
                                (Graphics::DrawMinPos.Y.Full) << 16,
                                (Graphics::DrawMaxPos.X.Full - Graphics::DrawMinPos.X.Full) << 16,
                                (Graphics::DrawMaxPos.Y.Full - Graphics::DrawMinPos.Y.Full) << 16, TileHighlightSelected, BLEND_TRANSPARENT);
                            break;
                        case EMS_SELECTED:
                            DrawHighlightedRect(
                                Graphics::DrawMinPos.X.Full,
                                Graphics::DrawMinPos.Y.Full,
                                Graphics::DrawMaxPos.X.Full - Graphics::DrawMinPos.X.Full,
                                Graphics::DrawMaxPos.Y.Full - Graphics::DrawMinPos.Y.Full);
                            break;
                        }
                    }

                    switch (ToolType) {
                        // Draw tile selection related things
                    case TOOL_SELECT:
                        // Draw mouse tile cursor
                        if (ClickDragType == CLICKDRAG_NONE) {
                            int x = (MouseWorldX >> 4) << 20;
                            int y = (MouseWorldY >> 4) << 20;
                            Graphics::DrawRectangle(x, y, 16 << 16, 16 << 16, TileHighlightHover, BLEND_TRANSPARENT);
                        }
                        // Draw mouse-defined tile selection region (snaps to tile grid)
                        else if (ClickDragType == CLICKDRAG_HIGHLIGHT) {
                            int x1 = ClickDragStartX;
                            int y1 = ClickDragStartY;
                            WindowToWorldCoords(&x1, &y1);
                            int x2 = MouseWorldX;
                            int y2 = MouseWorldY;

                            int x = M_MIN(x1, x2) >> 4;
                            int y = M_MIN(y1, y2) >> 4;
                            int w = (M_MAX(x1, x2) >> 4) - x + 1;
                            int h = (M_MAX(y1, y2) >> 4) - y + 1;
                            Graphics::DrawRectangle(x << 20, y << 20, w << 20, h << 20, TileHighlightHover, BLEND_TRANSPARENT);

                            x = x << 20;
                            y = y << 20;
                            w = w << 20;
                            h = h << 20;
                            int borderSize = 2 << 16;
                            int borderSizeHalf = borderSize >> 1;
                            Graphics::DrawRectangle(x - borderSizeHalf, y - borderSizeHalf, borderSize, h + borderSize, TileHighlightHover, BLEND_NONE);
                            Graphics::DrawRectangle(x + w - borderSizeHalf, y - borderSizeHalf, borderSize, h + borderSize, TileHighlightHover, BLEND_NONE);
                            Graphics::DrawRectangle(x - borderSizeHalf, y - borderSizeHalf, w + borderSize, borderSize, TileHighlightHover, BLEND_NONE);
                            Graphics::DrawRectangle(x - borderSizeHalf, y + h - borderSizeHalf, w + borderSize, borderSize, TileHighlightHover, BLEND_NONE);
                        }

                        // Draw tile selection (hidden while selecting new region)
                        if (ClickDragType != CLICKDRAG_HIGHLIGHT && TileSelectBounds.w > 0) {
                            int x = TileSelectBounds.x << 20;
                            int y = TileSelectBounds.y << 20;
                            int w = TileSelectBounds.w << 20;
                            int h = TileSelectBounds.h << 20;
                            int borderSize = 4 << 16;
                            int borderSizeHalf = borderSize >> 1;
                            Graphics::DrawRectangle(x, y, w, h, TileHighlightSelected, BLEND_TRANSPARENT);

                            Graphics::DrawRectangle(x - borderSizeHalf, y - borderSizeHalf, borderSize, h + borderSize, TileHighlightSelected, BLEND_NONE);
                            Graphics::DrawRectangle(x + w - borderSizeHalf, y - borderSizeHalf, borderSize, h + borderSize, TileHighlightSelected, BLEND_NONE);
                            Graphics::DrawRectangle(x - borderSizeHalf, y - borderSizeHalf, w + borderSize, borderSize, TileHighlightSelected, BLEND_NONE);
                            Graphics::DrawRectangle(x - borderSizeHalf, y + h - borderSizeHalf, w + borderSize, borderSize, TileHighlightSelected, BLEND_NONE);
                        }
                        break;
                    case TOOL_ERASE:
                    case TOOL_TILE_EYEDROPPER:
                    case TOOL_TILE_BUCKET_FILL:
                    case TOOL_TILE_COLLISION_BRUSH:
                        // Draw mouse tile cursor
                        if (ClickDragType == CLICKDRAG_NONE) {
                            // Draw mouse tile cursor
                            int x = (MouseWorldX >> 4) << 20;
                            int y = (MouseWorldY >> 4) << 20;
                            Graphics::DrawRectangle(x, y, 16 << 16, 16 << 16, TileHighlightHover, BLEND_TRANSPARENT);
                        }
                        // Draw mouse-defined tile selection region (snaps to tile grid)
                        else if (ClickDragType == CLICKDRAG_HIGHLIGHT) {
                            int x1 = ClickDragStartX;
                            int y1 = ClickDragStartY;
                            WindowToWorldCoords(&x1, &y1);
                            int x2 = MouseWorldX;
                            int y2 = MouseWorldY;

                            int x = M_MIN(x1, x2) >> 4;
                            int y = M_MIN(y1, y2) >> 4;
                            int w = (M_MAX(x1, x2) >> 4) - x + 1;
                            int h = (M_MAX(y1, y2) >> 4) - y + 1;
                            Graphics::DrawRectangle(x << 20, y << 20, w << 20, h << 20, TileHighlightHover, BLEND_TRANSPARENT);

                            x = x << 20;
                            y = y << 20;
                            w = w << 20;
                            h = h << 20;
                            int borderSize = 2 << 16;
                            int borderSizeHalf = borderSize >> 1;
                            Graphics::DrawRectangle(x - borderSizeHalf, y - borderSizeHalf, borderSize, h + borderSize, TileHighlightHover, BLEND_NONE);
                            Graphics::DrawRectangle(x + w - borderSizeHalf, y - borderSizeHalf, borderSize, h + borderSize, TileHighlightHover, BLEND_NONE);
                            Graphics::DrawRectangle(x - borderSizeHalf, y - borderSizeHalf, w + borderSize, borderSize, TileHighlightHover, BLEND_NONE);
                            Graphics::DrawRectangle(x - borderSizeHalf, y + h - borderSizeHalf, w + borderSize, borderSize, TileHighlightHover, BLEND_NONE);
                        }
                        break;

                        // Draw the current stamp
                    case TOOL_TILE_STAMP:
                        {
                            int tileIDs, tileIDe;
                            Editor->tileSelector->GetHighlightBounds(&tileIDs, &tileIDe);

                            int i = 0;
                            int mx = (MouseWorldX >> 4);
                            int my = (MouseWorldY >> 4);
                            for (int ty = my; ty < my + StampDataToBePlaced->Height; ty++) {
                                for (int tx = mx; tx < mx + StampDataToBePlaced->Width; tx++) {
                                    Graphics::DrawTile(tx << 20, ty << 20, StampDataToBePlaced->Data[i++]);
                                    Graphics::DrawRectangle(tx << 20, ty << 20, 16 << 16, 16 << 16, TileHighlightHover, BLEND_TRANSPARENT);
                                }
                            }
                        }
                        break;

                        // Draw highlight area
                    case TOOL_ENTITY_TOOL:
                        if (ClickDragType == CLICKDRAG_HIGHLIGHT) {
                            int x1 = ClickDragStartX;
                            int y1 = ClickDragStartY;
                            WindowToWorldCoords(&x1, &y1);
                            int x2 = MouseWorldX;
                            int y2 = MouseWorldY;

                            int x = M_MIN(x1, x2);
                            int y = M_MIN(y1, y2);
                            int w = (M_MAX(x1, x2)) - x + 1;
                            int h = (M_MAX(y1, y2)) - y + 1;
                            x = x << 16;
                            y = y << 16;
                            w = w << 16;
                            h = h << 16;

                            Graphics::DrawRectangle(x, y, w, h, TileHighlightHover, BLEND_TRANSPARENT);

                            int borderSize = 2 << 16;
                            int borderSizeHalf = borderSize >> 1;
                            Graphics::DrawRectangle(x - borderSizeHalf, y - borderSizeHalf, borderSize, h + borderSize, TileHighlightHover, BLEND_NONE);
                            Graphics::DrawRectangle(x + w - borderSizeHalf, y - borderSizeHalf, borderSize, h + borderSize, TileHighlightHover, BLEND_NONE);
                            Graphics::DrawRectangle(x - borderSizeHalf, y - borderSizeHalf, w + borderSize, borderSize, TileHighlightHover, BLEND_NONE);
                            Graphics::DrawRectangle(x - borderSizeHalf, y + h - borderSizeHalf, w + borderSize, borderSize, TileHighlightHover, BLEND_NONE);
                        }
                        break;
                    }

                    SDL_RenderSetScale(UI::Graphics::Renderer::Renderer, 1.0f, 1.0f);
                }
                SDL_SetRenderTarget(UI::Graphics::Renderer::Renderer, NULL);
            }

            // Render to screen
            SDL_Rect src = { 0, 0, Bounds.w, Bounds.h };

            SDL_Rect boundsAdj = Bounds;
            UI::Graphics::Renderer::DstRectAdjustment(&boundsAdj);

            SDL_RenderCopy(UI::Graphics::Renderer::Renderer, FrameBufferTexture, &src, &boundsAdj);

            // Update view size if requested
            if (UpdateZoomViewCoords) {
                ViewScale = ViewScaleNext;
                Graphics::View_SetSize(0, (int)(Bounds.w / ViewScale), (int)(Bounds.h / ViewScale));
                Graphics::ViewOutputs->ScaleType = -1; // Custom scale
                Graphics::ViewOutputs->X = Bounds.x;
                Graphics::ViewOutputs->Y = Bounds.y;
                Graphics::ViewOutputs->Width = Bounds.w;
                Graphics::ViewOutputs->Height = Bounds.h;

                ViewX -= (Graphics::CurrentView->Width - ZoomViewStoredWidth) * ZoomViewMouseX;
                ViewY -= (Graphics::CurrentView->Height - ZoomViewStoredHeight) * ZoomViewMouseY;
                UpdateZoomViewCoords = false;

                UpdateRenderTarget = true;
            }
        }
    };
    struct LayerControls : FlowLayoutPanel {
        SceneEditor* Editor = NULL;

        Label* labelLayers;
        ListView* listViewLayers;
        ToolStrip* toolStripLayer;
        ToolStripButton* toolStripButtonAddLayer;
        ToolStripButton* toolStripButtonRemoveLayer;
        ToolStripButton* toolStripButtonDuplicateLayer;
        ToolStripButton* toolStripButtonMoveLayerUp;
        ToolStripButton* toolStripButtonMoveLayerDown;
        Label* labelSettings;
        Label* labelLayerName;
        TextboxBase* textboxLayerName;
        Button* buttonResizeLayer;
        Button* buttonEditScrollBehavior;
        Label* labelParallax;
        ListView* listParallaxLines;
        Button* buttonEditParallaxBehavior;

        struct Form_ResizeLayer : Form {
            TextboxBase* textBoxName;
            TextboxBase* numberBoxWidth;
            TextboxBase* numberBoxHeight;
            Button* buttonOK;
            Button* buttonCancel;
            Label* labelName;
            Label* labelWidth;
            Label* labelHeight;
            Label* labelNoUndo;

            Form_ResizeLayer(CString title, Layer* layer, String* layerName) : Form(250, 140, title) {
                char stringBuffer[8];

                ::Size formSize;
                Size = formSize = { 250, 140 };

                labelName = new Label("Name:");
                labelName->Location = { 10, 10 };
                labelName->Location.Y += (25 - labelName->Size.Get().H) / 2;

                labelWidth = new Label("Width:");
                labelWidth->Location = { 10, 40 };
                labelWidth->Location.Y += (25 - labelWidth->Size.Get().H) / 2;

                labelHeight = new Label("Height:");
                labelHeight->Location = { 10, 70 };
                labelHeight->Location.Y += (25 - labelWidth->Size.Get().H) / 2;

				if (layerName)
                	textBoxName = new TextboxBase(layerName);
				else
					textBoxName = new TextboxBase("New Layer");
                textBoxName->Location = { 60, 10 };
                textBoxName->Size = { 90, 25 };

                sprintf(stringBuffer, "%d", layer ? layer->Width : 64);
                numberBoxWidth = new TextboxBase(stringBuffer);
                numberBoxWidth->Location = { 60, 40 };
                numberBoxWidth->Size = { 90, 25 };

                sprintf(stringBuffer, "%d", layer ? layer->Height : 64);
                numberBoxHeight = new TextboxBase(stringBuffer);
                numberBoxHeight->Location = { 60, 70 };
                numberBoxHeight->Size = { 90, 25 };

                buttonCancel = new Button("Cancel");
                buttonCancel->Result = DialogResult::Cancel;
                buttonCancel->Location = { formSize.W - 100 - 10, formSize.H - 25 - 10 };
                buttonCancel->Size = { 100, 25 };
                buttonCancel->onClick += [this](auto object, auto e) -> void {
                    this->Result = DialogResult::Cancel;
                    this->Close();
                };

                buttonOK = new Button("OK");
                buttonOK->Result = DialogResult::OK;
                buttonOK->Location = { buttonCancel->Location.X - 100 - 10, buttonCancel->Location.Y };
                buttonOK->Size = { 100, 25 };
                buttonOK->onClick += [this](auto object, auto e) -> void {
                    this->Result = DialogResult::OK;
                    this->Close();
                };

                // Label:
                // "NOTE: This action cannot be undone!"

                this->Controls.Add(labelName);
                this->Controls.Add(labelWidth);
                this->Controls.Add(labelHeight);
                this->Controls.Add(textBoxName);
                this->Controls.Add(numberBoxWidth);
                this->Controls.Add(numberBoxHeight);
                this->Controls.Add(buttonOK);
                this->Controls.Add(buttonCancel);
            }
            ~Form_ResizeLayer() {
                delete textBoxName;
                delete numberBoxWidth;
                delete numberBoxHeight;
                delete buttonOK;
                delete buttonCancel;
                delete labelName;
                delete labelWidth;
                delete labelHeight;
                // delete labelNoUndo;
            }
        };
        struct Form_EditScrollBehavior : Form {
            Label* labelBehaviorType;
            ComboBox* comboBoxBehavior;
            Label* labelRelativeScroll;
            NumericUpDown* numericUpDownRelativeScroll;
            Label* labelConstantScroll;
            NumericUpDown* numericUpDownConstantScroll;
            Label* labelDrawGroup;
            ComboBox* comboBoxDrawGroups;
            Button* buttonOK;
            Button* buttonCancel;

            FlowLayoutPanel* mainPanel;

            Form_EditScrollBehavior(CString title) : Form(250, 140, title) {
                mainPanel = new FlowLayoutPanel();
                mainPanel->BackColor = Color(0x000000, 0x00);
                mainPanel->Dock = DOCK_FILL;
                mainPanel->FlowDirection = FlowDirection::LEFT_TO_RIGHT;
                mainPanel->Padding = 10;
                mainPanel->WrapContents = false;


                labelBehaviorType = new Label("Scroll Behavior:");
                labelBehaviorType->Anchor = ANCHOR_TOP;
                labelBehaviorType->Margin.Top = 5;
                labelBehaviorType->Margin.Right = 10;
                mainPanel->Controls.Add(labelBehaviorType);

                comboBoxBehavior = new ComboBox();
                comboBoxBehavior->Anchor = ANCHOR_TOP;
                comboBoxBehavior->Size = { 100, 25 };
                comboBoxBehavior->LineBreak = true;
                comboBoxBehavior->Margin.Bottom = 5;
                comboBoxBehavior->Items.Add("HORIZONTAL");
                comboBoxBehavior->Items.Add("VERTICAL");
                comboBoxBehavior->Items.Add("CUSTOM");
                comboBoxBehavior->Select(0);
                mainPanel->Controls.Add(comboBoxBehavior);


                labelRelativeScroll = new Label("Relative Scroll:");
                labelRelativeScroll->Anchor = ANCHOR_TOP;
                labelRelativeScroll->Margin.Top = 5;
                labelRelativeScroll->Margin.Right = 10;
                mainPanel->Controls.Add(labelRelativeScroll);

                numericUpDownRelativeScroll = new NumericUpDown();
                numericUpDownRelativeScroll->Anchor = ANCHOR_TOP;
                numericUpDownRelativeScroll->Size = { 100, 25 };
                numericUpDownRelativeScroll->LineBreak = true;
                numericUpDownRelativeScroll->Margin.Bottom = 5;
                numericUpDownRelativeScroll->Minimum = -256.0;
                numericUpDownRelativeScroll->Maximum = 256.0;
                numericUpDownRelativeScroll->Increment = 0.01;
                numericUpDownRelativeScroll->DecimalPlaces = 3;
                mainPanel->Controls.Add(numericUpDownRelativeScroll);


                labelConstantScroll = new Label("Constant Scroll:");
                labelConstantScroll->Anchor = ANCHOR_TOP;
                labelConstantScroll->Margin.Top = 5;
                labelConstantScroll->Margin.Right = 10;
                mainPanel->Controls.Add(labelConstantScroll);

                numericUpDownConstantScroll = new NumericUpDown();
                numericUpDownConstantScroll->Anchor = ANCHOR_TOP;
                numericUpDownConstantScroll->Size = { 100, 25 };
                numericUpDownConstantScroll->LineBreak = true;
                numericUpDownConstantScroll->Margin.Bottom = 5;
                numericUpDownConstantScroll->Minimum = -256.0;
                numericUpDownConstantScroll->Maximum = 256.0;
                numericUpDownConstantScroll->Increment = 0.01;
                numericUpDownConstantScroll->DecimalPlaces = 3;
                mainPanel->Controls.Add(numericUpDownConstantScroll);


                labelDrawGroup = new Label("Draw Group:");
                labelDrawGroup->Anchor = ANCHOR_TOP;
                labelDrawGroup->Margin.Top = 5;
                labelDrawGroup->Margin.Right = 10;
                mainPanel->Controls.Add(labelDrawGroup);

                comboBoxDrawGroups = new ComboBox();
                comboBoxDrawGroups->Anchor = ANCHOR_TOP;
                comboBoxDrawGroups->Size = { 100, 25 };
                comboBoxDrawGroups->LineBreak = true;
                comboBoxDrawGroups->Margin.Bottom = 5;
                comboBoxDrawGroups->Items.Add("0 (back)");
                comboBoxDrawGroups->Items.Add("1");
                comboBoxDrawGroups->Items.Add("2");
                comboBoxDrawGroups->Items.Add("3");
                comboBoxDrawGroups->Items.Add("4");
                comboBoxDrawGroups->Items.Add("5");
                comboBoxDrawGroups->Items.Add("6");
                comboBoxDrawGroups->Items.Add("7");
                comboBoxDrawGroups->Items.Add("8");
                comboBoxDrawGroups->Items.Add("9");
                comboBoxDrawGroups->Items.Add("10");
                comboBoxDrawGroups->Items.Add("11");
                comboBoxDrawGroups->Items.Add("12");
                comboBoxDrawGroups->Items.Add("13");
                comboBoxDrawGroups->Items.Add("14");
                comboBoxDrawGroups->Items.Add("15 (front)");
                comboBoxDrawGroups->Select(0);
                mainPanel->Controls.Add(comboBoxDrawGroups);


                buttonOK = new Button("OK");
                buttonOK->Anchor = ANCHOR_TOP;
                buttonOK->Size = { 100, 25 };
                buttonOK->Margin.Right = 5;
                buttonOK->onClick += [this](auto object, auto e) -> void {
                    this->Result = DialogResult::OK;
                    this->Close();
                };
                mainPanel->Controls.Add(buttonOK);

                buttonCancel = new Button("Cancel");
                buttonCancel->Anchor = ANCHOR_TOP;
                buttonCancel->Size = { 100, 25 };
                buttonCancel->onClick += [this](auto object, auto e) -> void {
                    this->Result = DialogResult::Cancel;
                    this->Close();
                };
                mainPanel->Controls.Add(buttonCancel);


                this->Controls.Add(mainPanel);
                this->UpdateLayout(); // This should theoretically happen during Controls.Add

                this->Size = { 250, buttonCancel->Location.Y + buttonCancel->Size.Get().H + mainPanel->Padding.Bottom };
            }
            ~Form_EditScrollBehavior() {
                delete labelBehaviorType;
                delete comboBoxBehavior;
                delete labelRelativeScroll;
                delete numericUpDownRelativeScroll;
                delete labelConstantScroll;
                delete numericUpDownConstantScroll;
                delete labelDrawGroup;
                delete comboBoxDrawGroups;
                delete buttonOK;
                delete buttonCancel;

                delete mainPanel;
            }
        };
        struct Form_EditParallaxBehavior : Form {
            Label* labelStartPx;
            NumericUpDown* numericUpDownStartPx;
            Label* labelSizePx;
            NumericUpDown* numericUpDownSizePx;
            Label* labelRelativeScroll;
            NumericUpDown* numericUpDownRelativeScroll;
            Label* labelConstantScroll;
            NumericUpDown* numericUpDownConstantScroll;
            CheckBox* checkBoxCanDeform;
            Button* buttonOK;
            Button* buttonCancel;

            FlowLayoutPanel* mainPanel;

            Form_EditParallaxBehavior(CString title, Layer* layer) : Form(250, 140, title) {
                int lineCount = M_MAX(layer->Width, layer->Height) * TILE_SIZE;

                mainPanel = new FlowLayoutPanel();
                mainPanel->BackColor = Color(0x000000, 0x00);
                mainPanel->Dock = DOCK_FILL;
                mainPanel->FlowDirection = FlowDirection::LEFT_TO_RIGHT;
                mainPanel->Padding = 10;
                mainPanel->WrapContents = false;


                labelStartPx = new Label("Start (px):");
                labelStartPx->Anchor = ANCHOR_TOP;
                labelStartPx->Margin.Top = 5;
                labelStartPx->Margin.Right = 10;
                mainPanel->Controls.Add(labelStartPx);

                numericUpDownStartPx = new NumericUpDown();
                numericUpDownStartPx->Anchor = ANCHOR_TOP;
                numericUpDownStartPx->Size = { 100, 25 };
                numericUpDownStartPx->LineBreak = true;
                numericUpDownStartPx->Margin.Bottom = 5;
                numericUpDownStartPx->Minimum = 0.0;
                numericUpDownStartPx->Maximum = lineCount;
                numericUpDownStartPx->DecimalPlaces = 0;
                mainPanel->Controls.Add(numericUpDownStartPx);


                labelSizePx = new Label("Size (px):");
                labelSizePx->Anchor = ANCHOR_TOP;
                labelSizePx->Margin.Top = 5;
                labelSizePx->Margin.Right = 10;
                mainPanel->Controls.Add(labelSizePx);

                numericUpDownSizePx = new NumericUpDown();
                numericUpDownSizePx->Anchor = ANCHOR_TOP;
                numericUpDownSizePx->Size = { 100, 25 };
                numericUpDownSizePx->LineBreak = true;
                numericUpDownSizePx->Margin.Bottom = 5;
                numericUpDownSizePx->Minimum = 0.0;
                numericUpDownSizePx->Maximum = lineCount;
                numericUpDownSizePx->DecimalPlaces = 0;
                mainPanel->Controls.Add(numericUpDownSizePx);


                labelRelativeScroll = new Label("Relative Parallax:");
                labelRelativeScroll->Anchor = ANCHOR_TOP;
                labelRelativeScroll->Margin.Top = 5;
                labelRelativeScroll->Margin.Right = 10;
                mainPanel->Controls.Add(labelRelativeScroll);

                numericUpDownRelativeScroll = new NumericUpDown();
                numericUpDownRelativeScroll->Anchor = ANCHOR_TOP;
                numericUpDownRelativeScroll->Size = { 100, 25 };
                numericUpDownRelativeScroll->LineBreak = true;
                numericUpDownRelativeScroll->Margin.Bottom = 5;
                numericUpDownRelativeScroll->Minimum = -256.0;
                numericUpDownRelativeScroll->Maximum = 256.0;
                numericUpDownRelativeScroll->Increment = 0.01;
                numericUpDownRelativeScroll->DecimalPlaces = 3;
                mainPanel->Controls.Add(numericUpDownRelativeScroll);


                labelConstantScroll = new Label("Constant Parallax:");
                labelConstantScroll->Anchor = ANCHOR_TOP;
                labelConstantScroll->Margin.Top = 5;
                labelConstantScroll->Margin.Right = 10;
                mainPanel->Controls.Add(labelConstantScroll);

                numericUpDownConstantScroll = new NumericUpDown();
                numericUpDownConstantScroll->Anchor = ANCHOR_TOP;
                numericUpDownConstantScroll->Size = { 100, 25 };
                numericUpDownConstantScroll->LineBreak = true;
                numericUpDownConstantScroll->Margin.Bottom = 5;
                numericUpDownConstantScroll->Minimum = -256.0;
                numericUpDownConstantScroll->Maximum = 256.0;
                numericUpDownConstantScroll->Increment = 0.01;
                numericUpDownConstantScroll->DecimalPlaces = 3;
                mainPanel->Controls.Add(numericUpDownConstantScroll);


                checkBoxCanDeform = new CheckBox("Can Deform?");
                checkBoxCanDeform->Anchor = ANCHOR_TOP;
                checkBoxCanDeform->Margin.Top = 5;
                checkBoxCanDeform->Margin.Right = 10;
                checkBoxCanDeform->Margin.Bottom = 5;
                checkBoxCanDeform->LineBreak = true;
                mainPanel->Controls.Add(checkBoxCanDeform);


                buttonOK = new Button("OK");
                buttonOK->Anchor = ANCHOR_TOP;
                buttonOK->Size = { 100, 25 };
                buttonOK->Margin.Right = 5;
                buttonOK->onClick += [this](auto object, auto e) -> void {
                    this->Result = DialogResult::OK;
                    this->Close();
                };
                mainPanel->Controls.Add(buttonOK);

                buttonCancel = new Button("Cancel");
                buttonCancel->Anchor = ANCHOR_TOP;
                buttonCancel->Size = { 100, 25 };
                buttonCancel->onClick += [this](auto object, auto e) -> void {
                    this->Result = DialogResult::Cancel;
                    this->Close();
                };
                mainPanel->Controls.Add(buttonCancel);


                this->Controls.Add(mainPanel);
                this->UpdateLayout(); // This should theoretically happen during Controls.Add

                this->Size = { 250, buttonCancel->Location.Y + buttonCancel->Size.Get().H + mainPanel->Padding.Bottom };
            }
            ~Form_EditParallaxBehavior() {
                delete labelStartPx;
                delete numericUpDownStartPx;
                delete labelSizePx;
                delete numericUpDownSizePx;
                delete labelRelativeScroll;
                delete numericUpDownRelativeScroll;
                delete labelConstantScroll;
                delete numericUpDownConstantScroll;
                delete checkBoxCanDeform;
                delete buttonOK;
                delete buttonCancel;

                delete mainPanel;
            }
        };


        LayerControls(SceneEditor* editor) : FlowLayoutPanel() {
            Editor = editor;

            Dock = DOCK_FILL;
            Size = { 32, 0 };
            Padding = 6;

            BackColor = Color(0x282C34, 0xFF);
            ForeColor = Color(0xFFFFFF, 0xFF);

            FlowDirection = FlowDirection::TOP_TO_BOTTOM;

            // labelLayers
            labelLayers = new Label("Layers");
            labelLayers->Anchor = ANCHOR_LEFT;
            Controls.Add(labelLayers);

            // listViewLayers
            listViewLayers = new ListView();
            listViewLayers->Margin.Top = 4;
            listViewLayers->LayoutType = ListViewLayout::List;
            listViewLayers->Columns.Add(new ColumnHeader("L", 20, 1));
            listViewLayers->Columns.Add(new ColumnHeader("V", 20, 2));
            listViewLayers->Columns.Add(new ColumnHeader("Name", -1, 0));
            listViewLayers->Size = { 160, listViewLayers->ItemSize * 6 + listViewLayers->HeaderSize };
            listViewLayers->onSelectedIndexChanged += std::bind(&LayerControls::listViewLayers_onSelectedIndexChanged, this, std::placeholders::_1, std::placeholders::_2);
            Controls.Add(listViewLayers);

            // toolStripLayer
            toolStripLayer = new ToolStrip();
            toolStripLayer->BackColor = BackColor;

            toolStripButtonAddLayer = new ToolStripButton();
            toolStripButtonAddLayer->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonAddLayer->Icon, "Resources_Editor/ICON_ADD.png");
            toolStripButtonAddLayer->onMouseClick += std::bind(&LayerControls::toolStripButtonAddLayer_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            toolStripLayer->Controls.Add(toolStripButtonAddLayer);

            toolStripButtonRemoveLayer = new ToolStripButton();
            toolStripButtonRemoveLayer->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonRemoveLayer->Icon, "Resources_Editor/ICON_DELETE.png");
            toolStripButtonRemoveLayer->onMouseClick += std::bind(&LayerControls::toolStripButtonRemoveLayer_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            toolStripLayer->Controls.Add(toolStripButtonRemoveLayer);

            toolStripButtonDuplicateLayer = new ToolStripButton();
            toolStripButtonDuplicateLayer->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonDuplicateLayer->Icon, "Resources_Editor/ICON_DUPLICATE.png");
            toolStripButtonDuplicateLayer->onMouseClick += std::bind(&LayerControls::toolStripButtonDuplicateLayer_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            toolStripLayer->Controls.Add(toolStripButtonDuplicateLayer);

            toolStripButtonMoveLayerUp = new ToolStripButton();
            toolStripButtonMoveLayerUp->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonMoveLayerUp->Icon, "Resources_Editor/ICON_MOVE_UP.png");
            toolStripButtonMoveLayerUp->onMouseClick += std::bind(&LayerControls::toolStripButtonMoveLayerUp_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            toolStripLayer->Controls.Add(toolStripButtonMoveLayerUp);

            toolStripButtonMoveLayerDown = new ToolStripButton();
            toolStripButtonMoveLayerDown->IconSize = { 11, 11 };
            Studio::Textures::CreateTextureFromFilePNG(&toolStripButtonMoveLayerDown->Icon, "Resources_Editor/ICON_MOVE_DOWN.png");
            toolStripButtonMoveLayerDown->onMouseClick += std::bind(&LayerControls::toolStripButtonMoveLayerDown_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            toolStripLayer->Controls.Add(toolStripButtonMoveLayerDown);

            Controls.Add(toolStripLayer);

            // labelSettings
            labelSettings = new Label("Settings");
            labelSettings->Anchor = ANCHOR_LEFT;
            labelSettings->Margin.Top = 8;
            Controls.Add(labelSettings);

            // buttonResizeLayer
            buttonResizeLayer = new Button("Resize / Rename Layer...");
            buttonResizeLayer->Size = { 200, 25 };
            buttonResizeLayer->Margin.Top = 4;
            buttonResizeLayer->onMouseClick += std::bind(&LayerControls::buttonResizeLayer_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            Controls.Add(buttonResizeLayer);

            // buttonEditScrollBehavior
            buttonEditScrollBehavior = new Button("Edit Scroll Behavior...");
            buttonEditScrollBehavior->Size = { 200, 25 };
            buttonEditScrollBehavior->Margin.Top = 4;
            buttonEditScrollBehavior->onMouseClick += std::bind(&LayerControls::buttonEditScrollBehavior_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            Controls.Add(buttonEditScrollBehavior);

            // labelParallax
            labelParallax = new Label("Parallax");
            labelParallax->Anchor = ANCHOR_LEFT;
            labelParallax->Margin.Top = 8;
            Controls.Add(labelParallax);

            // listParallaxLines
            listParallaxLines = new ListView();
            listParallaxLines->Margin.Top = 4;
            listParallaxLines->LayoutType = ListViewLayout::List;
            listParallaxLines->Columns.Add(new ColumnHeader("L", 20, 1));
            listParallaxLines->Columns.Add(new ColumnHeader("V", 20, 2));
            listParallaxLines->Columns.Add(new ColumnHeader("Name", -1, 0));
            listParallaxLines->Size = { 160, listViewLayers->ItemSize * 6 + listViewLayers->HeaderSize };
            listParallaxLines->onSelectedIndexChanged += std::bind(&LayerControls::listParallaxLines_onSelectedIndexChanged, this, std::placeholders::_1, std::placeholders::_2);
            Controls.Add(listParallaxLines);

            // buttonEditParallaxBehavior
            buttonEditParallaxBehavior = new Button("Edit Parallax...");
            buttonEditParallaxBehavior->Margin.Top = 4;
            buttonEditParallaxBehavior->Size = { 150, 25 };
            buttonEditParallaxBehavior->onMouseClick += std::bind(&LayerControls::buttonEditParallaxBehavior_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
            buttonEditParallaxBehavior->Enabled = (listParallaxLines->SelectedIndex >= 0);
            Controls.Add(buttonEditParallaxBehavior);
        }
        ~LayerControls() {
            delete labelLayers;
            delete listViewLayers;
            delete toolStripLayer;
            delete toolStripButtonAddLayer;
            delete toolStripButtonRemoveLayer;
            delete toolStripButtonDuplicateLayer;
            delete toolStripButtonMoveLayerUp;
            delete toolStripButtonMoveLayerDown;
            delete labelSettings;
            // delete labelLayerName;
            // delete textboxLayerName;
            delete buttonResizeLayer;
            delete buttonEditScrollBehavior;
            delete labelParallax;
            delete listParallaxLines;
            delete buttonEditParallaxBehavior;
        }

        void listViewLayers_onSelectedIndexChanged(void* sender, EventArgs* args) {
            int index = listViewLayers->SelectedIndex;
            if (index >= 0) {
                Editor->tilePlacementField->CurrentLayer = index;

                UpdateParallaxList();
            }

            Editor->tilePlacementField->UpdateRenderTarget = true;
        }
        void listParallaxLines_onSelectedIndexChanged(void* sender, EventArgs* args) {
            buttonEditParallaxBehavior->Enabled = (listParallaxLines->SelectedIndex >= 0);
        }
        void buttonResizeLayer_onMouseClick(void* sender, MouseEventArgs* args) {
            Layer* layer = &Editor->Layers[Editor->tilePlacementField->CurrentLayer];
            String* layerName = &Editor->LayerNames[Editor->tilePlacementField->CurrentLayer];

            Form_ResizeLayer* dialog = new Form_ResizeLayer("Resize & Rename Layer", layer, layerName);
            dialog->BackColor = BackColor;

            UI::System::Application::ShowDialog(dialog, [this, dialog](DialogResult result) -> void {
                if (result == DialogResult::OK) {
                    int width, height;
					char stringBuffer[32];
                    int layerIndex = Editor->tilePlacementField->CurrentLayer;

                    Strings::ToCString(stringBuffer, &dialog->numberBoxWidth->Text);
                    width = atoi(stringBuffer);

                    Strings::ToCString(stringBuffer, &dialog->numberBoxHeight->Text);
                    height = atoi(stringBuffer);

					Editor->LayerResize(layerIndex, width, height);
                    Editor->LayerRename(layerIndex, &dialog->textBoxName->Text);
                }
            });
        }
        void buttonEditScrollBehavior_onMouseClick(void* sender, MouseEventArgs* args) {
            Layer* layer = &Editor->Layers[Editor->tilePlacementField->CurrentLayer];

            Form_EditScrollBehavior* dialog = new Form_EditScrollBehavior("Edit Scroll Behavior");
            dialog->BackColor = BackColor;

            dialog->comboBoxBehavior->Select(layer->DrawBehavior);
            dialog->numericUpDownRelativeScroll->Value = layer->RelativeScroll.Full / 65536.0f;
            dialog->numericUpDownConstantScroll->Value = layer->ConstantScroll.Full / 65536.0f;
            dialog->comboBoxDrawGroups->Select(layer->DrawGroup[0]);

            UI::System::Application::ShowDialog(dialog, [this, dialog, layer](DialogResult result) -> void {
                if (result == DialogResult::OK) {
                    layer->DrawBehavior = dialog->comboBoxBehavior->SelectedIndex;
                    layer->RelativeScroll.Fract = dialog->numericUpDownRelativeScroll->Value * 0x10000;
                    layer->ConstantScroll.Fract = dialog->numericUpDownConstantScroll->Value * 0x10000;
                    layer->DrawGroup[0] = dialog->comboBoxDrawGroups->SelectedIndex;
                }
            });
        }
        void buttonEditParallaxBehavior_onMouseClick(void* sender, MouseEventArgs* args) {
            int index = listParallaxLines->SelectedIndex;
            if (index < 0)
                return;


            Parallax* parallax = NULL;
            Layer* layer = &Editor->Layers[Editor->tilePlacementField->CurrentLayer];
            int lineCount = (layer->DrawBehavior == 1 ? layer->Width : layer->Height) * TILE_SIZE;
            int sliceLen, sliceCount = 0, lastLine = 0, lastValue;
            if (lineCount > 0) {
                lastValue = layer->ParallaxIndexLines[0];
                for (int line = 0; line <= lineCount; line++) {
                    if (line == lineCount || lastValue != layer->ParallaxIndexLines[line]) {
                        // Do slice
                        sliceLen = line - lastLine;
                        if (sliceCount == index) {
                            parallax = &layer->ParallaxInfos[lastValue];
                            break;
                        }
                        sliceCount++;

                        // Iterate
                        if (line == lineCount)
                            break;
                        lastValue = layer->ParallaxIndexLines[line];
                        lastLine = line;
                    }
                }
            }

            if (parallax == NULL)
                return;

            Form_EditParallaxBehavior* dialog = new Form_EditParallaxBehavior("Edit Parallax Behavior", layer);
            dialog->BackColor = BackColor;

            dialog->numericUpDownStartPx->Value = lastLine;
            dialog->numericUpDownSizePx->Value = sliceLen;
            dialog->numericUpDownRelativeScroll->Value = parallax->RelativeParallax.Full / 65536.0f;
            dialog->numericUpDownConstantScroll->Value = parallax->ConstantParallax.Full / 65536.0f;
            dialog->checkBoxCanDeform->CheckState = parallax->CanDeform ? CheckState::Checked : CheckState::Unchecked;

            UI::System::Application::ShowDialog(dialog, [this, dialog, layer, parallax, index, lineCount](DialogResult result) -> void {
                if (result == DialogResult::OK) {
                    int startPx = dialog->numericUpDownStartPx->Value;
                    int sizePx = dialog->numericUpDownSizePx->Value;
                    int relativePrx = dialog->numericUpDownRelativeScroll->Value * 0x10000;
                    int constantPrx = dialog->numericUpDownConstantScroll->Value * 0x10000;
                    bool canDeform = dialog->checkBoxCanDeform->GetChecked();

                    int prxIndex = layer->ParallaxInfoCount;
                    for (int i = 0; i <= layer->ParallaxInfoCount; i++) {
                        // If the info doesn't exist,
                        if (i == layer->ParallaxInfoCount) {
                            // Add it, make sure prxIndex = i, and break;
                            Editor->LayerResizeParallaxInfoCount(Editor->tilePlacementField->CurrentLayer, i + 1);

                            Parallax* p = &layer->ParallaxInfos[i];
                            p->RelativeParallax.Full = relativePrx;
                            p->ConstantParallax.Full = constantPrx;
                            p->CanDeform = canDeform;

                            prxIndex = i;
                            break;
                        }

                        // otherwise, check if the info already exists
                        Parallax* p = &layer->ParallaxInfos[i];
                        if (p->RelativeParallax.Full == relativePrx &&
                            p->ConstantParallax.Full == constantPrx &&
                            p->CanDeform == canDeform) {
                            prxIndex = i;
                            break;
                        }
                    }

                    // set the lines to prxIndex
                    for (int i = startPx; i < startPx + sizePx; i++) {
                        layer->ParallaxIndexLines[i] = prxIndex;
                    }

                    // mark all prx infos as unused (except the one we just used)
                    for (int i = 0; i < layer->ParallaxInfoCount; i++) {
                        Parallax* p = &layer->ParallaxInfos[i];
                        p->ParallaxOffset.Full = i == prxIndex;
                    }

                    // check for any used prx infos
                    for (int i = 0; i < lineCount; i++) {
                        // Skip over ones we already know are used
                        if (i == startPx) {
                            i += sizePx - 1;
                            continue;
                        }
                        layer->ParallaxInfos[layer->ParallaxIndexLines[i]].ParallaxOffset.Full |= true;
                    }

                    // remove prx infos, keep track of changed indexes of USED prx infos, iterate from 0 -> end
                    Uint8 trimMap[256];
                    int trimmedCount = layer->ParallaxInfoCount;
                    int freeIndex = 0;
                    for (int i = 0; i < layer->ParallaxInfoCount; i++) {
                        Parallax* p = &layer->ParallaxInfos[i];
                        // if unused,
                        if (p->ParallaxOffset.Full == 0) {
                            // next index -> this index
                            trimMap[i] = 0xFF; // error checker
                            trimmedCount--;
                        }
                        else {
                            trimMap[i] = freeIndex;
                            layer->ParallaxInfos[freeIndex] = *p;
                            freeIndex++; // push forward the free index, since this spot is not free
                        }
                    }

                    // remap old line indexes to new ones
                    for (int i = 0; i < lineCount; i++) {
                        Uint8 oldIndex = layer->ParallaxIndexLines[i];
                        Uint8 newIndex = trimMap[oldIndex];
                        if (newIndex == 0xFF)
                            break;

                        layer->ParallaxIndexLines[i] = newIndex;
                    }

                    // kinda hacky but to reduce on array resizes just do this
                    layer->ParallaxInfoCount = trimmedCount;

                    UpdateParallaxList();
                }
            });
        }
        void toolStripButtonAddLayer_onMouseClick(void* sender, MouseEventArgs* args) {
            if (Editor->LayerCount + 1 > Editor->LayerCapacity)
                return;

			Form_ResizeLayer* dialog = new Form_ResizeLayer("Add New Layer", NULL, NULL);
            dialog->BackColor = BackColor;

            UI::System::Application::ShowDialog(dialog, [this, dialog](DialogResult result) -> void {
                if (result == DialogResult::OK) {
                    int width, height;
					char stringBuffer[32];

                    Strings::ToCString(stringBuffer, &dialog->numberBoxWidth->Text);
                    width = atoi(stringBuffer);

                    Strings::ToCString(stringBuffer, &dialog->numberBoxHeight->Text);
                    height = atoi(stringBuffer);

                    int layerIndex = Editor->LayerCount;
                    Editor->LayerNew(layerIndex);
                    Editor->LayerResize(layerIndex, width, height);
                    Editor->LayerRename(layerIndex, &dialog->textBoxName->Text);
                }
            });
        }
        void toolStripButtonRemoveLayer_onMouseClick(void* sender, MouseEventArgs* args) {
            if (Editor->LayerCount > 1) {
                Editor->LayerRemove(Editor->tilePlacementField->CurrentLayer, true);

                if (Editor->tilePlacementField->CurrentLayer >= Editor->LayerCount)
                    listViewLayers->Select(Editor->LayerCount - 1);
            }
        }
        void toolStripButtonDuplicateLayer_onMouseClick(void* sender, MouseEventArgs* args) {
            // If adding one layer breaks capacity, ignore this action
            if (Editor->LayerCount + 1 > Editor->LayerCapacity)
                return;

            int srcLayerIndex = Editor->tilePlacementField->CurrentLayer;
            int dstLayerIndex = srcLayerIndex + 1;
            {
                Editor->LayerShiftDown(dstLayerIndex, Editor->LayerCount - 1);
                Editor->LayerCopy(dstLayerIndex, srcLayerIndex);

                char srcLayerNameC[256];
                String* srcLayerName = &Editor->LayerNames[srcLayerIndex];
                String* dstLayerName = &Editor->LayerNames[dstLayerIndex];
                Strings::ToCString(srcLayerNameC, srcLayerName);

                int numericalSuffix = 1;
                int numberStartIndex = -1;
                for (int i = 0; i < srcLayerName->Length; i++) {
                    char ch = srcLayerNameC[i];
                    if (ch >= '0' && ch <= '9') {
                        if (numberStartIndex == -1) {
                            numberStartIndex = i;
                        }
                    }
                    else {
                        numberStartIndex = -1;
                    }
                }

                if (numberStartIndex >= 0) {
                    numericalSuffix = atoi(srcLayerNameC + numberStartIndex);
					sprintf(srcLayerNameC + numberStartIndex, "%d", numericalSuffix + 1);
                }
                else {
                    srcLayerNameC[srcLayerName->Length] = ' ';
					sprintf(srcLayerNameC + srcLayerName->Length + 1, "%d", numericalSuffix + 1);
                }

                Strings::FromCString(dstLayerName, srcLayerNameC, 0);
            }

            Editor->LayerCount++;

            UpdateList();

            listViewLayers->Select(dstLayerIndex);
        }
        void toolStripButtonMoveLayerUp_onMouseClick(void* sender, MouseEventArgs* args) {
            int currentLayer = Editor->tilePlacementField->CurrentLayer;
            if (currentLayer == 0)
                return;

            Editor->LayerSwap(currentLayer - 1, currentLayer);

            listViewLayers->Select(currentLayer - 1);

            UpdateList();
        }
        void toolStripButtonMoveLayerDown_onMouseClick(void* sender, MouseEventArgs* args) {
            int currentLayer = Editor->tilePlacementField->CurrentLayer;
            if (currentLayer == Editor->LayerCount - 1)
                return;

            Editor->LayerSwap(currentLayer + 1, currentLayer);

            listViewLayers->Select(currentLayer + 1);

            UpdateList();
        }

        void AddToParallaxList(Layer* layer, int start, int size, int index) {
            char stringBuffer[256];
            Parallax* parallax = &layer->ParallaxInfos[index];
            snprintf(stringBuffer, sizeof(stringBuffer) - 1,
                "Start: %d, Size: %d (Rel: %.3f, Const: %.3f%s)", start, size,
                parallax->RelativeParallax.Full / 65536.0, parallax->ConstantParallax.Full / 65536.0, parallax->CanDeform ? ", Deformable" : "");
            listParallaxLines->Items.Add(new ListViewItem(stringBuffer));
        }

        void UpdateList() {
            for (int i = 0; i < listViewLayers->Items.Count(); i++)
                delete listViewLayers->Items[i];

            listViewLayers->Items.Clear();

            for (int i = 0; i < Editor->LayerCount; i++)
                listViewLayers->Items.Add(new ListViewItem(&Editor->LayerNames[i]));

            listViewLayers->ResizeChildren();
        }
        void UpdateParallaxList() {
            if (Editor->Layers == NULL)
                return;
            if (Editor->LayerCount + 1 > Editor->LayerCapacity)
                return;
            if (Editor->tilePlacementField->CurrentLayer < 0)
                return;

            Layer* layer = &Editor->Layers[Editor->tilePlacementField->CurrentLayer];

            for (int i = 0; i < listParallaxLines->Items.Count(); i++)
                delete listParallaxLines->Items[i];

            listParallaxLines->Items.Clear();

            int lineCount = (layer->DrawBehavior == 1 ? layer->Width : layer->Height) * TILE_SIZE;
            if (lineCount > 0) {
                int sliceLen, sliceCount = 0, lastLine = 0, lastValue = layer->ParallaxIndexLines[0];
                for (int line = 0; line <= lineCount; line++) {
                    if (line == lineCount || lastValue != layer->ParallaxIndexLines[line]) {
                        // Do slice
                        sliceLen = line - lastLine;
                        AddToParallaxList(layer, lastLine, sliceLen, lastValue);
                        sliceCount++;

                        // Iterate
                        if (line == lineCount)
                            break;
                        lastValue = layer->ParallaxIndexLines[line];
                        lastLine = line;
                    }
                }
            }

            listParallaxLines->ResizeChildren();
        }
    };
    #pragma endregion

    // Global stuffs
    static const int  LayerCapacity = 32;

    Layer*      Layers = NULL;
    int         LayerCount = 0;
    String      LayerNames[LayerCapacity + 1] = { };
    Entity*     CurrentEntity = NULL;
    EntitySlot* EntitySlots = NULL;
    EntityEditorData* EntityEditorSlots = NULL;
    int         EntityCount = 0;
    int         EntityCapacity = 0;
    Uint16*     ClassIndexList = NULL;
    Uint32      ClassIndexCount = 0;
    Stage*      LinkedStage = NULL;
    char*       StampFilename = NULL;
    Color       BGColor1 = Color(0x444444, 0xFF);
    Color       BGColor2 = Color(0x333333, 0xFF);
    int         CurrentFilter = 3;

    ArrayList<SavedStamp*> Stamps;

    /// File IO functions

    // Reset the state for a new file
    void Init() {
        LayerCount = 0;
        for (int i = 0; i < LayerCapacity + 1; i++)
            Strings::Init(&LayerNames[i], 16);

        EntityCount = 0;
        EntityCapacity = MAX_SLOT_ENTITIES * 2;

        // Capacity + 1 (the temp layer used for re-ordering Layers)
        if (!Layers)
            Layers = (Layer*)calloc((LayerCapacity + 1), sizeof(Layer));
        else
            memset(Layers, 0, (LayerCapacity + 1) * sizeof(Layer));

        if (!EntitySlots)
            EntitySlots = (EntitySlot*)calloc(EntityCapacity, sizeof(EntitySlot));
        else
            memset(EntitySlots, 0, EntityCapacity * sizeof(EntitySlot));

        if (!EntityEditorSlots)
            EntityEditorSlots = (EntityEditorData*)calloc(EntityCapacity, sizeof(EntityEditorData));
        else
            memset(EntityEditorSlots, 0, EntityCapacity * sizeof(EntityEditorData));
        for (int i = 0; i < EntityCapacity; i++) {
            // NOTE: Start with a high capacity to prevent moving around the possible string references in memory
            EntityEditorSlots[i].Properties = new List<EntityProperty>(16);
        }

		// Link data
		LinkScene();

        StampCollectionClear();
    }

    // For creating a new scene file from scratch
    void New() {
        Init();

        LinkedStage = new Stage();

        LayerNew(0);
        tilePlacementField->CurrentLayer = 0;

        // Update UI
        objectClasses->UpdateClassList();

        Strings::FromCString(&FilePath, "CurrentScene.HSCN", 0);
        SetTitle("CurrentScene.HSCN");

        SetChangesSaved();
        JustCreated = true;
    }

    const Version HSCN_VERSION = { 0, 1, 1 };
    bool Read_RSDK(Stream* stream) {
        // Scene.BIN
        // Check loaded Stage list by filename for matching <path>/StageConfig.bin, if not found,
        // Load Stage at <path>/StageConfig.bin and add to loaded Stage list, if not found,
        // Prompt "StageConfig not found, create new StageConfig!", do it and link if yes, if no,
        // Give up. Do not load the scene.
        Uint32 objectDefinitionCount;
        char streamStringBuffer[256];

        // Signature checking
        if (stream->ReadUInt32() == 0x004E4353) {
            // Editor metadata
            // stream->Skip(16); // 16 bytes
            stream->ReadByte(); // ?
            BGColor1 = stream->ReadUInt32(); // Background Color 1
            BGColor2 = stream->ReadUInt32(); // Background Color 2
            stream->ReadByte(); // ?
            stream->ReadByte(); // ?
            stream->ReadByte(); // ?
            stream->ReadByte(); // ?
            stream->ReadByte(); // ?
            stream->ReadByte(); // ?
            stream->ReadByte(); // ?
            stream->ReadHeaderedString(streamStringBuffer); // Stamp library name
            stream->ReadByte(); // ???

            // Layer count
            LayerCount = stream->ReadByte();

            EntityCount = 0;
            EntityCapacity = MAX_SLOT_ENTITIES * 2;

            memset(Layers, 0, LayerCount * sizeof(Layer));
            memset(EntitySlots, 0, EntityCapacity * sizeof(EntitySlot));

            for (int layerIndex = 0; layerIndex < LayerCount; layerIndex++) {
                Layer* layer = &Layers[layerIndex];
                layer->Hidden[0] = true;
            }

            union RSDKTile {
                struct { Uint16 ID : 10; Uint16 FlipX : 1; Uint16 FlipY : 1; Uint16 PlaneA : 2; Uint16 PlaneB : 2; };
                Uint16 Full;

                RSDKTile() {
                    Full = 0;
                }
                RSDKTile(Uint32 tile) {
                    Full = (Uint16)tile;
                }
                operator Uint16() const { return Full; }
            };

            for (int i = 0; i < LayerCount; i++) {
                Layer* layer = &Layers[i];

                stream->ReadByte(); // Ignored Byte

                stream->ReadHeaderedString(streamStringBuffer);
                layer->Name = MD5_HashString(streamStringBuffer);

                Strings::FromCString(&LayerNames[i], streamStringBuffer, 0);

                layer->DrawBehavior = stream->ReadByte();
                if (layer->DrawBehavior == 3)
                    layer->DrawBehavior = 0;

                bool hidden = false;
                int drawGroup = stream->ReadByte();
                if (drawGroup & 0x10) {
                    drawGroup &= 0xF;
                    hidden = true;
                }
                for (int i = 0; i < MAX_VIEWPORTS; i++) {
                    layer->DrawGroup[i] = drawGroup;
                    layer->Hidden[i] = hidden;
                }

                layer->Width = stream->ReadUInt16();
                layer->Height = stream->ReadUInt16();

                layer->DataWidth = Math::ToNextPOT(layer->Width);
                layer->DataHeight = Math::ToNextPOT(layer->Height);
                layer->WidthInBits = Math::CountEmptyBits(layer->DataWidth);
                layer->HeightInBits = Math::CountEmptyBits(layer->DataHeight);

                Memory::Alloc((void**)&layer->Tiles, layer->DataWidth * layer->DataHeight * sizeof(Tile), Memory::MEMPOOL_STAGE, false);
                Memory::Alloc((void**)&layer->ParallaxIndexLines, ((layer->DataWidth > layer->DataHeight ? layer->DataWidth : layer->DataHeight) << TILE_SIZE_IN_BITS) * sizeof(Uint8), Memory::MEMPOOL_STAGE, false);

                layer->RelativeScroll.Full = stream->ReadInt16() << 8;
                layer->ConstantScroll.Full = stream->ReadInt16() << 8;

                layer->ParallaxInfoCount = stream->ReadUInt16();
                Memory::Alloc((void**)&layer->ParallaxInfos, layer->ParallaxInfoCount * sizeof(Parallax), Memory::MEMPOOL_STAGE, false);

                Parallax* info = layer->ParallaxInfos;
                for (int g = 0; g < layer->ParallaxInfoCount; g++) {
                    struct ParallaxDefinition {
                        Sint16 relative;
                        Sint16 constant;
                        Uint8 canDeform;
                        Uint8 unused;
                    } temp;

                    stream->ReadBytes(&temp, sizeof(temp));

                    info->RelativeParallax.Full = temp.relative << 8;
                    info->ConstantParallax.Full = temp.constant << 8;
                    info->ParallaxPosition.Full = info->ParallaxOffset.Full = 0;

                    info->CanDeform = temp.canDeform;
                    info++;
                }

                size_t compressedSize;
                RSDKTile* tileBoys;
                Memory::Alloc((void**)&tileBoys, sizeof(RSDKTile) * layer->DataWidth * layer->DataHeight, Memory::MEMPOOL_TEMP, false);

                compressedSize = M_MAX(layer->DataWidth, layer->DataHeight) << TILE_SIZE_IN_BITS;
                Uint32 scrollIndexRead = stream->ReadCompressed(layer->ParallaxIndexLines, compressedSize);
                if (scrollIndexRead > compressedSize) {
                    printf("Read more parallax indexes (%u) than buffer (%zu) allows!\n", scrollIndexRead, compressedSize);
                }

                compressedSize = sizeof(Tile) * layer->Width * layer->Height;
                Uint32 tileBoysRead = stream->ReadCompressed(tileBoys, compressedSize);
                if (tileBoysRead > compressedSize) {
                    printf("Read more tile data (%u) than buffer (%zu) allows!\n", tileBoysRead, compressedSize);
                }

                // Convert to HatchTiles
                Tile* tileRowDst = layer->Tiles;
                RSDKTile* tileRowSrc = tileBoys;
                for (Uint32 y = 0; y < layer->Height; y++) {
                    for (Uint32 x = 0; x < layer->Width; x++) {
                        auto dst = &tileRowDst[x];
                        auto src = &tileRowSrc[x];

                        if (*src == 0xFFFFU) {
                            *dst = TILE_EMPTY;
                            continue;
                        }

                        dst->ID = src->ID;
                        dst->FlipX = src->FlipX;
                        dst->FlipY = src->FlipY;
                        dst->PlaneA = src->PlaneA;
                        dst->PlaneB = src->PlaneB;
                    }
                    // memcpy(tileRowDst, tileRowSrc, layer->Width * sizeof(Tile));

                    tileRowDst += layer->DataWidth;
                    tileRowSrc += layer->Width;
                }
            }

            int variableTypes[64];
            size_t variableOffsets[64];
            bool variableFound[64];
            Hash variableNameHash[64];

            EntitySlot* EntitySlotsSpillover;
            Memory::Alloc((void**)&EntitySlotsSpillover, MAX_SLOT_ENTITIES * sizeof(EntitySlot), Memory::MEMPOOL_TEMP, true);

            // Entity definitions
            objectDefinitionCount = stream->ReadByte();
            for (Uint32 i = 0; i < objectDefinitionCount; i++) {
                // Read hash
                Hash classHash;
                classHash.A = stream->ReadUInt32();
                classHash.B = stream->ReadUInt32();
                classHash.C = stream->ReadUInt32();
                classHash.D = stream->ReadUInt32();

                // Find the class via the "classHash"
                int classIndex = -1;
                UsedClass* usedClass = NULL;
                Classes::LinkedClass* linkedClass = NULL;
                for (Uint32 c = 0; c < LinkedStage->Classes.size(); c++) {
                    UsedClass* checkClass = LinkedStage->Classes[c];
                    if (classHash == checkClass->NameHash) {
                        classIndex = c;
                        if (checkClass->LinkedClassIndex > -1)
                            linkedClass = Classes::LinkedClasses[checkClass->LinkedClassIndex];

                        usedClass = checkClass;
                        break;
                    }
                }

                // Serialization data
                int variableCount = stream->ReadByte();

                variableTypes[0] = 9;
                variableFound[0] = true;
                variableOffsets[0] = offsetof(Entity, Position);
                variableNameHash[0] = MD5_HashString("position");

                for (int a = 1; a < variableCount; a++) {
                    variableNameHash[a].A = stream->ReadUInt32();
                    variableNameHash[a].B = stream->ReadUInt32();
                    variableNameHash[a].C = stream->ReadUInt32();
                    variableNameHash[a].D = stream->ReadUInt32();

                    variableTypes[a] = stream->ReadByte();
                    variableFound[a] = false;
                    variableOffsets[a] = 0;

                    if (linkedClass) {
                        for (int attr = 0; attr < linkedClass->Properties.Count(); attr++) {
                            if (variableNameHash[a] == linkedClass->Properties[attr].Name) {
                                variableFound[a] = true;
                                variableOffsets[a] = linkedClass->Properties[attr].StructOffset;
                                break;
                            }
                        }
                    }
                }

                int entityCount = stream->ReadUInt16();
                for (int n = 0; n < entityCount; n++) {
                    int slotID = stream->ReadUInt16();

                    auto currentEntity = &EntitySlots[slotID];
                    auto currentMetadata = &EntityEditorSlots[slotID];

                    Uint8* entityBytePtr = (Uint8*)currentEntity;

                    currentEntity->Position.X.Full = stream->ReadInt32();
                    currentEntity->Position.Y.Full = stream->ReadInt32();
                    currentEntity->ClassID = classIndex;

                    for (int a = 1; a < variableCount; a++) {
                        EntityProperty property;
                        size_t offset = variableOffsets[a];
                        // Copy data over to property
                        property.NameHash = variableNameHash[a];
                        property.ValueType = variableTypes[a];
                        property.ValueData = calloc(1, 16);

                        switch (variableTypes[a]) {
                        case VAR_INT8:
                        case VAR_UINT8: *(Uint8*)(property.ValueData) = stream->ReadByte(); break;
                        case VAR_INT16:
                        case VAR_UINT16: *(Uint16*)(property.ValueData) = stream->ReadUInt16(); break;
                        case 10:
                        case VAR_BOOL:
                        case VAR_ENUM:
                        case VAR_COLOR:
                        case VAR_INT32:
                        case VAR_UINT32: *(Uint32*)(property.ValueData) = stream->ReadUInt32(); break;
                        case VAR_VECTOR2: ((Vector2*)(property.ValueData))->X = stream->ReadInt32(); ((Vector2*)(property.ValueData))->Y = stream->ReadInt32(); break;
                        case VAR_STRING:
                        {
                            Uint16 length = stream->ReadUInt16();

                            String* string = (String*)(property.ValueData);
                            Strings::Init(string, length);

                            string->Length = length;
                            for (size_t c = 0; c < length; c++)
                                string->Text[c] = stream->ReadInt16();
                            break;
                        }
                        }
                        currentMetadata->Properties->Add(property);
                    }

                    if (classIndex > -1) {
                        if (!currentEntity->Filter)
                            currentEntity->Filter = 0xFF;
                    }

                    EntityCount = M_MAX(EntityCount, slotID + 1);
                }
            }
        }
        else {
            Diagnostics::SetError("Invalid format for RSDK Scene!");
            return false;
        }

        return true;
    }
    bool Read_HatchTiled(Stream* stream) {
        // .TMX
        // Creates its own LinkedStage
        return false;
    }
    bool Read_HatchLite(Stream* stream) {
        char streamStringBuffer[256];
        const Uint32 MAGIC_HSCN = 0x4E435348;
        // .HSCN
        // Read String to get the resource path of the stage.
        // Check loaded Stage list by resource path, if not found,
        // Load Stage at resource path and add to loaded Stage list, if not found,
        // Prompt "StageInfo not found, create new StageInfo!", do it and link if yes, if no,
        // Give up. Do not load the scene.

        // Read magic
        if (stream->ReadUInt32() != MAGIC_HSCN) {
            return false;
        }

        // Read version
        Version version;
        version.major = stream->ReadByte();   // MAJOR version when you make incompatible API changes,
        version.minor = stream->ReadByte();   // MINOR version when you add functionality in a backwards compatible manner, and
        version.patch = stream->ReadUInt16(); // PATCH version when you make backwards compatible bug fixes.
        if (version.major != 0) {
            return false;
        }

        // Read settings
        BGColor1 = stream->ReadUInt32();
        BGColor2 = stream->ReadUInt32();
        if (version.patch >= 1) {
            tilePlacementField->CurrentLayer = stream->ReadByte();
        }

        // Read kit (asset group) resource paths
        int kitCount = stream->ReadByte();
        // Kits can contain:
        // Classes to use, along with their properties
        // Palettes to load
        // Resource paths of sound effects to load

        // Read layers
        LayerCount = stream->ReadByte();
        memset(Layers, 0, LayerCount * sizeof(Layer));

        for (int i = 0; i < LayerCount; i++) {
            Layer* layer = &Layers[i];

            // Read name
            stream->ReadHeaderedString(streamStringBuffer);
            Strings::FromCString(&LayerNames[i], streamStringBuffer, 0);
            layer->Name = MD5_HashString(streamStringBuffer);

            layer->DrawBehavior = stream->ReadByte();

            bool hidden = false;
            int drawGroup = stream->ReadByte();
            if (drawGroup & 0x10) {
                drawGroup &= 0xF;
                hidden = true;
            }
            for (int i = 0; i < MAX_VIEWPORTS; i++) {
                layer->DrawGroup[i] = drawGroup;
                layer->Hidden[i] = hidden;
            }

            layer->Width = stream->ReadUInt16();
            layer->Height = stream->ReadUInt16();

            layer->DataWidth = Math::ToNextPOT(layer->Width);
            layer->DataHeight = Math::ToNextPOT(layer->Height);
            layer->WidthInBits = Math::CountEmptyBits(layer->DataWidth);
            layer->HeightInBits = Math::CountEmptyBits(layer->DataHeight);

            Memory::Alloc(&layer->Tiles, layer->DataWidth * layer->DataHeight * sizeof(Tile), Memory::MEMPOOL_STAGE, true);

            Memory::Alloc(&layer->ParallaxIndexLines, ((layer->DataWidth > layer->DataHeight ? layer->DataWidth : layer->DataHeight) << TILE_SIZE_IN_BITS) * sizeof(Uint8), Memory::MEMPOOL_STAGE, false);

            layer->RelativeScroll.Full = stream->ReadInt16() << 8;
            layer->ConstantScroll.Full = stream->ReadInt16() << 8;

            layer->ParallaxInfoCount = stream->ReadUInt16();
            Memory::Alloc(&layer->ParallaxInfos, layer->ParallaxInfoCount * sizeof(Parallax), Memory::MEMPOOL_STAGE, false);

            Parallax* info = layer->ParallaxInfos;
            for (int g = 0; g < layer->ParallaxInfoCount; g++) {
                struct ParallaxDefinition {
                    Sint16 relative;
                    Sint16 constant;
                    Uint8 canDeform;
                    Uint8 unused;
                } temp;

                stream->ReadBytes(&temp, sizeof(temp));

                info->RelativeParallax.Full = temp.relative << 8;
                info->ConstantParallax.Full = temp.constant << 8;
                info->ParallaxPosition.Full = info->ParallaxOffset.Full = 0;

                info->CanDeform = temp.canDeform;
                info++;
            }

            size_t uncompressedSize;

            uncompressedSize = M_MAX(layer->DataWidth, layer->DataHeight) << TILE_SIZE_IN_BITS;
            Uint32 scrollIndexRead = stream->ReadCompressed(layer->ParallaxIndexLines, uncompressedSize);
            if (scrollIndexRead > uncompressedSize) {
                printf("Read more parallax indexes (%u) than buffer (%zu) allows!\n", scrollIndexRead, uncompressedSize);
            }

            uncompressedSize = sizeof(Tile) * layer->DataWidth * layer->DataHeight;
            Uint32 tileBoysRead = stream->ReadCompressed(layer->Tiles, uncompressedSize);
            if (tileBoysRead > uncompressedSize) {
                printf("Read more tile data (%u) than buffer (%zu) allows!\n", tileBoysRead, uncompressedSize);
            }
        }

        // NOTE:
        // Properties should be able to be defined in the editor
        // Classes used for one scene should be able to be copied from one scene to another

        // NOTE:
        // In-engine, if a class isn't added to a kit, it will not have it's
        // entities loaded into the game

        EntityCount = 0;
        EntityCapacity = MAX_SLOT_ENTITIES * 2;
        memset(EntitySlots, 0, EntityCapacity * sizeof(EntitySlot));

        // Read classes & their properties (this can be defined both here, and in a kit)
        int classCount = stream->ReadUInt16();
        for (int i = 0; i < classCount; i++) {
            stream->ReadHeaderedString(streamStringBuffer); // Class Name

            LinkedStage->AddClassByName(streamStringBuffer);

            UsedClass* usedClass = LinkedStage->Classes.back();

            int propertyCount = stream->ReadByte();
            for (int p = 0; p < propertyCount; p++) {
                stream->ReadHeaderedString(streamStringBuffer); // propertyName

                usedClass->Properties.Add(Classes::ClassAttribute { });
                Classes::ClassAttribute* newProperty = new (&usedClass->Properties.Items[usedClass->Properties.Count() - 1]) Classes::ClassAttribute(streamStringBuffer);
                newProperty->AttributeType = stream->ReadByte();
            }
        }

        // Read entities
        int entityCount = stream->ReadUInt16();
        for (int i = 0; i < entityCount; i++) {
            auto entity = &EntitySlots[EntityCount];
            auto entityEd = &EntityEditorSlots[EntityCount];

            Hash classHash;
            classHash.A = stream->ReadUInt32(); // Class Name Hash
            classHash.B = stream->ReadUInt32(); // Class Name Hash
            classHash.C = stream->ReadUInt32(); // Class Name Hash
            classHash.D = stream->ReadUInt32(); // Class Name Hash
            entity->ClassID = LinkedStage->GetClass(classHash);

            entity->Position.X = stream->ReadUInt32();
            entity->Position.Y = stream->ReadUInt32();
            entity->Filter = stream->ReadByte(); // Filter

            entityEd->Properties = new List<EntityProperty>();

            int propertyCount = stream->ReadByte();
            for (int p = 0; p < propertyCount; p++) {
                EntityProperty property;
                property.ValueData = calloc(1, 16);

                property.NameHash.A = stream->ReadUInt32(); // propertyNameHash
                property.NameHash.B = stream->ReadUInt32(); // propertyNameHash
                property.NameHash.C = stream->ReadUInt32(); // propertyNameHash
                property.NameHash.D = stream->ReadUInt32(); // propertyNameHash

                property.ValueType = stream->ReadByte();

                switch (property.ValueType) {
                case VAR_INT8:
                case VAR_UINT8:
                    stream->ReadBytes(property.ValueData, 1);
                    break;
                case VAR_INT16:
                case VAR_UINT16:
                    stream->ReadBytes(property.ValueData, 2);
                    break;
                case VAR_ENUM:
                case VAR_BOOL:
                case VAR_COLOR:
                case VAR_INT32:
                case VAR_UINT32:
                    stream->ReadBytes(property.ValueData, 4);
                    break;
                case VAR_VECTOR2:
                    stream->ReadBytes(property.ValueData, 8);
                    break;
                case VAR_STRING:
                    String* string = (String*)property.ValueData;
                    Uint16 length = stream->ReadUInt16();

                    Strings::Init(string, length);
                    for (int i = 0; i < length; i++)
                        string->Text[i] = stream->ReadInt16();
                    string->Length = length;
                    break;
                }

                entityEd->Properties->Add(property);
            }

            EntityCount++;
        }

        return true;
    }
    bool Write_HatchLite(Stream* stream) {
        char streamStringBuffer[256];
        const Uint32 MAGIC_HSCN = 0x4E435348;
        // .HSCN

        // Read magic
        stream->WriteUInt32(MAGIC_HSCN);

        // Read version
        stream->WriteByte(HSCN_VERSION.major);   // MAJOR version when you make incompatible API changes,
        stream->WriteByte(HSCN_VERSION.minor);   // MINOR version when you add functionality in a backwards compatible manner, and
        stream->WriteUInt16(HSCN_VERSION.patch); // PATCH version when you make backwards compatible bug fixes.

        // Write settings
        stream->WriteUInt32(BGColor1);
        stream->WriteUInt32(BGColor2);
        stream->WriteByte(tilePlacementField->CurrentLayer);

        // Write kit (asset group) resource paths
        stream->WriteByte(0);
        // Kits can contain:
        // Classes to use, along with their properties
        // Palettes to load
        // Resource paths of sound effects to load

        // Write layers
        stream->WriteByte(LayerCount);
        for (int i = 0; i < LayerCount; i++) {
            Layer* layer = &Layers[i];

            // Read name
            Strings::ToCString(streamStringBuffer, &LayerNames[i]);
            stream->WriteHeaderedString(streamStringBuffer);

            stream->WriteByte(layer->DrawBehavior);

            stream->WriteByte(layer->DrawGroup[0] | (layer->Hidden[0] << 4));

            stream->WriteUInt16(layer->Width);
            stream->WriteUInt16(layer->Height);

            stream->WriteInt16(layer->RelativeScroll.Full >> 8);
            stream->WriteInt16(layer->ConstantScroll.Full >> 8);

            stream->WriteUInt16(layer->ParallaxInfoCount);

            Parallax* info = layer->ParallaxInfos;
            for (int g = 0; g < layer->ParallaxInfoCount; g++) {
                struct ParallaxDefinition {
                    Sint16 relative;
                    Sint16 constant;
                    Uint8 canDeform;
                    Uint8 unused;
                } temp;

                temp.relative = info->RelativeParallax.Full >> 8;
                temp.constant = info->ConstantParallax.Full >> 8;
                temp.canDeform = info->CanDeform;
                temp.unused = 0;

                stream->WriteBytes(&temp, sizeof(temp));
                info++;
            }

            size_t compressedSize, uncompressedSize;

            uncompressedSize = M_MAX(layer->DataWidth, layer->DataHeight) << TILE_SIZE_IN_BITS;
            compressedSize = stream->WriteCompressed(layer->ParallaxIndexLines, uncompressedSize);

            uncompressedSize = sizeof(Tile) * layer->DataWidth * layer->DataHeight;
            compressedSize = stream->WriteCompressed(layer->Tiles, uncompressedSize);
        }

        // NOTE:
        // Properties should be able to be defined in the editor
        // Classes used for one scene should be able to be copied from one scene to another

        // NOTE:
        // In-engine, if a class isn't added to a kit, it will not have it's
        // entities loaded into the game

        // a Class can define it's own properties in addition to the LinkedClass' properties
        //    If a conflict occurs between a user-defined and DLL-defined property,
        //    hide the user-defined one and route to the DLL-defined
        // a Entity can define it's own property values


        // Write classes & their properties (this can be defined both here, and in a kit)
        int classCount = LinkedStage->Classes.size();
        stream->WriteUInt16(classCount);
        for (int i = 0; i < classCount; i++) {
            UsedClass* usedClass = LinkedStage->Classes[i];
            stream->WriteHeaderedString(usedClass->Name);

            int propertyCount = usedClass->Properties.Count();
            stream->WriteByte(propertyCount);
            for (int p = 0; p < propertyCount; p++) {
                auto property = &usedClass->Properties[p];
                stream->WriteHeaderedString(property->NameString);
                stream->WriteByte(property->AttributeType);
            }
        }

        // Write entities
        stream->WriteUInt16(EntityCount);
        for (int i = 0; i < EntityCount; i++) {
            Entity* entity = &EntitySlots[i];
            EntityEditorData* entityEd = &EntityEditorSlots[i];

            if (entity->ClassID <= 0) {
                stream->WriteUInt32(0x19191919); // Class Name Hash
                stream->WriteUInt32(0x29292929); // Class Name Hash
                stream->WriteUInt32(0x39393939); // Class Name Hash
                stream->WriteUInt32(0x49494949); // Class Name Hash
            }
            else {
                UsedClass* usedClass = LinkedStage->Classes[entity->ClassID];
                stream->WriteUInt32(usedClass->NameHash.A); // Class Name Hash
                stream->WriteUInt32(usedClass->NameHash.B); // Class Name Hash
                stream->WriteUInt32(usedClass->NameHash.C); // Class Name Hash
                stream->WriteUInt32(usedClass->NameHash.D); // Class Name Hash
            }

            stream->WriteUInt32(entity->Position.X);
            stream->WriteUInt32(entity->Position.Y);
            stream->WriteByte(entity->Filter); // Filter

            int propertyCount = entityEd->Properties->Count();
            stream->WriteByte(propertyCount);
            for (int p = 0; p < propertyCount; p++) {
                auto prop = &entityEd->Properties->Items[p];
                stream->WriteUInt32(prop->NameHash.A); // propertyNameHash
                stream->WriteUInt32(prop->NameHash.B); // propertyNameHash
                stream->WriteUInt32(prop->NameHash.C); // propertyNameHash
                stream->WriteUInt32(prop->NameHash.D); // propertyNameHash

                stream->WriteByte(prop->ValueType);

                switch (prop->ValueType) {
                case VAR_INT8:
                case VAR_UINT8:
                    stream->WriteBytes(prop->ValueData, 1);
                    break;
                case VAR_INT16:
                case VAR_UINT16:
                    stream->WriteBytes(prop->ValueData, 2);
                    break;
                case VAR_ENUM:
                case VAR_BOOL:
                case VAR_COLOR:
                case VAR_INT32:
                case VAR_UINT32:
                    stream->WriteBytes(prop->ValueData, 4);
                    break;
                case VAR_VECTOR2:
                    stream->WriteBytes(prop->ValueData, 8);
                    break;
                case VAR_STRING:
                    String* string = (String*)prop->ValueData;
                    stream->WriteUInt16(string->Length);
                    for (int i = 0; i < string->Length; i++)
                        stream->WriteUInt16(string->Text[i]);
                    break;
                }
            }
        }
		return true;
    }

    bool Open() {
        char filename[256];
        char stringBuffer[256];
        Strings::ToCString(filename, &FilePath);

        Init();

        enum class LoadType {
            Unknown,
            RSDKv5,
            Tiled,
            HatchLite,
        } loadType;

        if (stristr(filename, ".tmx") != NULL)
            loadType = LoadType::Tiled;
        else if (stristr(filename, ".hscn") != NULL)
            loadType = LoadType::HatchLite;
        else if (strstr(filename, ".bin") != NULL && strstr(filename, "Scene") != NULL)
            loadType = LoadType::RSDKv5;
        else
            loadType = LoadType::Unknown;


        // If RSDK
        if (loadType == LoadType::RSDKv5) {
            LinkedStage = new Stage();

            // Should load GameConfig before StageConfig

            // Load Stage info
            if (!LinkedStage->LoadConfig(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "StageConfig.bin"))) {
                fprintf(stderr, "LoadConfig failed with reason: %s\n", Diagnostics::ErrorString);
                return false;
            }

            // Load tile collisions & info
            if (!LinkedStage->OpenTileConfig(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "TileConfig.bin"))) {
                fprintf(stderr, "OpenTileConfig failed with reason: %s\n", Diagnostics::ErrorString);
                return false;
            }

            // Load tile image data & hashes
            if (!LinkedStage->LoadTileset_RSDK(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "16x16Tiles.gif"))) {
                fprintf(stderr, "LoadTileset_RSDK failed with reason: %s\n", Diagnostics::ErrorString);
                return false;
            }

            // Load layer data
            Stream* stream = FileStream::New(filename, FileStream::READ_ACCESS);
            if (stream) {
                if (!Read_RSDK(stream)) {
                    fprintf(stderr, "Read_RSDK failed with reason: %s\n", Diagnostics::ErrorString);
                    return false;
                }
                stream->Close();
            }
            else {
                fprintf(stderr, "Read_RSDK failed with reason: %s\n", Diagnostics::ErrorString);
                return false;
            }
        }
        // If Hatch1 & Tiled
        else if (loadType == LoadType::Tiled) {
            LinkedStage = new Stage();
        }
        // If HatchLite
        else if (loadType == LoadType::HatchLite) {
            // If cannot find LinkedStage in memory, make new LinkedStage
            LinkedStage = new Stage();

            // Load tile collisions & info "TileInfo.HCOL"
            if (!LinkedStage->OpenTileConfig(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "TileCol.bin"))) {
                fprintf(stderr, "OpenTileConfig failed with reason: %s\n", Diagnostics::ErrorString);
                return false;
            }

            // Load tile image data & hashes
            TilesetOpen(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "Tileset.png"));
            /*if (!LinkedStage->LoadTileset_HatchLite(GetSiblingFilePath(stringBuffer, filename, "Tileset.htil"))) {
                fprintf(stderr, "LoadTileset_HatchLite failed with reason: %s\n", Diagnostics::ErrorString);
                return false;
            }*/


            StampCollectionOpen(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "Stamps.HSTM"));

            // Load layer data
            Stream* stream = FileStream::New(filename, FileStream::READ_ACCESS);
            if (stream) {
                if (!Read_HatchLite(stream)) {
                    fprintf(stderr, "Read_HatchLite failed with reason: %s\n", Diagnostics::ErrorString);
                    return false;
                }
                stream->Close();
            }
            else {
                fprintf(stderr, "Read_HatchLite failed with reason: %s\n", Diagnostics::ErrorString);
                return false;
            }
        }
        // Otherwise, it's unknown,
        else {
            Diagnostics::SetError("Unknown or invalid Scene format.");
            return false;
        }

        // Copy colors from stage and global palettes
        for (int p = 0; p < MAX_PALETTE_COUNT; p++) {
            for (int paletteLine = 0; paletteLine < 16; paletteLine++) {
                int row = (paletteLine << 4);
                if ((LinkedStage->StageConfigPalette.UsedLines[p] & (1 << paletteLine)) != 0) {
                    for (int c = 0; c < 16; c++) {
                        Graphics::Palette[p][row + c] = LinkedStage->StageConfigPalette.Palettes[p][row + c];
                    }
                }
                // else if ((LinkedStage->UsedGameConfigPaletteLines[p] & (1 << paletteLine)) != 0) {
                //     Graphics::Palette[p][row] = LinkedStage->GameConfigPalette[p][row];
                // }
            }
        }

        // Ensure all classes are linked that can be linked
        LinkedStage->LinkAllUsedClasses();

        GameLinker::State.IsEditor = true;

        // Link any entity metadata that has not been already linked
        for (int i = 0; i < EntityCount; i++) {
            auto entity = &EntitySlots[i];
            auto metadata = &EntityEditorSlots[i];

            for (int p = 0; p < metadata->Properties->Count(); p++) {
                auto property = metadata->Properties->Items[p];
                auto classProp = LinkedStage->GetPropertyDefinitionByHash(entity->ClassID, property.NameHash);
                if (classProp != NULL && classProp->StructOffset != 0) {

                    // If the property is an OPTION type but gives no options, it's just an int32
                    if (property.ValueType == VAR_ENUM && classProp->EnumPairs.Count() == 0) {
                        property.ValueType = VAR_INT32;
                        classProp->AttributeType = VAR_INT32;
                        // NOTE: This will change the type in the LinkedStage, but will have undefined behavior for
                        // saving/loading scenes!
                    }

                    switch (property.ValueType) {
                    case VAR_INT8:
                    case VAR_UINT8:
                        memcpy((Uint8*)entity + classProp->StructOffset, property.ValueData, 1);
                        break;
                    case VAR_INT16:
                    case VAR_UINT16:
                        memcpy((Uint8*)entity + classProp->StructOffset, property.ValueData, 2);
                        break;
                    case VAR_ENUM:
                    case VAR_BOOL:
                    case VAR_COLOR:
                    case VAR_INT32:
                    case VAR_UINT32:
                        memcpy((Uint8*)entity + classProp->StructOffset, property.ValueData, 4);
                        break;
                    case VAR_VECTOR2:
                        memcpy((Uint8*)entity + classProp->StructOffset, property.ValueData, sizeof(Vector2));
                        break;
                    case VAR_STRING:
                        // NOTE: String-type is not compatible with the Live Entity system
                        break;
                    }
                }
            }
        }

        // Update UI
        layerControls->UpdateList();
        objectClasses->UpdateClassList();
        entityProperties->UpdateEntityList();

        if (LayerCount > 0) {
            if (tilePlacementField->CurrentLayer >= 0)
                layerControls->listViewLayers->Select(tilePlacementField->CurrentLayer);
            else
                layerControls->listViewLayers->Select(0);
        }

        return true;
    }
    bool Save() {
        char filename[256];
        char stringBuffer[256];
        Strings::ToCString(filename, &FilePath);

        Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
        if (stream) {
            if (!Write_HatchLite(stream)) {
                fprintf(stderr, "Write_HatchLite failed with reason: %s\n", Diagnostics::ErrorString);
                return false;
            }
            stream->Close();
        }
        else {
            fprintf(stderr, "Write_HatchLite failed with reason: %s\n", Diagnostics::ErrorString);
            return false;
        }

        LinkedStage->SaveTileConfig(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "TileCol.bin"));
        StampCollectionSave(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "Stamps.HSTM"));
        TilesetSave(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "Tileset.png"));

        SetChangesSaved();
        JustCreated = false;
        return true;
    }

    int GetEditorType() {
        return EditorTypes::SCENE;
    }

    bool PromptImportTileset() {
        UI::SystemDialog::OpenFileData ofd;
        ofd.Title = "Open Tileset Image Files...";
        // ofd.InitialDirectory = ProjectDirectory;
        ofd.FilterPatterns.Add("*.gif");
        ofd.FilterPatterns.Add("*.png");
        ofd.FilterPatterns.Add("*.htil");
        ofd.Multiselect = true;

        if (UI::SystemDialog::OpenFile(&ofd)) {
            StampCollectionClear();

            if (TilesetImport(ofd.Filenames))
                return true;
        }
        return false;
    }

    bool TilesetImport(List<char*>& filenames) {
        int maxTileCount = TILE_IDENT_MASK + 1;

        const int MAX_SHEET_HEIGHT = 1024;
        const int MAX_TILE_PIXELS = 1024 * 1024;

        const int dstColumnCount = 64;
        const int dstColumnMask = 63;
        const int dstColumnBitshift = 6;

        int tileset_w = 1;
        int tileset_h = 1;
        int tileset_comp;

		Uint32* tileSrc;
		Uint32* tileDst;

        Uint32* newTilesetImageData = (Uint32*)calloc(1024 * 1024 * 4, sizeof(Uint32));
        if (!newTilesetImageData) {
            Diagnostics::SetError("Could not allocate space for tileset image data.");
            return false;
        }

        Stage::TileImageHash* oldTileHashes = (Stage::TileImageHash*)malloc(sizeof(LinkedStage->TileHashes));
        if (!oldTileHashes) {
            Diagnostics::SetError("Could not allocate space for old tileset hash data.");
            return false;
        }

        memcpy(oldTileHashes, LinkedStage->TileHashes, sizeof(LinkedStage->TileHashes));
        memset(LinkedStage->TileHashes, 0x00, sizeof(LinkedStage->TileHashes));

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

            if (!tileArray || srcRowCount == 0 || srcColumnCount == 0)
                return false;

            tileArray = (Tile*)realloc(tileArray, srcRowCount * srcColumnCount * sizeof(Tile));
            if (!tileArray) {
                Diagnostics::SetError("Could not allocate space for stamp tile data.");
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
                        if (LinkedStage->TileHashes[t].FLIP_NONE == srcHash) {
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
                    LinkedStage->TileHashes[tile].FLIP_NONE = srcHash;
                    tile++;
                NextSourceTile:
                    tind++;
                }
            }

            // Create stamp
            char filenameBuffer[256];
            const char* STAMP_FILENAME_PREFIX = "Stamp_";
            if (strncmp(STAMP_FILENAME_PREFIX, UI::Filesystem::Paths::GetFilenameWithoutExtension(filenameBuffer, filenames[i]), strlen(STAMP_FILENAME_PREFIX)) == 0) {
                StampCollectionAdd(filenameBuffer + strlen(STAMP_FILENAME_PREFIX),
                    Stamp::FromTileArray(tileArray, srcColumnCount, srcRowCount));
            }

            stbi_image_free(tileset_imagedata);
        }

        TotalTiles:
		if (tile == 0)
			goto FreeMemoryAndFail;

        LinkedStage->TileCount = tile;

        // Flip tiles horizontally
        tileSrc = &newTilesetImageData[0];
        tileDst = &newTilesetImageData[MAX_TILE_PIXELS];
        for (int line = 0; line < 0x1000 * TILE_SIZE; line++) {
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
        tileSrc = &newTilesetImageData[0];
        tileDst = &newTilesetImageData[MAX_TILE_PIXELS << 1];
        for (int tileRow = 0; tileRow < MAX_SHEET_HEIGHT / TILE_SIZE; ) {
            for (int row = 0, ySrc = 0, yDst = dstColumnCount * TILE_SIZE * (TILE_SIZE - 1); row < TILE_SIZE; row++) {
                // Copy tile line
                memcpy(&tileDst[yDst], &tileSrc[ySrc], dstColumnCount * TILE_SIZE * sizeof(Uint32));
                ySrc += dstColumnCount * TILE_SIZE;
                yDst -= dstColumnCount * TILE_SIZE;
            }

            tileSrc += dstColumnCount * TILE_SIZE * TILE_SIZE;
            tileDst += dstColumnCount * TILE_SIZE * TILE_SIZE;
            tileRow++;
        }

        // Flip tiles horizontally & vertically
        tileSrc = &newTilesetImageData[MAX_TILE_PIXELS << 1];
        tileDst = &newTilesetImageData[MAX_TILE_PIXELS << 1 | MAX_TILE_PIXELS];
        for (int line = 0; line < 0x1000 * TILE_SIZE; line++) {
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

        // Update tile image data
        for (int f = 0; f < 4; f++) {
            Studio::Textures::CreateTextureFromSTBI(&LinkedStage->TileImageTextures[f], (Uint8*)&newTilesetImageData[f * MAX_TILE_PIXELS], 1024, 1024);
        }

        // Create the tile remapping array
        for (int oldID = 0; oldID < 0x1000; oldID++) {
            // Set conversion value to default of "no-conversion"
            LinkedStage->TileRemapArray[oldID] = -1;
            // Set conversion value to pass-through
            LinkedStage->TileRemapArray[oldID] = oldID;

            // Match for any tiles from old to new,
            // and if new tile is an old one, set the conversion value to the new ID.
            for (int newID = 0; newID < LinkedStage->TileCount; newID++) {
                if (oldTileHashes[oldID].FLIP_NONE != 0 &&
                    oldTileHashes[oldID].FLIP_NONE == LinkedStage->TileHashes[newID].FLIP_NONE) {
                    LinkedStage->TileRemapArray[oldID] = newID;
                    break;
                }
            }
        }

    FreeMemoryAndSucceed:
        if (LinkedStage->TileImagePixelData)
            free(LinkedStage->TileImagePixelData);
        LinkedStage->TileImagePixelData = newTilesetImageData;

        free(oldTileHashes);
        free(tileArray);

        // Update tileSelector
        tileSelector->ResizeChildren();

        return true;

    FreeMemoryAndFail:
        LinkedStage->TileImagePixelData = NULL;

        free(newTilesetImageData);
        free(oldTileHashes);
        free(tileArray);

        return false;
    }
    bool TilesetOpen(CString filename) {
        List<char*> filenames;
        filenames.Add((char*)filename);
        TilesetImport(filenames);
        return true;
    }
    bool TilesetSave(CString filename) {
        int pitch;
        Uint32* pixels;
        SDL_Texture* texture = LinkedStage->TileImageTextures[0];

        stbi_write_png(filename, 1024, 1024, 4, LinkedStage->TileImagePixelData, 1024 * 4);
        return true;
    }

    // Data Functions
    void LayerNew(int layerIndex) {
        Layer* layer = &Layers[layerIndex];
        const int defaultWidth = 64;
        const int defaultHeight = 64;

        layer->DrawBehavior = 0;
        layer->DrawGroup[0] = 0;
        layer->Hidden[0] = false;

        // Name the layer
        char nameBuffer[128];
        snprintf(nameBuffer, 128, "Layer %d", LayerCount);
        LayerRename(layerIndex, nameBuffer);

        // Initialize tile data & parallax lines
        LayerResize(layerIndex, defaultWidth, defaultHeight);

        // Initialize scroll values
        layer->RelativeScroll.Full = 0x10000;
        layer->ConstantScroll.Full = 0x00000;

        // Initialize parallax values
        LayerResizeParallaxInfoCount(layerIndex, 1);
        for (int i = 0; i < defaultHeight * TILE_SIZE; i++) {
            layer->ParallaxIndexLines[i] = 0;
        }

        // Add layer to count
        LayerCount = M_MAX(LayerCount, layerIndex + 1);


        // Update UI List
        layerControls->UpdateList();
    }
    void LayerRemove(int layerIndex, bool shift) {
        Layer* layer = &Layers[layerIndex];
        String* layerName = &LayerNames[layerIndex];

        memset(layer, 0, sizeof(Layer));

        if (shift) {
            for (int i = layerIndex; i < LayerCount - 1; i++) {
                LayerCopy(i, i + 1);
            }
        }

        LayerCount--;

        layerControls->UpdateList();
    }
    void LayerShiftDown(int startLayerIndex, int endLayerIndex) {
        if (endLayerIndex > (LayerCapacity - 1) - 1)
            endLayerIndex = (LayerCapacity - 1) - 1;

        if (startLayerIndex > endLayerIndex)
            return;

        for (int i = endLayerIndex; i >= startLayerIndex; i--) {
            LayerCopy(i + 1, i);
        }

        Layer* layer = &Layers[startLayerIndex];
        String* layerName = &LayerNames[startLayerIndex];

        memset(layer, 0, sizeof(Layer));

        layerControls->UpdateList();
    }
    void LayerCopy(int dstIndex, int srcIndex) {
        Layer* dstLayer = &Layers[dstIndex];
        Layer* srcLayer = &Layers[srcIndex];

        dstLayer->DrawBehavior = srcLayer->DrawBehavior;
        dstLayer->DrawGroup[0] = srcLayer->DrawGroup[0];
        dstLayer->Hidden[0] = srcLayer->Hidden[0];

        // Copy the layer name
        LayerRename(dstIndex, &LayerNames[srcIndex]);

        // Copy tile data & parallax lines
        LayerResize(dstIndex, srcLayer->Width, srcLayer->Height);
        memcpy(dstLayer->Tiles, srcLayer->Tiles, srcLayer->DataWidth * srcLayer->DataHeight * sizeof(Tile));
        memcpy(dstLayer->ParallaxIndexLines, srcLayer->ParallaxIndexLines, (M_MAX(srcLayer->DataWidth, srcLayer->DataHeight) << TILE_SIZE_IN_BITS) * sizeof(Uint8));

        // Copy scroll values
        dstLayer->RelativeScroll.Full = srcLayer->RelativeScroll.Full;
        dstLayer->ConstantScroll.Full = srcLayer->ConstantScroll.Full;

        // Copy parallax values
        LayerResizeParallaxInfoCount(dstIndex, srcLayer->ParallaxInfoCount);
        memcpy(dstLayer->ParallaxInfos, srcLayer->ParallaxInfos, srcLayer->ParallaxInfoCount * sizeof(Parallax));
    }
    void LayerSwap(int dstIndex, int srcIndex) {
        LayerCopy(LayerCapacity, srcIndex);
        LayerCopy(srcIndex, dstIndex);
        LayerCopy(dstIndex, LayerCapacity);
    }
    void LayerRename(int layerIndex, CString name) {
        Layer* layer = &Layers[layerIndex];

        Strings::FromCString(&LayerNames[layerIndex], name, 0);
        layer->Name = MD5_HashString(name);

        layerControls->UpdateList();
    }
    void LayerRename(int layerIndex, String* name) {
        char nameBuffer[128];
        Strings::ToCString(nameBuffer, name);

        Layer* layer = &Layers[layerIndex];

        Strings::FromCString(&LayerNames[layerIndex], nameBuffer, 0);
        layer->Name = MD5_HashString(nameBuffer);



        layerControls->UpdateList();
    }
    void LayerResize(int layerIndex, int width, int height) {
        Layer* layer = &Layers[layerIndex];

        auto old_Width = layer->Width;
        auto old_Height = layer->Height;
        auto old_DataWidth = layer->DataWidth;
        auto old_DataHeight = layer->DataHeight;
        auto old_WidthInBits = layer->WidthInBits;
        auto old_HeightInBits = layer->HeightInBits;
        auto old_Tiles = layer->Tiles;
        auto old_ParallaxIndexLines = layer->ParallaxIndexLines;

        layer->Width = width;
        layer->Height = height;

        // If this is first time resizing layer,
        if (layer->Tiles == NULL) {
            // Set data size in tiles and bits
            layer->DataWidth = Math::ToNextPOT(layer->Width);
            layer->DataHeight = Math::ToNextPOT(layer->Height);
            layer->WidthInBits = Math::CountEmptyBits(layer->DataWidth);
            layer->HeightInBits = Math::CountEmptyBits(layer->DataHeight);

            // Allocate tile data
            Memory::Alloc(&layer->Tiles, layer->DataWidth * layer->DataHeight * sizeof(Tile), Memory::MEMPOOL_STAGE, false);

            // Initialize tile data
            for (int y = 0; y < layer->Height; y++) {
                for (int x = 0; x < layer->Width; x++)
                    layer->Tiles[(y << layer->WidthInBits) | x] = TILE_EMPTY;
            }

            // Allocate parallax lines
            size_t parallaxIndexLineCount = M_MAX(layer->DataWidth, layer->DataHeight) << TILE_SIZE_IN_BITS;
            Memory::Alloc(&layer->ParallaxIndexLines, parallaxIndexLineCount * sizeof(Uint8), Memory::MEMPOOL_STAGE, false);

            // Initialize parallax lines
            for (int line = 0; line < parallaxIndexLineCount; line++)
                layer->ParallaxIndexLines[line] = 0;
        }
        // If this changes the data size,
        else if (Math::ToNextPOT(width) != layer->DataWidth || Math::ToNextPOT(height) != layer->DataHeight) {
            // Set data size in tiles and bits
            layer->DataWidth = Math::ToNextPOT(layer->Width);
            layer->DataHeight = Math::ToNextPOT(layer->Height);
            layer->WidthInBits = Math::CountEmptyBits(layer->DataWidth);
            layer->HeightInBits = Math::CountEmptyBits(layer->DataHeight);

            // Re-allocate tile data
            Memory::Alloc(&layer->Tiles, layer->DataWidth * layer->DataHeight * sizeof(Tile), Memory::MEMPOOL_STAGE, false);

            // Transfer old tile data to new tile allocation,
            // also initializing unused spaces
            // NOTE: Until next Alloc, old_Tiles is guaranteed to still have data
            int intersect_Width = M_MIN(old_Width, layer->Width);
            int intersect_Height = M_MIN(old_Height, layer->Height);
            for (int y = 0; y < intersect_Height; y++) {
                memcpy(&layer->Tiles[y << layer->WidthInBits], &old_Tiles[y << old_WidthInBits], intersect_Width * sizeof(Tile));

                for (int x = intersect_Width; x < layer->Width; x++)
                    layer->Tiles[(y << layer->WidthInBits) | x] = TILE_EMPTY;
            }
            for (int y = intersect_Height; y < layer->Height; y++) {
                for (int x = 0; x < layer->Width; x++)
                    layer->Tiles[(y << layer->WidthInBits) | x] = TILE_EMPTY;
            }

            // Re-allocate parallax lines
            Memory::Alloc(&layer->ParallaxIndexLines, (M_MAX(layer->DataWidth, layer->DataHeight) << TILE_SIZE_IN_BITS) * sizeof(Uint8), Memory::MEMPOOL_STAGE, true);

            // Transfer old parallax lines to new parallax lines allocation,
            // also initializing unused spaces
            size_t parallaxIndexLineCount = M_MAX(layer->DataWidth, layer->DataHeight) << TILE_SIZE_IN_BITS;
            size_t old_parallaxIndexLineCount = M_MAX(old_DataWidth, old_DataHeight) << TILE_SIZE_IN_BITS;
            size_t intersect_parallaxIndexLineCount = M_MIN(parallaxIndexLineCount, old_parallaxIndexLineCount);
            memcpy(layer->ParallaxIndexLines, old_ParallaxIndexLines, intersect_parallaxIndexLineCount * sizeof(Uint8));
            for (int line = intersect_parallaxIndexLineCount; line < parallaxIndexLineCount; line++) {
                layer->ParallaxIndexLines[line] = 0;
            }
        }
        // If this doesn't change the data size,
        else {
            // Initialize unused spaces
            int intersect_Width = M_MIN(old_Width, layer->Width);
            int intersect_Height = M_MIN(old_Height, layer->Height);
            for (int y = 0; y < intersect_Height; y++) {
                for (int x = intersect_Width; x < layer->Width; x++)
                    layer->Tiles[(y << layer->WidthInBits) | x] = TILE_EMPTY;
            }
            for (int y = intersect_Height; y < layer->Height; y++) {
                for (int x = 0; x < layer->Width; x++)
                    layer->Tiles[(y << layer->WidthInBits) | x] = TILE_EMPTY;
            }
        }

        tilePlacementField->UpdateRenderTarget = true;
    }
    void LayerResizeParallaxInfoCount(int layerIndex, int count) {
        Layer* layer = &Layers[layerIndex];

        auto old_ParallaxInfos = layer->ParallaxInfos;
        auto old_ParallaxInfoCount = layer->ParallaxInfoCount;

        if (layer->ParallaxInfos == NULL) {
            // Set parallax info count
            layer->ParallaxInfoCount = count;

            // Allocate parallax infos
            Memory::Alloc(&layer->ParallaxInfos, layer->ParallaxInfoCount * sizeof(Parallax), Memory::MEMPOOL_STAGE, false);

            // Initialize parallax infos
            for (int i = 0; i < layer->ParallaxInfoCount; i++) {
                auto parallax = &layer->ParallaxInfos[i];
                parallax->RelativeParallax = 0x10000;
                parallax->ConstantParallax = 0x00000;
                parallax->CanDeform = true;
            }
        }
        else {
            // Set parallax info count
            layer->ParallaxInfoCount = count;

            // Allocate parallax infos
            Memory::Alloc(&layer->ParallaxInfos, count * sizeof(Parallax), Memory::MEMPOOL_STAGE, false);

            // Migrate old data
            for (int i = 0; i < old_ParallaxInfoCount && i < count; i++) {
                layer->ParallaxInfos[i] = old_ParallaxInfos[i];
            }

            // Initialize unused parallax infos (if new count > old count)
            for (int i = old_ParallaxInfoCount; i < count; i++) {
                auto parallax = &layer->ParallaxInfos[i];
                parallax->ConstantParallax = 0x0000;
                parallax->RelativeParallax = 0x10000;
                parallax->CanDeform = true;
            }
        }
    }
    void LayerRemapAllTiles() {
        for (int l = 0; l < LayerCount; l++) {
            Layer* layer = &Layers[l];
            int rowLength = layer->Width;
            for (int row = 0; row < layer->Height; row++) {
                Tile* tileRow = &layer->Tiles[row << layer->WidthInBits];
                for (int col = 0; col < layer->Width; col++) {
                    if (tileRow[col] == TILE_EMPTY)
                        continue;

                    int newID = LinkedStage->TileRemapArray[tileRow[col].ID];
                    if (newID == -1)
                        tileRow[col] = TILE_EMPTY;
                    else
                        tileRow[col].ID = newID;
                }
            }
        }

        tilePlacementField->UpdateRenderTarget = true;
    }

    void StampCollectionUpdateUI() {
        stampCollection->UpdateList();
    }
    void StampCollectionAdd(const char* title, Stamp* stamp) {
        SavedStamp* savedStamp = new SavedStamp();
        Strings::FromCString(&savedStamp->Title, title, 0);
        savedStamp->Data = stamp;
        Stamps.Add(savedStamp);

        StampCollectionUpdateUI();
    }
    void StampCollectionDuplicate(int index) {
        SavedStamp* srcStamp = Stamps[index];
        SavedStamp* dstStamp = new SavedStamp();

        CString prefix = "Copy of ";
        char dstTitle[256];
        strcpy(dstTitle, prefix);
        Strings::ToCString(dstTitle + strlen(prefix), &srcStamp->Title);

        Strings::FromCString(&dstStamp->Title, dstTitle, 0);
        dstStamp->Data = Stamp::Clone(srcStamp->Data);
        Stamps.Insert(index + 1, dstStamp);

        StampCollectionUpdateUI();
    }
    void StampCollectionClear() {
        Stamps.Clear();

        StampCollectionUpdateUI();
    }
    void StampCollectionOpen(CString filename) {
        const Uint32 MAGIC_HSTM = 0x4D545348;

        Stream* stream = FileStream::New(filename, FileStream::READ_ACCESS);
        if (stream) {
            // Read magic
            Uint32 magic = stream->ReadUInt32();
            if (magic != MAGIC_HSTM) {
                Diagnostics::SetError("Invalid magic for HSTM!");
            }

            // Read size
            int count = stream->ReadUInt32();

            for (int i = 0; i < count; i++) {
                SavedStamp* savedStamp = new SavedStamp();
                savedStamp->Read(stream);
                Stamps.Add(savedStamp);
            }
            stream->Close();

            StampCollectionUpdateUI();
        }
        else {
            fprintf(stderr, "StampCollectionOpen failed with reason: %s\n", Diagnostics::ErrorString);
        }
    }
    void StampCollectionSave(CString filename) {
        const Uint32 MAGIC_HSTM = 0x4D545348;

        Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
        if (stream) {
            // Write magic
            stream->WriteUInt32(MAGIC_HSTM);

            // Write size
            stream->WriteUInt32(Stamps.Count());

            for (int i = 0; i < Stamps.Count(); i++) {
                SavedStamp* savedStamp = Stamps[i];
                savedStamp->Write(stream);
            }
            stream->Close();
        }
        else {
            fprintf(stderr, "StampCollectionSave failed with reason: %s\n", Diagnostics::ErrorString);
        }
    }

    void EntityUpdateUI() {
        entityProperties->UpdateEntityList();

        tilePlacementField->UpdateRenderTarget = true;
    }
    void EntityAdd(int classID) {
        Entity* entity = &EntitySlots[EntityCount];
        EntityEditorData* metadata = &EntityEditorSlots[EntityCount];
        if (EntityCount >= EntityCapacity)
            return;

        memset(entity, 0, sizeof(EntitySlot));
        entity->Position.X.Whole = (int)tilePlacementField->ViewX + Graphics::Views->Width / 2;
        entity->Position.Y.Whole = (int)tilePlacementField->ViewY + Graphics::Views->Height / 2;
        entity->ClassID = classID;
        entity->Filter = tilePlacementField->CurrentFilter;

        metadata->Properties = new List<EntityProperty>();


        tilePlacementField->SelectTool(TilePlacementField::TOOL_ENTITY_TOOL);
        EntityCount++;

        EntityUpdateUI();
    }
    void EntityRemove(int slot) {
        if (entityProperties->propertyGridEntity->SelectedEntity == &EntitySlots[slot])
            entityProperties->propertyGridEntity->SelectedEntity = NULL;

        ActionStack_Do(new EntityRemoveCommand(this, slot), tilePlacementField->ActionSiblingKeyID << 8);
    }
    void EntityRemapClasses() {
        int classID = -1;

        // anything that was:
        // classID - 1     -> classID - 1
        // classID         -> -1
        // classID + 1     -> classID
        // classID + 2     -> classID + 1
        // classID + n + 1 -> classID + n
        List<int> classIdRemapList;

        for (int i = 0; i < classID; i++)
            classIdRemapList.Add(i);

        classIdRemapList.Add(-1);

        for (int i = classID + 1; i < LinkedStage->Classes.size() + 1; i++)
            classIdRemapList.Add(i - 1);

        // Remap all the entity classes
        for (int e = 0; e < EntityCount; e++) {
            Entity* entity = &EntitySlots[e];
            entity->ClassID = classIdRemapList[entity->ClassID];
        }
    }
    void EntitySelectAllOfClass(int classID) {
        tilePlacementField->SelectedEntity_Clear();

        for (int i = 0; i < EntityCount; i++) {
            auto ent = &EntitySlots[i];
            auto entEd = &EntityEditorSlots[i];
            if (!(ent->Filter & tilePlacementField->CurrentFilter))
                continue;

            if (ent->ClassID == classID)
                tilePlacementField->SelectedEntity_Add(ent);
        }

        tilePlacementField->UpdateRenderTarget = true;
    }
    void EntitySelectAll() {
        tilePlacementField->SelectedEntity_Clear();

        for (int i = 0; i < EntityCount; i++) {
            auto ent = &EntitySlots[i];
            auto entEd = &EntityEditorSlots[i];
            if (!(ent->Filter & tilePlacementField->CurrentFilter))
                continue;

            tilePlacementField->SelectedEntity_Add(ent);
        }

        tilePlacementField->UpdateRenderTarget = true;
    }
    int  EntityGetSlot(Entity* entity) {
        return (EntitySlot*)entity - &EntitySlots[0];
    }

    void ClassUpdateUI() {
        objectClasses->UpdateClassList();
    }
    void ClassRemove(int classID) {
        UsedClass* usedClass = LinkedStage->Classes[classID];
        delete usedClass;
        LinkedStage->Classes.erase(LinkedStage->Classes.begin() + classID);

        ClassUpdateUI();

        // Change any classIDs that need changing and
        // Remove all the empty objects
        int freeIndex = 0;
        int removed = 0;
        for (int i = 0; i < EntityCount; i++) {
            Entity* entity = &EntitySlots[i];
            EntityEditorData* metadata = &EntityEditorSlots[i];
            if (entity->ClassID != classID) {
                if (entity->ClassID > classID)
                    entity->ClassID--;

                EntitySlots[freeIndex] = EntitySlots[i];
                EntityEditorSlots[freeIndex] = EntityEditorSlots[i];
                freeIndex++;
            }
            else
                removed++;
        }
        EntityCount -= removed;

        EntityUpdateUI();
    }

    void ClassUpdatePropertyUI() {
        entityProperties->propertyGridEntity->UpdatePropertyUI();
        objectClasses->UpdatePropertyList();
    }
    bool ClassHasProperty(int classID, CString propertyName) {
        UsedClass* usedClass = LinkedStage->Classes[classID];
        Hash propertyNameHash = MD5_HashString(propertyName);
        for (int i = 0; i < usedClass->Properties.Count(); i++) {
            if (usedClass->Properties[i].Name == propertyNameHash)
                return true;
        }
        return false;
    }
    void ClassAddProperty(int classID, CString propertyName, int propertyType) {
        UsedClass* usedClass = LinkedStage->Classes[classID];
        usedClass->Properties.Add(Classes::ClassAttribute { });

        Classes::ClassAttribute* newProperty = new (&usedClass->Properties.Items[usedClass->Properties.Count() - 1]) Classes::ClassAttribute(propertyName);
        newProperty->AttributeType = propertyType;

        ClassUpdatePropertyUI();
    }
    void ClassRemoveProperty(int classID, Hash propertyNameHash) {
        UsedClass* usedClass = LinkedStage->Classes[classID];
        for (int i = 0; i < usedClass->Properties.Count(); i++) {
            if (usedClass->Properties[i].Name == propertyNameHash) {
                for (int m = 0; m < EntityCount; m++) {
                    Entity* entity = &EntitySlots[m];
                    EntityEditorData* metadata = &EntityEditorSlots[m];
                    if (entity->ClassID == classID) {
                        for (int p = 0; p < metadata->Properties->Count(); p++) {
                            if (metadata->Properties->Items[p].NameHash == propertyNameHash) {
                                metadata->Properties->RemoveAt(p);
                                break;
                            }
                        }
                    }
                }
                usedClass->Properties.RemoveAt(i);
                break;
            }
        }

        ClassUpdatePropertyUI();
    }
    void ClassRemoveProperty(int classID, CString propertyName) {
        ClassRemoveProperty(classID, MD5_HashString(propertyName));
    }

    // Action / Command Stack Functions
    UndoRedoStack* actions = NULL;
    void ActionStack_Do(Command* cmd, int siblingID) {
        actions->Do(cmd, siblingID);

        SetChangesUnsaved();
    }
    void ActionStack_Undo() {
        actions->Undo();
    }
    void ActionStack_Redo() {
        actions->Redo();
    }
    void ActionStack_Clear() {
        actions->Reset();
    }

    // UI stuffs
    TileSelector* tileSelector;
    ObjectClasses* objectClasses;
    StampCollection* stampCollection;
    TilePlacementField* tilePlacementField;
    TileCollisionEditor* tileCollisionEditor;
    TilePlacementToolbar* tilePlacementToolbar;
    EntityProperties* entityProperties;
    LayerControls* layerControls;

    std::vector<Control*> stupidGC;

    template<class T>
    T* StupidGC(T* a) {
        stupidGC.push_back(a);
        return a;
    }

    // UI Functions
    SceneEditor() : ResourceEditor() {
        Dock = DOCK_FILL;
        Padding = 0;

        // TabPage BG: Color(0x282C34, 0xFF)
        // TabControl BG: Color(0x21252B, 0xFF)

        BackColor = Color(0x21252B, 0xFF);

        actions = new UndoRedoStack();

        // Init panels
        SplitContainer* splitterMain = StupidGC(new SplitContainer());
        SplitContainer* splitterField = StupidGC(new SplitContainer());
        SplitContainer* splitterTiles = StupidGC(new SplitContainer());
        TabControl* leftTab = StupidGC(new TabControl());
        TabPage* tabPageTiles = StupidGC(new TabPage("Tiles"));
        TabPage* tabPageStamps = StupidGC(new TabPage("Stamps"));
        TabPage* tabPageCollision = StupidGC(new TabPage("Collision"));
        TabControl* rightTab = StupidGC(new TabControl());
        TabPage* tabPageEntities = StupidGC(new TabPage("Entities"));
        TabPage* tabPageObjects = StupidGC(new TabPage("Objects"));
        TabPage* tabPageLayers = StupidGC(new TabPage("Layers"));
        TabPage* tabPageSettings = StupidGC(new TabPage("Settings"));
        tileSelector = new TileSelector(this);
        stampCollection = new StampCollection(this);
        tileCollisionEditor = new TileCollisionEditor(this);
        tilePlacementField = new TilePlacementField(this);
        entityProperties = new EntityProperties(this);
        objectClasses = new ObjectClasses(this);
        tilePlacementToolbar = new TilePlacementToolbar(this);
        layerControls = new LayerControls(this);
        FlowLayoutPanel* tileSelectorButtons = StupidGC(new FlowLayoutPanel());
        Button* buttonImportTileset = StupidGC(new Button());
        Label* labelCurrentTileRange = StupidGC(new Label());

        // splitterTiles Buttons
        tileSelector->ShowTileGraphics = true;
        tileSelector->onSelectedTileRangeChanged += [this, labelCurrentTileRange](auto* idont, auto* caare) -> void {
            int _min = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
            int _max = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
            char stringBuffer[256];
            if (_min != _max)
                snprintf(stringBuffer, 255, "Current Tile Range: %d - %d", _min, _max);
            else
                snprintf(stringBuffer, 255, "Current Tile ID: %d", tileSelector->SelectedTileID);
            labelCurrentTileRange->SetText(stringBuffer);
        };
        tileSelector->onSelectedTileIDChanged += [this, labelCurrentTileRange](auto* idont, auto* caare) -> void {
            int _min = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
            int _max = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
            char stringBuffer[256];
            if (_min != _max)
                snprintf(stringBuffer, 255, "Current Tile Range: %d - %d", _min, _max);
            else
                snprintf(stringBuffer, 255, "Current Tile ID: %d", tileSelector->SelectedTileID);
            labelCurrentTileRange->SetText(stringBuffer);
        };

        tileSelectorButtons->BackColor = tileSelector->BackColor;
        tileSelectorButtons->Padding = 6;
        tileSelectorButtons->FlowDirection = FlowDirection::TOP_TO_BOTTOM;
        tileSelectorButtons->Dock = DOCK_FILL;

        buttonImportTileset->Dock = DOCK_NONE;
        buttonImportTileset->Anchor = ANCHOR_NONE;
        buttonImportTileset->Size = { 200, 25 };
        buttonImportTileset->SetText("Import Tileset / Stamps...");
        buttonImportTileset->onClick += [this](auto* a, auto* d) -> void {
            if (PromptImportTileset()) {
                tilePlacementField->RemapStampDataToBePlaced();
                LinkedStage->RemapTileConfig();
                LayerRemapAllTiles();
            }
        };
        tileSelectorButtons->Controls.Add(buttonImportTileset);

        labelCurrentTileRange->SetText("Current Tile ID: 0");
        labelCurrentTileRange->Margin.Top = 6;
        labelCurrentTileRange->Dock = DOCK_TOP;
        labelCurrentTileRange->Anchor = ANCHOR_NONE;
        tileSelectorButtons->Controls.Add(labelCurrentTileRange);

        // splitterTiles
        splitterTiles->Dock = DOCK_FILL;
        splitterTiles->Orientation = SplitOrientation::Vertical;
        splitterTiles->FixedPanel = SplitPanelFix::Panel2;
        splitterTiles->IsSplitterFixed = true;
        splitterTiles->SplitterWidth = 0;
        splitterTiles->Size = { 1000, 1000 };
        splitterTiles->SplitterDistance = 1000 - 55 - tileSelectorButtons->Padding.Vertical();
        splitterTiles->BackColor = Color(0x000000, 0x00);
        splitterTiles->Panel1->BackColor = Color(0x000000, 0x00);
        splitterTiles->Panel2->BackColor = Color(0x000000, 0x00);

        splitterTiles->Panel1->Controls.Add(tileSelector);
        splitterTiles->Panel2->Controls.Add(tileSelectorButtons);

        // splitterMain
        splitterMain->Dock = DOCK_FILL;
        splitterMain->Size = { 1000, 1000 };
        splitterMain->SplitterDistance = tileSelector->Padding.Horizontal() + tileSelector->TileSpace * 16 + 16;
        splitterMain->BackColor = Color(0x000000, 0x00);
        splitterMain->Panel1->BackColor = Color(0x000000, 0x00);
        splitterMain->Panel2->BackColor = Color(0x000000, 0x00);
        splitterMain->FixedPanel = SplitPanelFix::Panel1;

        splitterMain->Panel1->Controls.Add(leftTab);
        splitterMain->Panel2->Controls.Add(splitterField);
        splitterMain->Panel1MinSize = tileSelector->Padding.Horizontal() + tileSelector->TileSize + 16;

        // splitterField
        splitterField->Dock = DOCK_FILL;
        splitterField->Size = { 1000, 1000 };
        splitterField->SplitterDistance = 1000 - 300;
        splitterField->BackColor = Color(0x000000, 0x00);
        splitterField->Panel1->BackColor = Color(0x000000, 0x00);
        splitterField->Panel2->BackColor = Color(0x000000, 0x00);
        splitterField->FixedPanel = SplitPanelFix::Panel2;

        splitterField->Panel1->Controls.Add(tilePlacementToolbar);
        splitterField->Panel1->Controls.Add(tilePlacementField);
        splitterField->Panel2->Controls.Add(rightTab);

        // Add controls
        Controls.Add(splitterMain);

        // leftTab
        tabPageTiles->Controls.Add(splitterTiles);
        tabPageStamps->Controls.Add(stampCollection);
        tabPageCollision->Controls.Add(tileCollisionEditor);
        leftTab->TabPages.Add(tabPageTiles);
        leftTab->TabPages.Add(tabPageStamps);
        leftTab->TabPages.Add(tabPageCollision);
        leftTab->SelectedIndex = 0;
        leftTab->Dock = DOCK_FILL;

        // rightTab
        tabPageEntities->Controls.Add(entityProperties);
        tabPageObjects->Controls.Add(objectClasses);
        tabPageLayers->Controls.Add(layerControls);
        rightTab->TabPages.Add(tabPageEntities);
        rightTab->TabPages.Add(tabPageObjects);
        rightTab->TabPages.Add(tabPageLayers);
        // rightTab->TabPages.Add(tabPageSettings);
        rightTab->SelectedIndex = 0;
        rightTab->Dock = DOCK_FILL;

        // Tool stuff
        tileSelector->onSelectedTileRangeChanged += [this](void* sender, EventArgs* e) -> void {
            Tile* tile = tileSelector->StampTileBuffer;
            int start = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
            int end = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
            size_t maxStampBufferSize = sizeof(tileSelector->StampTileBuffer) / sizeof(tileSelector->StampTileBuffer[0]);
            for (int i = 0; i <= end - start && i < maxStampBufferSize; i++)
                (tile++)->ID = start + i;

            tilePlacementField->Action_SetStampData(end - start + 1, 1, tileSelector->StampTileBuffer);
        };

        tilePlacementToolbar->toolStripButtonSelect->onMouseDown += [this] (void* sender, MouseEventArgs* e) -> void {
            tilePlacementField->SelectTool(TilePlacementField::TOOL_SELECT);
        };
        tilePlacementToolbar->toolStripButtonErase->onMouseDown += [this] (void* sender, MouseEventArgs* e) -> void {
            tilePlacementField->SelectTool(TilePlacementField::TOOL_ERASE);
        };
        tilePlacementToolbar->toolStripButtonTileStamp->onMouseDown += [this] (void* sender, MouseEventArgs* e) -> void {
            tilePlacementField->SelectTool(TilePlacementField::TOOL_TILE_STAMP);
        };
		tilePlacementToolbar->toolStripButtonTileEyedropper->onMouseDown += [this] (void* sender, MouseEventArgs* e) -> void {
            tilePlacementField->SelectTool(TilePlacementField::TOOL_TILE_EYEDROPPER);
        };
		tilePlacementToolbar->toolStripButtonTileBucketFill->onMouseDown += [this] (void* sender, MouseEventArgs* e) -> void {
            tilePlacementField->SelectTool(TilePlacementField::TOOL_TILE_BUCKET_FILL);
        };
		tilePlacementToolbar->toolStripButtonTileCollisionBrush->onMouseDown += [this] (void* sender, MouseEventArgs* e) -> void {
            tilePlacementField->SelectTool(TilePlacementField::TOOL_TILE_COLLISION_BRUSH);
        };
		tilePlacementToolbar->toolStripButtonParallaxTool->onMouseDown += [this] (void* sender, MouseEventArgs* e) -> void {
            tilePlacementField->SelectTool(TilePlacementField::TOOL_PARALLAX_RESIZER);
        };
		tilePlacementToolbar->toolStripButtonEntityTool->onMouseDown += [this] (void* sender, MouseEventArgs* e) -> void {
            tilePlacementField->SelectTool(TilePlacementField::TOOL_ENTITY_TOOL);
        };
        tilePlacementField->SelectTool(TilePlacementField::TOOL_SELECT);
    }
    ~SceneEditor() {
        delete tileSelector;
        delete objectClasses;
        delete stampCollection;
        delete tilePlacementField;
        delete tileCollisionEditor;
        delete tilePlacementToolbar;
        delete entityProperties;
        delete layerControls;
        delete actions;

        for (int i = 0; i < EntityCapacity; i++) {
            auto metadata = &EntityEditorSlots[i];
            for (int p = 0; p < metadata->Properties->Count(); p++) {
                free(metadata->Properties->Items[p].ValueData);
            }
            // delete metadata->Properties;
        }

        for (int i = 0; i < stupidGC.size(); i++) {
            delete stupidGC[i];
        }

        for (int i = 0; i < Stamps.Count(); i++) {
            delete Stamps[i];
        }

        delete LinkedStage;

        Memory::RunGC(Memory::MEMPOOL_STRING);
    }

	void LinkScene() {
		// Link currently active scene
		Scene::Layers = this->Layers;
		Graphics::TileImageData = this->LinkedStage->TileImageTextures;
		Graphics::TileCollisionImageData = this->LinkedStage->TileCollisionTextures;
		Scene::CurrentEntity = this->CurrentEntity;
		Scene::EntitySlots = this->EntitySlots;
		Scene::ClassIndexList = this->ClassIndexList;
		Scene::ClassIndexCount = this->ClassIndexCount;
	}

    void Update() {
		LinkScene();

        Control::Update();
    }
    void Render() {
		LinkScene();
        Control::Render();
    }
};

// .HSPR - Sprite File
// .HMSH - 3D Mesh File
// .HPAL - Palette File
// .HSTG - Stage File
// .HSCN - Scene File
// .HCOL - Tile Collision File
// .HTIL - Tile Image File (Un/compressed)
// .HATCH - Resource Pack File
// .HPROJ - Project Info File

struct Form_NewProjectWizard : Form {
    Label* labelProjectName;
    TextboxBase* textBoxProjectName;
    Label* labelEngineVersion;
    ComboBox* comboBoxEngineVersion;
    Label* labelShortName;
    TextboxBase* textBoxShortName;
    Button* buttonOK;
    Button* buttonCancel;

    FlowLayoutPanel* mainPanel;

    Form_NewProjectWizard() : Form(250, 140, "") {
        mainPanel = new FlowLayoutPanel();
        mainPanel->BackColor = Color(0x000000, 0x00);
        mainPanel->Dock = DOCK_FILL;
        mainPanel->FlowDirection = FlowDirection::LEFT_TO_RIGHT;
        mainPanel->Padding = 10;
        mainPanel->WrapContents = false;

        // Project Name
        labelProjectName = new Label("Project Name:");
        labelProjectName->Anchor = ANCHOR_TOP;
        labelProjectName->Margin.Top = 5;
        labelProjectName->Margin.Right = 10;
        mainPanel->Controls.Add(labelProjectName);

        textBoxProjectName = new TextboxBase("");
        textBoxProjectName->Size = { 180, 25 };
        textBoxProjectName->LineBreak = true;
        mainPanel->Controls.Add(textBoxProjectName);

        // Engine Version
        labelEngineVersion = new Label("Engine Version:");
        labelEngineVersion->Anchor = ANCHOR_TOP;
        labelEngineVersion->Margin.Top = 5;
        labelEngineVersion->Margin.Right = 10;
        mainPanel->Controls.Add(labelEngineVersion);

        comboBoxEngineVersion = new ComboBox();
        comboBoxEngineVersion->Anchor = ANCHOR_TOP;
        comboBoxEngineVersion->Size = { 180, 25 };
        comboBoxEngineVersion->LineBreak = true;
        comboBoxEngineVersion->Margin.Bottom = 5;
        comboBoxEngineVersion->Items.Add("Hatch Game Engine");
        comboBoxEngineVersion->Items.Add("HatchLite");
        comboBoxEngineVersion->Select(0);
        mainPanel->Controls.Add(comboBoxEngineVersion);

        // Short Name
        labelShortName = new Label("Short Name: (for mobile)");
        labelShortName->Anchor = ANCHOR_TOP;
        labelShortName->Margin.Top = 5;
        labelShortName->Margin.Right = 10;
        mainPanel->Controls.Add(labelShortName);

        textBoxShortName = new TextboxBase("Hatch");
        textBoxShortName->Size = { 180, 25 };
        textBoxShortName->LineBreak = true;
        mainPanel->Controls.Add(textBoxShortName);


        buttonOK = new Button("OK");
        buttonOK->Anchor = ANCHOR_TOP;
        buttonOK->Size = { 100, 25 };
        buttonOK->Margin.Right = 5;
        buttonOK->Margin.Top = 15;
        buttonOK->onClick += [this](auto object, auto e) -> void {
            this->Result = DialogResult::OK;
            if (this->textBoxProjectName->Text.Length > 0)
                this->Close();
        };
        mainPanel->Controls.Add(buttonOK);

        buttonCancel = new Button("Cancel");
        buttonCancel->Anchor = ANCHOR_TOP;
        buttonCancel->Size = { 100, 25 };
        buttonCancel->Margin.Top = 15;
        buttonCancel->onClick += [this](auto object, auto e) -> void {
            this->Result = DialogResult::Cancel;
            this->Close();
        };
        mainPanel->Controls.Add(buttonCancel);


        this->Controls.Add(mainPanel);
        this->UpdateLayout(); // This should theoretically happen during Controls.Add

        this->Size = { 500, buttonCancel->Location.Y + buttonCancel->Size.Get().H + mainPanel->Padding.Bottom };
    }
    ~Form_NewProjectWizard() {
        delete labelProjectName;
        delete textBoxProjectName;
        delete labelEngineVersion;
        delete comboBoxEngineVersion;
        delete labelShortName;
        delete textBoxShortName;
        delete buttonOK;
        delete buttonCancel;

        delete mainPanel;
    }
};

struct RecentProject {
    char* Name = NULL;
    char* Filepath = NULL;
};
struct HatchProject {
    enum HatchVersion : int {
        HatchGameEngine,
        HatchLite,
    };

    const char* MAGIC_HPROJ = "HPROJ";

    char* ProjectName;
    int EngineVersion;

    List<char*> LastOpenFiles;

    // Mobile settings
    char* ShortName;

    HatchProject() {
        ProjectName = NULL;
        EngineVersion = HatchVersion::HatchGameEngine;

        ShortName = NULL;
    }
    ~HatchProject() {
        free(ProjectName);
        free(ShortName);
    }

    void Read(Stream* stream) {
        char magic[5];
        char fileTypeVersion[3];

        // Read Magic
        stream->ReadBytes(magic, 5);
        if (memcmp(MAGIC_HPROJ, magic, 5) != 0)
            return;

        // Read File Type version
        stream->ReadBytes(fileTypeVersion, 3);

        // Others
        ProjectName = stream->ReadHeaderedString();
        EngineVersion = stream->ReadByte();

        ShortName = stream->ReadHeaderedString();

        // v0.0.2: Added last open files support
        if (fileTypeVersion[2] < 2) return;

        int count = stream->ReadInt32();
        for (int i = 0; i < count; i++) {
            LastOpenFiles.Add(stream->ReadHeaderedString());
        }
    }
    void Write(Stream* stream) {
        char fileTypeVersion[3] = { 0, 0, 2 };

        // Write Magic
        stream->WriteBytes((void*)MAGIC_HPROJ, 5);

        // Write File Type version
        stream->WriteBytes(fileTypeVersion, 3);

        // Others
        stream->WriteHeaderedString(ProjectName);
        stream->WriteByte(EngineVersion);

        stream->WriteHeaderedString(ShortName);

        stream->WriteInt32(LastOpenFiles.Count());
        for (int i = 0; i < LastOpenFiles.Count(); i++) {
            stream->WriteHeaderedString(LastOpenFiles[i]);
        }
    }
};
struct HatchStudioSettings {
    int WindowX = 0;
    int WindowY = 0;
    int WindowWidth = 1280;
    int WindowHeight = 720;
    bool Maximized = false;
    bool RunFromStartScene = true;
    bool ReopenLastProject = true;
    List<RecentProject> RecentProjects;

    ~HatchStudioSettings() {
        return;
    }

    void Read(Stream* stream) {
        WindowX = stream->ReadInt32();
        WindowY = stream->ReadInt32();
        WindowWidth = stream->ReadInt32();
        WindowHeight = stream->ReadInt32();
        Maximized = stream->ReadInt32();
        RunFromStartScene = stream->ReadInt32();

        int count = stream->ReadInt32();
        for (int i = 0; i < count; i++) {
            RecentProjects.Add(RecentProject { stream->ReadHeaderedString(), stream->ReadHeaderedString() });
        }
    }
    void Write(Stream* stream) {
        stream->WriteInt32(WindowX);
        stream->WriteInt32(WindowY);
        stream->WriteInt32(WindowWidth);
        stream->WriteInt32(WindowHeight);
        stream->WriteInt32(Maximized);
        stream->WriteInt32(RunFromStartScene);

        stream->WriteInt32(RecentProjects.Count());
        for (int i = 0; i < RecentProjects.Count(); i++) {
            stream->WriteHeaderedString(RecentProjects[i].Name);
            stream->WriteHeaderedString(RecentProjects[i].Filepath);
        }
    }
};

struct HatchStudioForm : Form {
    static HatchStudioForm* MainForm;

    UI::Menu* mainMenu = NULL;
    UI::Menu* menuFile = NULL;
    UI::Menu* menuRecentProjects = NULL;
    UI::Menu* menuProject = NULL;
    UI::Menu* menuRunFromScene = NULL;
    UI::Menu* menuHelp = NULL;
    UI::Menu* menuApple = NULL;
    UI::Menu* menuWindow = NULL;
    int menuIndex_SaveFile = -1;
    int menuIndex_SaveFileAs = -1;
    int menuIndex_SaveAllFile = -1;
    int menuIndex_CloseFile = -1;
    int menuIndex_CloseAllFiles = -1;
    int menuIndex_RunFromStartScene = -1;
    int menuIndex_RunFromCurrentScene = -1;

    #pragma region Shortcut definitions
    #if !defined(_MACOS) // #if defined(_WINDOWS)
    const int SHORTCUT_NEW_PROJECT = UI::Menu::SM_CONTROL | UI::Menu::SM_ALT | 'n';
    const int SHORTCUT_OPEN_PROJECT = UI::Menu::SM_CONTROL | UI::Menu::SM_ALT | 'o';
    const int SHORTCUT_CLOSE_PROJECT = UI::Menu::SM_CONTROL | UI::Menu::SM_ALT | 'w';

    const int SHORTCUT_NEW_FILE = UI::Menu::SM_CONTROL | 'n';
    const int SHORTCUT_OPEN_FILE = UI::Menu::SM_CONTROL | 'o';
    const int SHORTCUT_CLOSE_FILE = UI::Menu::SM_CONTROL | 'w';

    const int SHORTCUT_SAVE_FILE = UI::Menu::SM_CONTROL | 's';
    const int SHORTCUT_SAVE_FILE_AS = UI::Menu::SM_CONTROL | UI::Menu::SM_SHIFT | 's';
    const int SHORTCUT_SAVE_ALL = UI::Menu::SM_CONTROL | UI::Menu::SM_ALT | 's';

    const int SHORTCUT_CLOSE_ALL = UI::Menu::SM_NONE; // SM_CONTROL | SM_SHIFT | SM_ALT | 'w';

    const int SHORTCUT_BUILD_GAME_LOGIC = UI::Menu::SM_CONTROL | 'b'; // CMD+B on Mac, CTRL+B on Windows
    const int SHORTCUT_RUN_LOCALLY = UI::Menu::SM_CONTROL | 'r'; // CMD+R on Mac, F5 on Windows
    const int SHORTCUT_RUN_REMOTELY = UI::Menu::SM_NONE; // CMD+R on Mac, F5 on Windows
    #endif
    #if defined(_MACOS)
    const int SHORTCUT_NEW_PROJECT = UI::Menu::SM_COMMAND | UI::Menu::SM_ALT | 'n';
    const int SHORTCUT_OPEN_PROJECT = UI::Menu::SM_COMMAND | UI::Menu::SM_ALT | 'o';
    const int SHORTCUT_CLOSE_PROJECT = UI::Menu::SM_COMMAND | UI::Menu::SM_ALT | 'w';

    const int SHORTCUT_NEW_FILE = UI::Menu::SM_COMMAND | 'n';
    const int SHORTCUT_OPEN_FILE = UI::Menu::SM_COMMAND | 'o';
    const int SHORTCUT_CLOSE_FILE = UI::Menu::SM_COMMAND | 'w';

    const int SHORTCUT_SAVE_FILE = UI::Menu::SM_COMMAND | 's';
    const int SHORTCUT_SAVE_FILE_AS = UI::Menu::SM_COMMAND | UI::Menu::SM_SHIFT | 's';
    const int SHORTCUT_SAVE_ALL = UI::Menu::SM_COMMAND | UI::Menu::SM_ALT | 's';

    const int SHORTCUT_CLOSE_ALL = UI::Menu::SM_NONE; // SM_COMMAND | SM_SHIFT | SM_ALT | 'w';

    const int SHORTCUT_BUILD_GAME_LOGIC = UI::Menu::SM_COMMAND | 'b'; // CMD+B on Mac, CTRL+B on Windows
    const int SHORTCUT_RUN_LOCALLY = UI::Menu::SM_COMMAND | 'r'; // CMD+R on Mac, F5 on Windows
    const int SHORTCUT_RUN_REMOTELY = UI::Menu::SM_NONE; // CMD+R on Mac, F5 on Windows
    #endif
    #pragma endregion
    #pragma region "File" menu actions
    static void Action_NewProject() {
        if (!MainForm->CloseCurrentProject())
            return;

        MainForm->NewProject();
    }
    static void Action_OpenProject() {
        UI::SystemDialog::OpenFileData ofd;
        ofd.Title = "Open Hatch Project...";
        // ofd.InitialDirectory = ProjectDirectory;
        ofd.FilterPatterns.Add("*.HPROJ");
        ofd.Multiselect = false;

        if (UI::SystemDialog::OpenFile(&ofd)) {
            if (!MainForm->CloseCurrentProject())
                return;

            MainForm->OpenProject(ofd.Filenames[0]);
        }
    }
    static void Action_SaveProject() {
        MainForm->SaveProject(MainForm->CurrentProjectFilePath);
    }
    static void Action_CloseProject() {
        MainForm->CloseCurrentProject();
    }
    static void Action_ClearRecentProjects() {
        MainForm->ClearRecentProjects();
    }
    static void Action_NewResource() {
        MainForm->NewFile();
    }
    static void Action_OpenResource() {
        UI::SystemDialog::OpenFileData ofd;
        ofd.Title = "Open a Resource from the current project...";
        // ofd.InitialDirectory = MainForm->CurrentProjectFilePath; + "Scene.hscn"
        ofd.FilterPatterns.Add("*.hscn");
        // ofd.FilterPatterns.Add("*.tmx");
        // ofd.FilterPatterns.Add("*.bin");
        ofd.Multiselect = false;

        if (UI::SystemDialog::OpenFile(&ofd)) {
            for (int i = 0; i < ofd.Filenames.Count(); i++)
                MainForm->OpenFile(ofd.Filenames[i]);
        }
    }
    static void Action_SaveResource() {
        int index = MainForm->MainTabControl->SelectedIndex;
        if (index < 0 || index >= MainForm->Editors.Count())
            return;

        if (MainForm->Editors[index]->JustCreated) {
            MainForm->Editors[index]->PromptSaveAs();
        }
        else {
            MainForm->Editors[index]->Save();
        }
    }
    static void Action_SaveResourceAs() {
        int index = MainForm->MainTabControl->SelectedIndex;
        if (index < 0 || index >= MainForm->Editors.Count())
            return;

        MainForm->Editors[index]->PromptSaveAs();
    }
    static void Action_SaveAllResources() {
        for (int index = 0; index < MainForm->Editors.Count(); index++) {
            MainForm->Editors[index]->Save();
        }
    }
    static void Action_CloseResource() {
        int index = MainForm->MainTabControl->SelectedIndex;
        if (index < 0 || index >= MainForm->Editors.Count())
            return;

        if (!MainForm->Editors[index]->CloseFile())
            return;

        delete MainForm->Editors[index];
        MainForm->Editors.RemoveAt(index);
        MainForm->MainTabControl->TabPages.RemoveAt(index);
        MainForm->ReflectCurrentFileEditorChange();
    }
    static void Action_CloseAllResources() {
        for (int index = 0; index < MainForm->Editors.Count(); index++) {
            if (!MainForm->Editors[index]->CloseFile()) {
                break;
            }

            delete MainForm->Editors[index];
            MainForm->Editors.RemoveAt(index);
            MainForm->MainTabControl->TabPages.RemoveAt(index);
            index--;
        }
        MainForm->ReflectCurrentFileEditorChange();
    }
    static void Action_Exit() {
        // MainForm->Close();
        SDL_Event e;
        e.type = SDL_QUIT;
        SDL_PushEvent(&e);
    }
    #pragma endregion
    #pragma region "Project" menu actions
    static void Action_BuildGameLogic() {

    }
    static void Action_RunLocally() {
        if (MainForm->CurrentProject == NULL)
            return;

        char filePath[256];
        char appPath[256];
        char cmdLine[512];

        int index = MainForm->MainTabControl->SelectedIndex;
        if (index >= 0 && index < MainForm->Editors.Count()) {
            String* title = &MainForm->Editors[index]->FilePath;
            if (title->Length > 0) {
                Strings::ToCString(filePath, title);

                snprintf(appPath, 256, "%s/%s.exe", MainForm->CurrentProjectFolderPath, MainForm->CurrentProject->ProjectName);
                if (MainForm->Preferences->RunFromStartScene)
                    snprintf(cmdLine, 512, "%s", appPath);
                else
                    snprintf(cmdLine, 512, "%s -s %s", appPath, filePath + strlen(MainForm->CurrentProjectFolderPath) + strlen("/Resources/"));

                UI::SystemDialog::StartProcess(appPath, cmdLine, MainForm->CurrentProjectFolderPath);
            }
        }
    }
    static void Action_RunRemotely() {

    }
    static void Action_PackResources() {

    }
    static void Action_PackAssetFolder() {

    }
    #pragma endregion
    #pragma region "Help" menu actions
    static void Action_Documentation() {

    }
    static void Action_AboutHatchStudio() {

    }
    #pragma endregion

    static void Action_Check_RunFromStartScene() {
        MainForm->menuRunFromScene->EditItem(MainForm->menuIndex_RunFromStartScene, "Run From Start Scene", Action_Check_RunFromStartScene, UI::Menu::SM_NONE, true, UI::Menu::ItemType::IT_RADIO_CHECKED);
        MainForm->menuRunFromScene->EditItem(MainForm->menuIndex_RunFromCurrentScene, "Run From Current Scene", Action_Check_RunFromCurrentScene, UI::Menu::SM_NONE, true, UI::Menu::ItemType::IT_RADIO_UNCHECKED);
        MainForm->Preferences->RunFromStartScene = true;
    }
    static void Action_Check_RunFromCurrentScene() {
        MainForm->menuRunFromScene->EditItem(MainForm->menuIndex_RunFromStartScene, "Run From Start Scene", Action_Check_RunFromStartScene, UI::Menu::SM_NONE, true, UI::Menu::ItemType::IT_RADIO_UNCHECKED);
        MainForm->menuRunFromScene->EditItem(MainForm->menuIndex_RunFromCurrentScene, "Run From Current Scene", Action_Check_RunFromCurrentScene, UI::Menu::SM_NONE, true, UI::Menu::ItemType::IT_RADIO_CHECKED);
        MainForm->Preferences->RunFromStartScene = false;
    }

    void MenuSetup() {
        mainMenu = new UI::Menu();
        menuFile = new UI::Menu();
        menuRecentProjects = new UI::Menu();
        menuProject = new UI::Menu();
        menuRunFromScene = new UI::Menu();
        menuHelp = new UI::Menu();

        // "File" menu
        menuFile->AddItem("New Project...", Action_NewProject, SHORTCUT_NEW_PROJECT, true, UI::Menu::ItemType::IT_TEXT, 'N');
        menuFile->AddItem("Open Project...", Action_OpenProject, SHORTCUT_OPEN_PROJECT, true, UI::Menu::ItemType::IT_TEXT, 'O');
        menuFile->AddSubmenu("Recent Projects", menuRecentProjects, 'R');
        menuFile->AddItem("Close Project", Action_CloseProject, SHORTCUT_CLOSE_PROJECT, true, UI::Menu::ItemType::IT_TEXT, 'C');
        menuFile->AddSeparator();
        menuFile->AddItem("New Resource...", Action_NewResource, SHORTCUT_NEW_FILE, true);
        menuFile->AddItem("Open Resource...", Action_OpenResource, SHORTCUT_OPEN_FILE, true);
        menuFile->AddSeparator();
        menuIndex_SaveFile = menuFile->AddItem("Save", Action_SaveResource, SHORTCUT_SAVE_FILE, true, UI::Menu::ItemType::IT_TEXT, 'S');
        menuIndex_SaveFileAs = menuFile->AddItem("Save As...", Action_SaveResourceAs, SHORTCUT_SAVE_FILE_AS, true, UI::Menu::ItemType::IT_TEXT, 'A');
        menuIndex_SaveAllFile = menuFile->AddItem("Save All", Action_SaveAllResources, SHORTCUT_SAVE_ALL, true);
        menuFile->AddSeparator();
        menuIndex_CloseFile = menuFile->AddItem("Close", Action_CloseResource, SHORTCUT_CLOSE_FILE, true);
        menuIndex_CloseAllFiles = menuFile->AddItem("Close All", Action_CloseAllResources, SHORTCUT_CLOSE_ALL, true);
#if defined(_WINDOWS)
        menuFile->AddSeparator();
        menuFile->AddItem("Exit", Action_Exit, UI::Menu::SM_NONE, true);
#endif

        // "Recent Projects" menu
        if (Preferences->RecentProjects.Count()) {
            for (int i = 0; i < Preferences->RecentProjects.Count(); i++) {
                menuRecentProjects->AddItem(Preferences->RecentProjects[i].Name, NULL, UI::Menu::SM_NONE, true);
            }
            menuRecentProjects->AddSeparator();
        }
        menuRecentProjects->AddItem("Clear Recent Projects", Action_ClearRecentProjects, UI::Menu::SM_NONE, true);

        // "Project" menu
        menuProject->AddItem("Build Game Logic", Action_BuildGameLogic, SHORTCUT_BUILD_GAME_LOGIC, false);
        menuProject->AddSeparator();
        menuProject->AddItem("Run Locally", Action_RunLocally, SHORTCUT_RUN_LOCALLY, true);
        menuProject->AddItem("Run On Device...", Action_RunRemotely, SHORTCUT_RUN_REMOTELY, false);
        // Shows any devices on the local network that are actively running the HatchLite application (runs a broadcast or something on a thread)
        // OR any users that are connected to an open lobby
        menuProject->AddSubmenu("Set Run Start Scene", menuRunFromScene);
        menuProject->AddSeparator();
        menuProject->AddItem("Pack Resources", Action_PackResources, UI::Menu::SM_NONE, false);
        menuProject->AddItem("Pack Asset Folder...", Action_PackAssetFolder, UI::Menu::SM_NONE, false);
        menuProject->AddSeparator();
        menuProject->AddItem("Create Release Bundle...", NULL, UI::Menu::SM_NONE, false);

        // "Set Run Start Scene" menu
        menuIndex_RunFromStartScene = menuRunFromScene->AddItem("Run From Start Scene", Action_Check_RunFromStartScene, UI::Menu::SM_NONE, true,
            Preferences->RunFromStartScene ? UI::Menu::ItemType::IT_RADIO_CHECKED : UI::Menu::ItemType::IT_RADIO_UNCHECKED);
        menuIndex_RunFromCurrentScene = menuRunFromScene->AddItem("Run From Current Scene", Action_Check_RunFromCurrentScene, UI::Menu::SM_NONE, true,
            Preferences->RunFromStartScene ? UI::Menu::ItemType::IT_RADIO_UNCHECKED : UI::Menu::ItemType::IT_RADIO_CHECKED);

        // "Help" menu
        menuHelp->AddItem("Documentation", Action_Documentation, UI::Menu::SM_NONE, false);
#if defined(_WINDOWS)
        menuHelp->AddSeparator();
        menuHelp->AddItem("About HatchStudio", Action_AboutHatchStudio, UI::Menu::SM_NONE, false);
#endif

        // Main menu (MacOS)
#if defined(_MACOS)
        menuApple = new UI::Menu();
        menuWindow = new UI::Menu();
        mainMenu->AddSubmenu("HatchStudio", menuApple);
        mainMenu->AddSubmenu("File", menuFile);
        mainMenu->AddSubmenu("Project", menuProject);
        mainMenu->AddSubmenu("Window", menuWindow);
        mainMenu->AddSubmenu("Help", menuHelp);

        UI::Menu::SetAppleMenu(menuApple);
        UI::Menu::SetWindowMenu(menuWindow);
        UI::Menu::SetHelpMenu(menuHelp);
#else
        // Main menu (Anywhere else)
        mainMenu->AddSubmenu("File", menuFile, 'F');
        mainMenu->AddSubmenu("Project", menuProject, 'P');
        mainMenu->AddSubmenu("Help", menuHelp, 'H');
#endif

#ifdef USE_NATIVE_MENU
        UI::Menu::SetNativeMainMenu(mainMenu);
#else
        MenuBarControl->SetMenu(mainMenu);
#endif
    }

    HatchProject* CurrentProject = NULL;
    char* CurrentProjectFilePath = NULL;
    char* CurrentProjectFolderPath = NULL;

    CString ProjectName = NULL;
    CString PresenceState = "";
    Sint64  PresenceStartTime = 0;
    HatchStudioSettings* Preferences = NULL;
    ArrayList<ResourceEditor*> Editors;
    TabControl* MainTabControl = NULL;
    MenuBar* MenuBarControl = NULL;
    HatchStudioForm() : Form(100, 100, NULL) { }
    ~HatchStudioForm() {
        delete MainTabControl;
        delete MenuBarControl;
    }

    bool LoadSettings() {
        Stream* stream = FileStream::New("Preferences.pref", FileStream::READ_ACCESS);
        if (stream) {
            Preferences->Read(stream);
            stream->Close();

            return true;
        }
        return false;
    }
    void SaveSettings() {
        Preferences->Maximized = (SDL_GetWindowFlags(UI::Graphics::Renderer::Window) & SDL_WINDOW_MAXIMIZED);
        if (!Preferences->Maximized) {
            SDL_GetWindowSize(UI::Graphics::Renderer::Window, &Preferences->WindowWidth, &Preferences->WindowHeight);
            SDL_GetWindowPosition(UI::Graphics::Renderer::Window, &Preferences->WindowX, &Preferences->WindowY);
        }

        Stream* stream = FileStream::New("Preferences.pref", FileStream::WRITE_ACCESS);
        if (stream) {
            Preferences->Write(stream);
            stream->Close();
        }
    }

    void OnClosing(FormClosingEventArgs* e) {
        // TODO: Save last open files to project
        e->Cancel = !CloseCurrentProject();
        Form::OnClosing(e);
    }
    void OnClosed(FormClosedEventArgs* e) {
        SaveSettings();

        // Hide the window to make the exit *look* graceful
        SDL_HideWindow(UI::Graphics::Renderer::Window);

        Form::OnClosed(e);
    }

    void UpdatePresence() {
        char stringBuffer[256];
        if (CurrentProject != NULL) {
            sprintf(stringBuffer, "Working on \"%s\"", CurrentProject->ProjectName);
            GameLinker::ServiceFuncs.UserData.UpdateRichPresence(PresenceState, stringBuffer, "logo", PresenceStartTime, 0);
        }
        else {
            GameLinker::ServiceFuncs.UserData.UpdateRichPresence("", "", "logo", PresenceStartTime, 0);
        }
    }
    void ReflectProjectNameChange() {
        if (CurrentProject == NULL) {
            SetTitle("HatchStudio");
        }
        else {
            char stringBuffer[256];
            sprintf(stringBuffer, "%s - HatchStudio", CurrentProject->ProjectName);
            SetTitle(stringBuffer);
        }

        UpdatePresence();
    }
    void ReflectCurrentFileEditorChange() {
		char menuItemTitle[128];
        char currentFilename[128];

        menuFile->EditItem(menuIndex_SaveAllFile, "Save All", Action_SaveAllResources, SHORTCUT_SAVE_ALL, MainForm->Editors.Count() > 0);
        menuFile->EditItem(menuIndex_CloseAllFiles, "Close All", Action_CloseAllResources, SHORTCUT_CLOSE_ALL, MainForm->Editors.Count() > 0);

        if (CurrentProject != NULL) {
            int index = MainForm->MainTabControl->SelectedIndex;
            if (index >= 0 && index < MainForm->Editors.Count()) {
                String* title = &Editors[index]->Title;
                if (title->Length > 0) {
                    Strings::ToCString(currentFilename, title);

                    sprintf(menuItemTitle, "Save %s", currentFilename);
                    menuFile->EditItem(menuIndex_SaveFile, menuItemTitle, Action_SaveResource, SHORTCUT_SAVE_FILE, true);

                    sprintf(menuItemTitle, "Save %s As...", currentFilename);
                    menuFile->EditItem(menuIndex_SaveFileAs, menuItemTitle, Action_SaveResourceAs, SHORTCUT_SAVE_FILE_AS, true);

                    sprintf(menuItemTitle, "Close %s", currentFilename);
                    menuFile->EditItem(menuIndex_CloseFile, menuItemTitle, Action_CloseResource, SHORTCUT_CLOSE_FILE, true);
                    return;
                }
            }
        }

        menuFile->EditItem(menuIndex_SaveFile, "Save", Action_SaveResource, SHORTCUT_SAVE_FILE, false);
        menuFile->EditItem(menuIndex_SaveFileAs, "Save As...", Action_SaveResourceAs, SHORTCUT_SAVE_FILE_AS, false);
        menuFile->EditItem(menuIndex_CloseFile, "Close", Action_CloseResource, SHORTCUT_CLOSE_FILE, false);
    }
    void UpdateRecentProjectsMenu() {
        menuRecentProjects->ClearItems();

        if (Preferences->RecentProjects.Count()) {
            for (int i = 0; i < Preferences->RecentProjects.Count(); i++) {
                menuRecentProjects->AddItem(Preferences->RecentProjects[i].Name, NULL, UI::Menu::SM_NONE, true);
            }
            menuRecentProjects->AddSeparator();
        }
        menuRecentProjects->AddItem("Clear Recent Projects", Action_ClearRecentProjects, UI::Menu::SM_NONE, true);
    }
    void ClearRecentProjects() {
        Preferences->RecentProjects.Clear();
        UpdateRecentProjectsMenu();
    }

    static char* FromLiteral(const char* str) {
        size_t len = strlen(str);
        char* buf = (char*)malloc(len + 1);
        if (!buf)
            return NULL;
        memcpy(buf, str, len);
        buf[len] = 0;
        return buf;
    }

    void InitProject() {

    }
    void NewProject() {
        Form_NewProjectWizard* dialog = new Form_NewProjectWizard();
        dialog->BackColor = BackColor;

        UI::System::Application::ShowDialog(dialog, [this, dialog](DialogResult result) -> void {
            if (result == DialogResult::OK) {
                char stringBuf[256];
                auto oldProject = CurrentProject;

                CurrentProject = new HatchProject();

                Strings::ToCString(stringBuf, &dialog->textBoxProjectName->Text);
                CurrentProject->ProjectName = FromLiteral(stringBuf);

                CurrentProject->EngineVersion = dialog->comboBoxEngineVersion->SelectedIndex;

                Strings::ToCString(stringBuf, &dialog->textBoxShortName->Text);
                CurrentProject->ShortName = FromLiteral(stringBuf);

                UI::SystemDialog::SaveFileData sfd;
                sfd.Title = "Select a destination for the project file...";
                // sfd.InitialDirectory = MainForm->CurrentProjectFolderPath;
                sfd.FilterPatterns.Add("*.HPROJ");

                if (UI::SystemDialog::SaveFile(&sfd)) {
                    const char* filepath = sfd.Filename;

                    CurrentProjectFilePath = FromLiteral(filepath);
                    if (CurrentProjectFilePath) {
                        CurrentProjectFilePath = UI::Filesystem::Paths::SanitizePath(CurrentProjectFilePath);
                        CurrentProjectFolderPath = UI::Filesystem::Paths::GetEnclosingFolder(new char[256], filepath);

                        SaveProject(filepath);

                        Preferences->RecentProjects.Insert(0, RecentProject { FromLiteral(CurrentProject->ProjectName), CurrentProjectFilePath });
                        UpdateRecentProjectsMenu();
                    }
                }
                else {
                    delete CurrentProject;
                    CurrentProject = oldProject;
                }

                ReflectProjectNameChange();
            }
        });
    }
    bool OpenProject(const char* filepath) {
        Stream* stream;
        auto oldProj = CurrentProject;

        stream = FileStream::New(filepath, FileStream::READ_ACCESS);
        if (!stream)
            goto FailAndFree;

        CurrentProject = new HatchProject();
        CurrentProject->Read(stream);
        if (false)
            goto FailAndFree;

        stream->Close();
        stream = NULL;

        // Success
        CurrentProjectFilePath = FromLiteral(filepath);
        if (!CurrentProjectFilePath)
            goto FailAndFree;

        CurrentProjectFilePath = UI::Filesystem::Paths::SanitizePath(CurrentProjectFilePath);
        CurrentProjectFolderPath = UI::Filesystem::Paths::GetEnclosingFolder(new char[256], filepath);

        // Link the game logic before we even open any files
        ::GameLinker::Load(CurrentProjectFolderPath);

        for (int i = 0; i < CurrentProject->LastOpenFiles.Count(); i++) {
            OpenFile(CurrentProject->LastOpenFiles[i]);
        }

        ReflectProjectNameChange();

        {
            bool alreadyExists = false;
            for (int i = 0; i < Preferences->RecentProjects.Count(); i++) {
                auto rp = Preferences->RecentProjects[i];
                if (strcmp(rp.Filepath, CurrentProjectFilePath) == 0) {
                    Preferences->RecentProjects.RemoveAt(i);
                    Preferences->RecentProjects.Insert(0, rp);
                    alreadyExists = true;
                    break;
                }
            }
            if (!alreadyExists)
                Preferences->RecentProjects.Insert(0, RecentProject { FromLiteral(CurrentProject->ProjectName), CurrentProjectFilePath });
        }


        UpdateRecentProjectsMenu();
        return true;

    FailAndFree:
        if (CurrentProject) {
            delete CurrentProject;
            CurrentProject = NULL;
        }
        if (stream) {
            stream->Close();
        }
        CurrentProject = oldProj;
        return false;
    }
    void SaveProject(const char* filepath) {
        Stream* stream;

        stream = FileStream::New(filepath, FileStream::WRITE_ACCESS);
        if (!stream)
            goto FailAndFree;

        CurrentProject->Write(stream);
        if (false)
            goto FailAndFree;

        stream->Close();
        stream = NULL;

        // Success
        return;

    FailAndFree:
        if (stream) {
            stream->Close();
        }
    }
    bool CloseCurrentProject() {
        char stringBuffer[256];
        if (CurrentProject == NULL)
            return true;

        // Free old open file list, add current open files to it
        for (int i = 0; i < CurrentProject->LastOpenFiles.Count(); i++)
            free(CurrentProject->LastOpenFiles[i]);
        CurrentProject->LastOpenFiles.Clear();

        for (int i = 0; i < Editors.Count(); i++) {
            Strings::ToCString(stringBuffer, &Editors[i]->FilePath);
            CurrentProject->LastOpenFiles.Add(FromLiteral(stringBuffer));
        }

        // Try to close each file
        for (int i = 0; i < Editors.Count(); i++) {
            if (!Editors[i]->CloseFile()) {
                return false;
            }
            delete Editors[i];
        }
        Editors.Clear();
        MainTabControl->TabPages.Clear();

        SaveProject(CurrentProjectFilePath);
        delete CurrentProject;
        CurrentProject = NULL;

        ReflectProjectNameChange();
        ReflectCurrentFileEditorChange();
        return true;
    }

    void NewFile() {
        ResourceEditor* editor = new SceneEditor();
        editor->New();
        Editors.Insert(0, editor);
        MainTabControl->TabPages.Insert(0, editor);

        MainTabControl->Select(0);
    }
    bool OpenFile(const char* filepath) {
        char resourceFolder[1024];
        sprintf(resourceFolder, "%s/Resources", CurrentProjectFolderPath);

        size_t parentPathLen = strlen(resourceFolder);
        if (strncmp(filepath, resourceFolder, parentPathLen) != 0 || filepath[parentPathLen] != '/') {
            // TODO: Make this a UI::Form dialog
            const SDL_MessageBoxButtonData buttons[] = {
                { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "OK" },
            };
            const SDL_MessageBoxData messageboxdata = {
                SDL_MESSAGEBOX_INFORMATION, UI::Graphics::Renderer::Window,
                "Non-Project Resource",
                "Resource must be in project's Resource folder.",
                SDL_arraysize(buttons), buttons, NULL
            };

            int buttonid;
            if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) {
                SDL_Log("error displaying message box");
                return false;
            }
            return false;
        }

        ResourceEditor* editor = new SceneEditor();
        if (!editor->Open(filepath)) {
            delete editor;
            return false;
        }
        Editors.Insert(0, editor);
        MainTabControl->TabPages.Insert(0, editor);

        MainTabControl->Select(0);
        return true;
    }

    void OnTabChange(void* sender, EventArgs* e) {
        int editorType = Editors[MainTabControl->SelectedIndex]->GetEditorType();

        switch (editorType) {
        case EditorTypes::SCENE:
            PresenceState = "Editing a scene";
            ReflectCurrentFileEditorChange();
            UpdatePresence();
            break;
        }
    }

    void Load() {
        Form::Load();
        MainForm = this;

        BackColor = Color(0x21252B, 0xFF);

        MainTabControl = new TabControl();
        MainTabControl->Dock = DOCK_FILL;
        MainTabControl->SelectedIndex = 0;
        MainTabControl->Alignment = TabAlignment::Top;
        MainTabControl->MaxSize = 200;
        MainTabControl->onSelected += std::bind(&HatchStudioForm::OnTabChange, this, std::placeholders::_1, std::placeholders::_2);
        Controls.Add(MainTabControl);

#ifndef USE_NATIVE_MENU
        MenuBarControl = new MenuBar();
        MenuBarControl->Size = { Size.Get().W, MenuBarControl->ItemHeight };
        Controls.Add(MenuBarControl);
#endif

        SDL_Rect displayBounds;
        if (SDL_GetDisplayBounds(0, &displayBounds) == 0) {
            // success
        }

        ::Size startSize = { 1280, 720 };
		startSize.W = M_MIN(startSize.W, displayBounds.w - 40);
		startSize.H = M_MIN(startSize.H, displayBounds.h * 3 / 4);

        // Load settings
        Preferences = new HatchStudioSettings();
        if (LoadSettings()) {
            startSize.W = Preferences->WindowWidth;
            startSize.H = Preferences->WindowHeight;
        }

        // Set up the menu
        try {
            MenuSetup();
        }
        catch (const char* err) {
            fprintf(stderr, "Couldn't setup menu: %s\n", err);
            exit(EXIT_FAILURE);
        }

        // Resize window
        Size = startSize;
        SDL_SetWindowSize(UI::Graphics::Renderer::Window, startSize.W, startSize.H);
        SDL_SetWindowPosition(UI::Graphics::Renderer::Window, SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0));
        if (Preferences->Maximized)
            SDL_MaximizeWindow(UI::Graphics::Renderer::Window);

        // Set up presence timer
        PresenceStartTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        Studio::ResourcePathPrefix = &CurrentProjectFolderPath;

        // Reopen last project if desired
        if (Preferences->ReopenLastProject && Preferences->RecentProjects.Count() > 0) {
            OpenProject(Preferences->RecentProjects[0].Filepath);
        }
        else {
            ReflectProjectNameChange();
            ReflectCurrentFileEditorChange();
        }

        SDL_ShowWindow(UI::Graphics::Renderer::Window);
    }

    void HandleSDLEvent(SDL_Event* e) {
        switch (e->type) {
        case SDL_KEYDOWN:
            CheckShortcuts(e->key.keysym.sym, (SDL_Keymod)e->key.keysym.mod);
            break;

        case SDL_WINDOWEVENT:
            switch (e->window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                Size = { e->window.data1, e->window.data2 };
                OnResized(NULL);
                break;
            }
            break;
        }

        // Handle the menu bar control first
        MenuBarControl->HandleSDLEvent(e);

        if (MenuBarControl->Dropdown == NULL) {
            // If a dropdown is up then don't handle the other controls
            for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
                if (Controls.Items[i] != MenuBarControl) {
                    Controls.Items[i]->HandleSDLEvent(e);
                }
            }
        }
    }
};

HatchStudioForm* HatchStudioForm::MainForm = NULL;

int main(int argc, char** args) {
    // Handle options here
    bool option_PackAssets = false;
    char option_AssetFolder[256] = { 0 };
    char option_AssetFilePath[256] = { 0 };
    while (true) {
        switch (UI::System::Application::ParseOptions(argc, args, "p:i:o:h")) {
        case 'p':
            option_PackAssets = true;
            continue;

        case 'i':
            strncpy(option_AssetFolder, UI::System::Application::optarg, 255);
            continue;

        case 'o':
            strncpy(option_AssetFilePath, UI::System::Application::optarg, 255);
            continue;

        case '?':
        case 'h':
        default:
            printf("Command Line Help Guide:\n");
            printf("-h                   | Display this help message.\n");
            printf("-p [version]         | Starts the packing program. Follow the option with a 'G' for Hatch Game Engine, or 'L' for HatchLite.\n");
            printf("-i [resourceFolder]  | Sets the Resources folder to pack.\n");
            printf("-o [outputPath]      | Sets the output file path for the packer.\n");
            break;

        case -1:
            break;
        }
        break;
    }

    if (option_PackAssets) {
        if (option_AssetFolder[0] == '\0') {
            fprintf(stderr, "Resources folder not specified. Use -h for help.\n");
            return 0;
        }
        if (option_AssetFilePath[0] == '\0') {
            fprintf(stderr, "Asset pack output file path not specified. Use -h for help.\n");
            return 0;
        }
        return 0;
    }

    UI::System::Application::Start(argc, args, new HatchStudioForm());
	return 0;
}

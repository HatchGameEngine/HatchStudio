#pragma once

#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/IO/Stream.h>
#include <Hatch/IO/FileStream.h>

#include <Studio/EditableTileConfig.hpp>
#include <Studio/Stamp.hpp>

#define MAGIC_TILESET_RSDK 0x004C4954
#define MAGIC_TILESET_HATCH 0x4C4F4354
#define MAGIC_TILESET_HATCHLITE 0x4C4F4348

struct StageTileset {
    struct TileImageHash {
        Uint32 FLIP_NONE;
    };

    const int HATCH_TILESIZE = TILE_SIZE;
    const int HATCH_TILESHEET_ROWSIZE = 64;
    const int HATCH_TILESHEET_COLSIZE = 64;
    const int HATCH_TILESHEET_WIDTH = HATCH_TILESHEET_ROWSIZE * HATCH_TILESIZE;
    const int HATCH_TILESHEET_HEIGHT = HATCH_TILESHEET_COLSIZE * HATCH_TILESIZE;

    int TileCount = 0;
    SDL_Texture* TileImageTexture = NULL;
    SDL_Texture* TileCollisionTextures[2];

    Uint32*      TileImagePixelData = NULL;

    EditableTileConfig TileCfg[2][0x1000 << 2]; // [planeIndex][FlipXY | TileID]
    TileImageHash TileHashes[0x1000];

    int TileRemapArray[0x1000];

    // RSDK Max Tile Count: 0x400 (1024)
    // HatchLite (Prospective) Max Tile Count: 0x1000 (4096)

    StageTileset();
    ~StageTileset();

    bool Import(List<char*>& filenames, ArrayList<SavedStamp*>* stampsList = NULL);
    bool Import(CString filename, ArrayList<SavedStamp*>* stampsList = NULL);
    bool Save(CString filename);

    bool UpdateTileCollisionTexture_All();
    bool UpdateTileCollisionTexture(int plane, int tileID);

    bool LoadTileset_RSDK(CString filename);

    bool ReadTileConfig_RSDK(Stream* stream);
    bool ReadTileConfig_Hatch(Stream* stream);
    bool ReadTileConfig_HatchLite(Stream* stream);
    bool OpenTileConfig(CString filename);

    bool WriteTileConfig_Hatch(Stream* stream);
    bool SaveTileConfig(CString filename);

    void RemapTileConfig();
};

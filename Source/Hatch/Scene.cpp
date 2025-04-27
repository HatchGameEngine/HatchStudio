#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Scene.h>

#include <Hatch/Hashing/MD5.h>
#include <Hatch/ImageFormats/GIF.h>
#include <Hatch/IO/ResourceStream.h>

#include <Hatch/Audio.h>
#include <Hatch/Classes.h>
#include <Hatch/Clock.h>
#include <Hatch/Collision.h>
#include <Hatch/Game.h>
#include <Hatch/GameLinker.h>
#include <Hatch/Graphics.h>
#include <Hatch/Math.h>
#include <Hatch/Memory.h>
#include <Hatch/Resources.h>
#include <Hatch/Strings.h>

#define MEASURE_PERF 1

namespace Scene {
    ::UpdateBounds UpdateBounds[MAX_VIEWPORTS];
    int            UpdateBoundCount = 0;

    char    CurrentStage[16];
    bool    ReloadStage = false;

    Layer   static_Layers[MAX_LAYERS];
    Uint8   static_TileImageData[MAX_TILE_COUNT * TILE_SIZE * TILE_SIZE * 4];
    Uint16  static_ClassIndexList[MAX_CLASSES];
    EntitySlot static_EntitySlots[MAX_ENTITIES];

    Layer*  Layers = static_Layers;
    Uint8*  TileImageData = static_TileImageData;
    Uint16* ClassIndexList = static_ClassIndexList;
    Uint32  ClassIndexCount = 0;

    Uint32  Frame;

    Entity* CurrentEntity;
    EntitySlot* EntitySlots = static_EntitySlots;
    StageClassSlotList ClassSlotLists[MAX_CLASS_SLOTLISTS];

    Uint16  SearchStack[0x10];
    Uint16* SearchStackTop;

    Uint16  GlobalClassIndexList[MAX_CLASSES];
    Uint32  GlobalClassIndexCount = 0;

    Pixel GameConfigPalette[MAX_PALETTE_COUNT][0x100];
    int UsedGameConfigPaletteLines[MAX_PALETTE_COUNT];
    Pixel StageConfigPalette[MAX_PALETTE_COUNT][0x100];
    int UsedStageConfigPaletteLines[MAX_PALETTE_COUNT];

    struct TileConfigData {
        Uint8 Collision[16];
        Uint8 HasCollision[16];
        Uint8 Orientation;
        Uint8 Angle[4];
        Uint8 Behavior;
    };
    TileConfigData temp[2][0x400];

    void LoadStageConfig(const char* filename) {
        #ifdef MEASURE_PERF
        Clock::Counter counter;
        #endif

        Class* objectClass;
        StaticObject** staticObjectPtr;

        ClassIndexCount = 0;
        ClassIndexList[ClassIndexCount++] = 0; // Default object

        char bufferString[256];
        bufferString[0] = 0;
        if (Game::OverridenStartScene[0]) {
            strcat(bufferString, Game::OverridenStartScene);

            char* lastSep = strrchr(bufferString, '/');
            if (lastSep) {
                *lastSep = '\0';
                strcat(bufferString, "/StageConfig.bin");
            }
        }
        else {
            strcat(bufferString, "Stages/");
            strcat(bufferString, Game::State.Scenes[Game::State.CurrentSceneIndex].Zone);
            strcat(bufferString, "/StageConfig.bin");
        }

        Stream* stream = ResourceStream::New(bufferString);
        if (stream) {
            char streamStringBuffer[256];
            Uint32 magic = stream->ReadUInt32();
            if (magic == 0x00474643) {
                // Add global classes (if desired)
                bool useGlobalClasses = stream->ReadByte();
                if (useGlobalClasses) {
                    int classIndexStart = ClassIndexCount;

                    for (Uint32 i = 0; i < GlobalClassIndexCount; i++) {
                        ClassIndexList[ClassIndexCount++] = GlobalClassIndexList[i];
                    }

                    #ifdef MEASURE_PERF
                        Clock::CounterStart(&counter);
                    #endif

                    // Init Static Objects for global classes
                    for (Uint32 i = classIndexStart; i < ClassIndexCount; i++) {
                        objectClass = &GameLinker::ClassList[ClassIndexList[i]];

                        Memory::Alloc(objectClass->StaticObjectPtr, objectClass->StaticObjectSize, Memory::MEMPOOL_STAGE, false);

                        staticObjectPtr = (StaticObject**)objectClass->StaticObjectPtr;
                        if (*staticObjectPtr) {
                            if (objectClass->onStaticConstructor) {
                                objectClass->onStaticConstructor(*staticObjectPtr);
                                (*staticObjectPtr)->StageClassID = i;
                                (*staticObjectPtr)->UpdateFlag = 0;
                            }
                        }
                    }

                    #ifdef MEASURE_PERF
                        Clock::CounterFinish(&counter);
                        printf("Static Objects (Global) took %.1f ms\n", Clock::CounterGetElapsed(&counter));
                    #endif
                }

                #ifdef MEASURE_PERF
                    Clock::CounterStart(&counter);
                #endif

                // Add Stage Classes
                int stageClassIndexStart = ClassIndexCount;
                int stageClassCount = stream->ReadByte();
                for (int i = 0; i < stageClassCount; i++) {
                    stream->ReadHeaderedString(streamStringBuffer);
                    Hash  stageClassHash = MD5_HashString(streamStringBuffer);

                    auto oldClassCount = Scene::ClassIndexCount;
                    for (int c = 0; c < GameLinker::ClassCount; c++) {
                        if (stageClassHash == GameLinker::ClassList[c].Name) {
                            ClassIndexList[ClassIndexCount++] = c;
                            break;
                        }
                    }

                    if (oldClassCount == Scene::ClassIndexCount) {
                        printf("Could not find logic for class '%s'.\n", streamStringBuffer);
                    }
                }

                #ifdef MEASURE_PERF
                    Clock::CounterFinish(&counter);
                    printf("Name Hashing (Stage) took %.1f ms\n", Clock::CounterGetElapsed(&counter));
                #endif

                #ifdef MEASURE_PERF
                    Clock::CounterStart(&counter);
                #endif

                // Init Static Objects for stage classes
                for (Uint32 i = stageClassIndexStart; i < ClassIndexCount; i++) {
                    objectClass = &GameLinker::ClassList[ClassIndexList[i]];

                    Memory::Alloc(objectClass->StaticObjectPtr, objectClass->StaticObjectSize, Memory::MEMPOOL_STAGE, false);

                    staticObjectPtr = (StaticObject**)objectClass->StaticObjectPtr;
                    if (*staticObjectPtr) {
                        if (objectClass->onStaticConstructor) {
                            objectClass->onStaticConstructor(*staticObjectPtr);
                            (*staticObjectPtr)->StageClassID = i;
                            (*staticObjectPtr)->UpdateFlag = 0;
                        }
                    }
                }

                #ifdef MEASURE_PERF
                    Clock::CounterFinish(&counter);
                    printf("Static Objects (Stage) took %.1f ms\n", Clock::CounterGetElapsed(&counter));
                #endif

                #ifdef MEASURE_PERF
                    Clock::CounterStart(&counter);
                #endif

                // Load palettes
                Color color;
                for (int i = 0; i < MAX_PALETTE_COUNT; i++) {
                    // Palette Set
                    int bitmap = UsedStageConfigPaletteLines[i] = stream->ReadUInt16();
                    for (int paletteLine = 0; paletteLine < 16; paletteLine++) {
                        if ((bitmap & (1 << paletteLine)) != 0) {
                            for (int d = 0; d < 16; d++) {
                                color.R = stream->ReadByte();
                                color.G = stream->ReadByte();
                                color.B = stream->ReadByte();

                                Scene::StageConfigPalette[i][(paletteLine << 4) | d] = color;
                            }
                        }
                    }
                }

                #ifdef MEASURE_PERF
                    Clock::CounterFinish(&counter);
                    printf("Loading Palettes (Stage) took %.1f ms\n", Clock::CounterGetElapsed(&counter));
                #endif

                #ifdef MEASURE_PERF
                    Clock::CounterStart(&counter);
                #endif

                // Load sound effects
                int wavConfigCount = stream->ReadByte();
                for (int i = 0; i < wavConfigCount; i++) {
                    stream->ReadHeaderedString(streamStringBuffer);
                    Audio::LoadSoundFX(streamStringBuffer, stream->ReadByte(), UNLOAD_STAGE_END);
                }

                #ifdef MEASURE_PERF
                    Clock::CounterFinish(&counter);
                    printf("Loading Sounds (Stage) took %.1f ms\n", Clock::CounterGetElapsed(&counter));
                #endif
            }
            else {
                fprintf(stderr, "Invalid magic for file '%s'!\n", filename);
            }
            stream->Close();
        }
        else {
            fprintf(stderr, "Could not open '%s'!\n", bufferString);
        }
    }
    void LoadTileConfig(const char* filename) {
        char bufferString[256];
        bufferString[0] = 0;
        if (Game::OverridenStartScene[0]) {
            strcat(bufferString, Game::OverridenStartScene);

            char* lastSep = strrchr(bufferString, '/');
            if (lastSep) {
                *lastSep = '\0';
                strcat(bufferString, "/TileConfig.bin");
            }
        }
        else {
            strcat(bufferString, "Stages/");
            strcat(bufferString, Game::State.Scenes[Game::State.CurrentSceneIndex].Zone);
            strcat(bufferString, "/TileConfig.bin");
        }

        Stream* stream = ResourceStream::New(bufferString);
        if (stream) {
            Uint32 magic = stream->ReadUInt32();
            if (magic == 0x004C4954) {
                stream->ReadCompressed(&temp);

                const int RSDK_MAX_TILE_COUNT = 0x400;

                for (size_t p = 0; p < 2; p++) {
                    for (size_t i = 0; i < RSDK_MAX_TILE_COUNT; i++) {
                        TileConfig* tileConfig = &Collision::TileCfg[p][i];
                        TileConfigData* tileConfigData = &temp[p][i];

                        Uint8* col;
                        // Interpret up/down collision
                        if (tileConfigData->Orientation) {
                            col = &tileConfigData->Collision[0];
                            for (int c = 0; c < 16; c++) {
                                if (tileConfigData->HasCollision[c]) {
                                    tileConfig->CollisionTop[c] = 0;
                                    tileConfig->CollisionBottom[c] = *col;
                                }
                                else {
                                    tileConfig->CollisionTop[c] =
                                        tileConfig->CollisionBottom[c] = -1;
                                }
                                col++;
                            }

                            // Interpret left/right collision
                            for (int y = 15; y >= 0; y--) {
                                // Left-to-right check
                                for (int x = 0; x <= 15; x++) {
                                    Uint8 data = tileConfig->CollisionBottom[x];
                                    if (data != 0xFF && data >= y) {
                                        tileConfig->CollisionLeft[y] = x;
                                        goto COLLISION_LINE_LEFT_BOTTOMUP_FOUND;
                                    }
                                }
                                tileConfig->CollisionLeft[y] = -1;

                            COLLISION_LINE_LEFT_BOTTOMUP_FOUND:

                                // Right-to-left check
                                for (int x = 15; x >= 0; x--) {
                                    Uint8 data = tileConfig->CollisionBottom[x];
                                    if (data != 0xFF && data >= y) {
                                        tileConfig->CollisionRight[y] = x;
                                        goto COLLISION_LINE_RIGHT_BOTTOMUP_FOUND;
                                    }
                                }
                                tileConfig->CollisionRight[y] = -1;

                            COLLISION_LINE_RIGHT_BOTTOMUP_FOUND:
                                ;
                            }
                        }
                        else {
                            col = &tileConfigData->Collision[0];
                            for (int c = 0; c < 16; c++) {
                                if (tileConfigData->HasCollision[c]) {
                                    tileConfig->CollisionTop[c] = *col;
                                    tileConfig->CollisionBottom[c] = 15;
                                }
                                else {
                                    tileConfig->CollisionTop[c] =
                                        tileConfig->CollisionBottom[c] = -1;
                                }
                                col++;
                            }

                            // Interpret left/right collision
                            for (int y = 0; y <= 15; y++) {
                                // Left-to-right check
                                for (int x = 0; x <= 15; x++) {
                                    Uint8 data = tileConfig->CollisionTop[x];
                                    if (data != 0xFF && data <= y) {
                                        tileConfig->CollisionLeft[y] = x;
                                        goto COLLISION_LINE_LEFT_TOPDOWN_FOUND;
                                    }
                                }
                                tileConfig->CollisionLeft[y] = -1;

                            COLLISION_LINE_LEFT_TOPDOWN_FOUND:

                                // Right-to-left check
                                for (int x = 15; x >= 0; x--) {
                                    Uint8 data = tileConfig->CollisionTop[x];
                                    if (data != 0xFF && data <= y) {
                                        tileConfig->CollisionRight[y] = x;
                                        goto COLLISION_LINE_RIGHT_TOPDOWN_FOUND;
                                    }
                                }
                                tileConfig->CollisionRight[y] = -1;

                            COLLISION_LINE_RIGHT_TOPDOWN_FOUND:
                                ;
                            }
                        }

                        tileConfig->Behavior = tileConfigData->Behavior;
                        memcpy(&tileConfig->AngleTop, &tileConfigData->Angle, 4);
                    }
                }

                TileConfig* tileDest;
                TileConfig* tileLast;
                for (size_t p = 0; p < 2; p++) {
                    for (size_t i = 0; i < RSDK_MAX_TILE_COUNT; i++) {
                        TileConfig* tile = &Collision::TileCfg[p][i];
                        // Flip X
                        tileDest = tile + MAX_TILE_COUNT;
                        tileDest->AngleTop = -tile->AngleTop;
                        tileDest->AngleLeft = -tile->AngleRight;
                        tileDest->AngleRight = -tile->AngleLeft;
                        tileDest->AngleBottom = -tile->AngleBottom;
                        tileDest->Behavior = tile->Behavior;
                        for (int xD = 0, xS = 15; xD <= 15; xD++, xS--) {
                            tileDest->CollisionTop[xD] = tile->CollisionTop[xS];
                            tileDest->CollisionBottom[xD] = tile->CollisionBottom[xS];
                            // Swaps
                            tileDest->CollisionLeft[xD] = tile->CollisionRight[xD] ^ 15;
                            tileDest->CollisionRight[xD] = tile->CollisionLeft[xD] ^ 15;
                        }
                        // Flip Y
                        tileDest = tile + (MAX_TILE_COUNT * 2);
                        tileDest->AngleTop = 0x80 - tile->AngleBottom;
                        tileDest->AngleLeft = 0x80 - tile->AngleLeft;
                        tileDest->AngleRight = 0x80 - tile->AngleRight;
                        tileDest->AngleBottom = 0x80 - tile->AngleTop;
                        tileDest->Behavior = tile->Behavior;
                        for (int xD = 0, xS = 15; xD <= 15; xD++, xS--) {
                            tileDest->CollisionLeft[xD] = tile->CollisionLeft[xS];
                            tileDest->CollisionRight[xD] = tile->CollisionRight[xS];
                            // Swaps
                            tileDest->CollisionTop[xD] = tile->CollisionBottom[xD] ^ 15;
                            tileDest->CollisionBottom[xD] = tile->CollisionTop[xD] ^ 15;
                        }
                        // Flip XY
                        tileLast = tileDest;
                        tileDest = tile + (MAX_TILE_COUNT * 3);
                        tileDest->AngleTop = -tileLast->AngleTop;
                        tileDest->AngleLeft = -tileLast->AngleRight;
                        tileDest->AngleRight = -tileLast->AngleLeft;
                        tileDest->AngleBottom = -tileLast->AngleBottom;
                        tileDest->Behavior = tileLast->Behavior;
                        for (int xD = 0, xS = 15; xD <= 15; xD++, xS--) {
                            tileDest->CollisionTop[xD] = tile->CollisionBottom[xS] ^ 15;
                            tileDest->CollisionLeft[xD] = tile->CollisionRight[xS] ^ 15;
                            tileDest->CollisionRight[xD] = tile->CollisionLeft[xS] ^ 15;
                            tileDest->CollisionBottom[xD] = tile->CollisionTop[xS] ^ 15;
                        }
                    }
                }


            }
            else {
                fprintf(stderr, "Invalid magic for file '%s'!\n", filename);
            }
            stream->Close();
        }
        else {
            fprintf(stderr, "Could not open '%s'!\n", bufferString);
        }
    }
    void LoadStage(const char* filename) {
        #ifdef MEASURE_PERF
        Clock::Counter counter;
        #endif

        Game::State.TimerTicks = 0;
        Game::State.TimerCentiseconds = 0;
        Game::State.TimerSeconds = 0;
        Game::State.TimerMinutes = 0;

        // Graphics::FilterTable = NULL;

        // Clear DrawGroup entity lists
        for (int d = 0; d < MAX_DRAWGROUPS; d++)
            Graphics::DrawGroups[d].EntityCount = 0;

        // Clear Stage Class entity lists
        for (int i = 0; i < MAX_CLASS_SLOTLISTS; i++)
            ClassSlotLists[i].SlotCount = 0;

        // If stage has not changed AND we're not telling the engine to reload the stage,
        if (strcmp(CurrentStage, Game::State.Scenes[Game::State.CurrentSceneIndex].Zone) == 0 && !ReloadStage) {
            // Clear Scene Layers, and that's it
            for (Uint32 i = 0; i < MAX_LAYERS; i++) {
                Layers[i] = Layer();
                for (int v = 0; v < MAX_VIEWPORTS; v++)
                    Layers[i].Hidden[v] = true;
            }

            Memory::RunGC(Memory::MEMPOOL_STAGE);
            return;
        }

        // Clear Layers
        for (Uint32 i = 0; i < MAX_LAYERS; i++) {
            Layers[i] = Layer();
            for (int v = 0; v < MAX_VIEWPORTS; v++)
                Layers[i].Hidden[v] = true;
        }

        // Unload Meshes of UNLOAD_STAGE_END
        for (int index = 0; index < MAX_MESHES; index++) {
            Resources::ResMesh* resource = &Resources::ResourceMeshes[index];
            if (resource->UnloadPolicy == UNLOAD_STAGE_END) {
                memset(&resource->MeshData, 0, sizeof(resource->MeshData));
                resource->UnloadPolicy = 0;
            }
        }

        // Unload 3D Views of UNLOAD_STAGE_END
        for (int index = 0; index < MAX_ARRAY_BUFFERS; index++) {
            Resources::ResView3D* resource = &Resources::ResourceView3Ds[index];
            if (resource->UnloadPolicy == UNLOAD_STAGE_END) {
                memset(&resource->View3DData, 0, sizeof(resource->View3DData));
                resource->UnloadPolicy = 0;
            }
        }

        // Unload Animations of UNLOAD_STAGE_END
        for (int index = 0; index < MAX_SPRITES; index++) {
            Resources::ResSprite* resource = &Resources::ResourceSprites[index];
            if (resource->UnloadPolicy == UNLOAD_STAGE_END) {
                memset(&resource->SpriteData, 0, sizeof(resource->SpriteData));
                resource->UnloadPolicy = 0;
            }
        }

        // Unload Images of UNLOAD_STAGE_END
        for (int index = 0; index < MAX_IMAGES; index++) {
            Resources::ResImage* resource = &Resources::ResourceImages[index];
            if (resource->UnloadPolicy == UNLOAD_STAGE_END) {
                // printf("bye %d\n", index);
                memset(&resource->ImageData, 0, sizeof(resource->ImageData));
                resource->UnloadPolicy = 0;
            }
        }

        // Stop all SFX and Unload SoundFX of UNLOAD_STAGE_END
        if (Audio::Lock()) {
            Audio::AudioPlayback* playback = Audio::Playbacks;
            for (int i = 0; i < MAX_AUDIO_PLAYBACKS; i++) {
                if (playback->State == Audio::PLAYBACK_SOUNDFX || playback->State == Audio::PLAYBACK_SOUNDFX_PAUSED) {
                    playback->State = Audio::PLAYBACK_NONE;
                    playback->Index = -1;
                }
                playback++;
            }

            for (int index = 0; index < MAX_SOUNDS; index++) {
                Resources::ResSound* resource = &Resources::ResourceSounds[index];
                if (resource->UnloadPolicy == UNLOAD_STAGE_END) {
                    memset(&resource->SoundData, 0, sizeof(resource->SoundData));
                    resource->UnloadPolicy = 0;
                }
            }
            Audio::Unlock();
        }

        // Clear all Static object pointers
        for (Uint32 i = 0; i < ClassIndexCount; i++) {
            *GameLinker::ClassList[ClassIndexList[i]].StaticObjectPtr = NULL;
        }

        // Clear DrawGroup prefix function and sort flag
        for (int d = 0; d < MAX_DRAWGROUPS; d++) {
            Graphics::DrawGroups[d].EntityDepthSortingEnabled = 0;
            Graphics::DrawGroups[d].PrefixFunction = NULL;
        }

        // Run cleanups
        Memory::RunGC(Memory::MEMPOOL_STAGE);
        Memory::RunGC(Memory::MEMPOOL_SOUND);

        // Reset views
        for (Uint32 v = 0; v < MAX_VIEWPORTS; v++) {
            View* view = &Graphics::Views[v];
            view->X = 0;
            view->Y = 0;
        }

        // Set CurrentStage
        strncpy(CurrentStage, Game::State.Scenes[Game::State.CurrentSceneIndex].Zone, 16);
        ReloadStage = false;

        Game::State.SceneFilter = Game::State.Scenes[Game::State.CurrentSceneIndex].Filter;

        // Load TileConfig
        #ifdef MEASURE_PERF
            Clock::CounterStart(&counter);
        #endif

        LoadTileConfig(filename);

        #ifdef MEASURE_PERF
            Clock::CounterFinish(&counter);
            printf("LoadTileConfig took %.1f ms\n", Clock::CounterGetElapsed(&counter));
        #endif

        // Load StageConfig
        #ifdef MEASURE_PERF
            Clock::CounterStart(&counter);
        #endif

        LoadStageConfig(filename);

        #ifdef MEASURE_PERF
            Clock::CounterFinish(&counter);
            printf("LoadStageConfig took %.1f ms\n", Clock::CounterGetElapsed(&counter));
        #endif

        // Load Tileset.

        // Copy over tile data.
        char bufferString[256];
        bufferString[0] = 0;
        if (Game::OverridenStartScene[0]) {
            strcat(bufferString, Game::OverridenStartScene);
            
            char* lastSep = strrchr(bufferString, '/');
            if (lastSep) {
                *lastSep = '\0';
                strcat(bufferString, "/16x16Tiles.gif");
            }
        }
        else {
            strcat(bufferString, "Stages/");
            strcat(bufferString, Game::State.Scenes[Game::State.CurrentSceneIndex].Zone);
            strcat(bufferString, "/16x16Tiles.gif");
        }

        #ifdef MEASURE_PERF
            Clock::CounterStart(&counter);
        #endif
        Image tiles16x16;
        Stream* stream = ResourceStream::New(bufferString);
        if (stream) {
            GIF_Load(stream, &tiles16x16);
            stream->Close();

            Uint8* tileSrc;
            Uint8* tileDst;
            const int MAX_TILE_PIXELS = MAX_TILE_COUNT * TILE_SIZE * TILE_SIZE;

            // Add any unset palette lines from the 16x16Tiles to the first Palette.
            for (int paletteLine = 0; paletteLine < 16; paletteLine++) {
                if ((UsedGameConfigPaletteLines[0] & (1 << paletteLine)) == 0 &&
                    (UsedStageConfigPaletteLines[0] & (1 << paletteLine)) == 0) {
                    memcpy(&Graphics::Palette[0][paletteLine << 4], &tiles16x16.Palette[paletteLine << 4], sizeof(Pixel) * 16);
                }
            }

            // Get tile data from GIF
            memcpy(TileImageData, tiles16x16.Data, MAX_TILE_PIXELS * sizeof(Uint8));

            // Flip tiles horizontally
            tileSrc = &TileImageData[0];
            tileDst = &TileImageData[MAX_TILE_PIXELS];
            for (int line = 0; line < MAX_TILE_COUNT * TILE_SIZE; line++) {
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
            tileSrc = &TileImageData[0];
            tileDst = &TileImageData[MAX_TILE_PIXELS << 1];
            for (int tile = 0; tile < MAX_TILE_COUNT; tile++) {
                int ySrc = 0;
                int yDst = (TILE_SIZE - 1) * TILE_SIZE;
                for (; ySrc < TILE_SIZE * TILE_SIZE; ) {
                    // Copy tile line
                    memcpy(&tileDst[yDst], &tileSrc[ySrc], TILE_SIZE * sizeof(Uint8));
                    ySrc += TILE_SIZE;
                    yDst -= TILE_SIZE;
                }
                tileSrc += TILE_SIZE * TILE_SIZE; // Move to next tile
                tileDst += TILE_SIZE * TILE_SIZE; // Move to next tile
            }

            // Flip tiles horizontally & vertically
            tileSrc = &TileImageData[MAX_TILE_PIXELS << 1];
            tileDst = &TileImageData[MAX_TILE_PIXELS << 1 | MAX_TILE_PIXELS];
            for (int line = 0; line < MAX_TILE_COUNT * TILE_SIZE; line++) {
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
        }
        else {
            ZERO_OUT(TileImageData);
        }

        #ifdef MEASURE_PERF
            Clock::CounterFinish(&counter);
            printf("Load16x16Tiles took %.1f ms\n", Clock::CounterGetElapsed(&counter));
        #endif
    }
    void LoadScene(const char* filename) {
        #ifdef MEASURE_PERF
        Clock::Counter counter;
        #endif

        // Load Scene (Layers & Tiles)
        memset(&EntitySlots[0], 0, MAX_ENTITIES * sizeof(EntitySlot));

        Memory::ClearPool(Memory::MEMPOOL_TEMP);

        // Reset deform split lines
        for (Uint32 v = 0; v < Game::State.ViewCount; v++)
            Graphics::Views[v].DeformSplitLine = Graphics::Views[v].Height;

        // Copy colors from stage and global palettes
        for (int p = 0; p < MAX_PALETTE_COUNT; p++) {
            for (int paletteLine = 0; paletteLine < 16; paletteLine++) {
                int row = (paletteLine << 4);
                if ((UsedStageConfigPaletteLines[p] & (1 << paletteLine)) != 0)
                    memcpy(&Graphics::Palette[p][row], &Scene::StageConfigPalette[p][row], 16 * sizeof(Pixel));
                else if ((UsedGameConfigPaletteLines[p] & (1 << paletteLine)) != 0) {
                    memcpy(&Graphics::Palette[p][row], &Scene::GameConfigPalette[p][row], 16 * sizeof(Pixel));
                }
            }
        }

        // Reset palette lines
        if (Graphics::Views[0].Height)
            memset(Graphics::PaletteIndexLines, 0, Graphics::Views[0].Height);

        char bufferString[256];
        bufferString[0] = 0;
        if (Game::OverridenStartScene[0]) {
            strcat(bufferString, Game::OverridenStartScene);
        }
        else {
            strcat(bufferString, "Stages/");
            strcat(bufferString, Game::State.Scenes[Game::State.CurrentSceneIndex].Zone);
            strcat(bufferString, "/Scene");
            strcat(bufferString, Game::State.Scenes[Game::State.CurrentSceneIndex].SceneID);
            strcat(bufferString, ".bin");
        }

        #ifdef MEASURE_PERF
            Clock::CounterStart(&counter);
        #endif

        Stream* stream = ResourceStream::New(bufferString);
        if (stream) {
            Uint32 objectDefinitionCount, layerCount;
            char streamStringBuffer[256];

            // Signature checking
            if (stream->ReadUInt32() == 0x004E4353) {
                // Editor metadata
                stream->Skip(16); // 16 bytes
                // stream->ReadByte(); // ?
                // stream->ReadUint32(); // Background Color 1
                // stream->ReadUint32(); // Background Color 2
                // stream->ReadByte(); // ?
                // stream->ReadByte(); // ?
                // stream->ReadByte(); // ?
                // stream->ReadByte(); // ?
                // stream->ReadByte(); // ?
                // stream->ReadByte(); // ?
                // stream->ReadByte(); // ?
                stream->ReadHeaderedString(streamStringBuffer); // Stamp library name
                stream->ReadByte(); // ???

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

                // Layer count
                layerCount = stream->ReadByte();
                for (Uint32 i = 0; i < layerCount; i++) {
                    Layer* layer = &Layers[i];

                    stream->ReadByte(); // Ignored Byte

                    stream->ReadHeaderedString(streamStringBuffer);
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

                    layer->RelativeScroll.Full = stream->ReadInt16() << 8;
                    layer->ConstantScroll.Full = stream->ReadInt16() << 8;
                    layer->ParallaxInfoCount = stream->ReadUInt16();

                    Memory::Alloc(&layer->Tiles, layer->DataWidth * layer->DataHeight * sizeof(Tile), Memory::MEMPOOL_STAGE, false);
                    Memory::Alloc(&layer->ParallaxIndexLines, (M_MAX(layer->DataWidth, layer->DataHeight) << TILE_SIZE_IN_BITS) * sizeof(Uint8), Memory::MEMPOOL_STAGE, false);
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

                    size_t compressedSize;

                    RSDKTile* rawTileData;
                    Memory::Alloc(&rawTileData, sizeof(RSDKTile) * layer->DataWidth * layer->DataHeight, Memory::MEMPOOL_TEMP, false);

                    compressedSize = M_MAX(layer->DataWidth, layer->DataHeight) << TILE_SIZE_IN_BITS;
                    stream->ReadCompressed(layer->ParallaxIndexLines, compressedSize);

                    compressedSize = sizeof(Tile) * layer->Width * layer->Height;
                    stream->ReadCompressed(rawTileData, compressedSize);

                    // Convert to HatchTiles
                    Tile* tileRowDst = layer->Tiles;
                    RSDKTile* tileRowSrc = rawTileData;
                    for (Uint32 y = 0; y < layer->Height; y++) {
                        for (Uint32 x = 0; x < layer->Width; x++) {
                            auto dst = &tileRowDst[x];
                            auto src = &tileRowSrc[x];

                            if (*src == 0xFFFFU) {
                                *dst = TILE_EMPTY;
                                continue;
                            }

                            *dst = 0;
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

                #ifdef MEASURE_PERF
                Clock::CounterFinish(&counter);
                printf("LoadScene (Layers) took %.1f ms\n", Clock::CounterGetElapsed(&counter));
                #endif

                bool variableFound[64];
                int variableTypes[64];
                size_t variableOffsets[64];

                #ifdef MEASURE_PERF
                Clock::CounterStart(&counter);
                #endif

                EntitySlot* EntitySlotsSpillover;
                Memory::Alloc(&EntitySlotsSpillover, MAX_SLOT_ENTITIES * sizeof(EntitySlot), Memory::MEMPOOL_TEMP, true);

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
                    for (Uint32 i = 0; i < ClassIndexCount; i++) {
                        if (classHash == GameLinker::ClassList[ClassIndexList[i]].Name)
                            classIndex = i;
                    }

                    // Serialization data
                    int variableCount = stream->ReadByte();

                    // Setup class serialization
                    Classes::ClassAttributeCount = 0;
                    if (classIndex > -1) {
                        Classes::SetupAttribute(VAR_UINT8, "filter", offsetof(Entity, Filter));

                        // Call the class' Setup function
                        auto setupFunction = GameLinker::ClassList[ClassIndexList[classIndex]].onSetup;
                        if (setupFunction)
                            setupFunction();
                    }

                    Hash variableNameHash;
                    variableTypes[0] = 9;
                    variableFound[0] = true;
                    variableOffsets[0] = offsetof(Entity, Position);

                    for (int a = 1; a < variableCount; a++) {
                        variableNameHash.A = stream->ReadUInt32();
                        variableNameHash.B = stream->ReadUInt32();
                        variableNameHash.C = stream->ReadUInt32();
                        variableNameHash.D = stream->ReadUInt32();

                        variableTypes[a] = stream->ReadByte();
                        variableFound[a] = false;
                        variableOffsets[a] = 0;

                        for (int attr = 0; attr < Classes::ClassAttributeCount; attr++) {
                            if (variableNameHash == Classes::ClassAttributes[attr].Name) {
                                variableFound[a] = true;
                                variableOffsets[a] = Classes::ClassAttributes[attr].StructOffset;
                                break;
                            }
                        }
                    }

                    int entityCount = stream->ReadUInt16();
                    for (int n = 0; n < entityCount; n++) {
                        int slotID = stream->ReadUInt16();

                        Entity* currentEntity = &EntitySlots[slotID + MAX_RESERVED_ENTITIES];
                        if (slotID >= MAX_SLOT_ENTITIES) {
                            slotID -= MAX_SLOT_ENTITIES;
                            currentEntity = &EntitySlotsSpillover[slotID];
                        }

                        Uint8* entityBytePtr = (Uint8*)currentEntity;

                        currentEntity->Position.X.Full = stream->ReadInt32();
                        currentEntity->Position.Y.Full = stream->ReadInt32();
                        currentEntity->ClassID = classIndex;

                        for (int a = 1; a < variableCount; a++) {
                            size_t offset = variableOffsets[a];
                            if (variableFound[a]) {
                                switch (variableTypes[a]) {
                                    case VAR_UINT8: *(Uint8*)(entityBytePtr + offset) = stream->ReadByte(); break;
                                    case VAR_UINT16: *(Uint16*)(entityBytePtr + offset) = stream->ReadUInt16(); break;
                                    case VAR_UINT32: *(Uint32*)(entityBytePtr + offset) = stream->ReadUInt32(); break;
                                    case VAR_INT8: *(Sint8*)(entityBytePtr + offset) = stream->ReadByte(); break;
                                    case VAR_INT16: *(Sint16*)(entityBytePtr + offset) = stream->ReadInt16(); break;
                                    case VAR_INT32: *(Sint32*)(entityBytePtr + offset) = stream->ReadInt32(); break;
                                    // Enum
                                    case VAR_ENUM: *(Sint32*)(entityBytePtr + offset) = stream->ReadInt32(); break;
                                    // bool
                                    case VAR_BOOL: *(bool*)(entityBytePtr + offset) = stream->ReadUInt32(); break;
                                    // String
                                    case VAR_STRING:
                                    {
                                        Uint16 length = stream->ReadUInt16();

                                        String* string = (String*)(entityBytePtr + offset);
                                        Strings::Init(string, length);

                                        string->Length = length;
                                        for (size_t c = 0; c < length; c++)
                                            string->Text[c] = stream->ReadUInt16();
                                        break;
                                    }
                                    // Position
                                    case VAR_VECTOR2:
                                        ((Vector2*)(entityBytePtr + offset))->X = stream->ReadInt32();
                                        ((Vector2*)(entityBytePtr + offset))->Y = stream->ReadInt32();
                                        break;
                                    // Unknown
                                    case 10: *(Uint32*)(entityBytePtr + offset) = stream->ReadUInt32(); break;
                                    // Color
                                    case VAR_COLOR: *(Color*)(entityBytePtr + offset) = stream->ReadUInt32(); break;
                                }
                            }
                            else {
                                switch (variableTypes[a]) {
                                    case VAR_UINT8: stream->Skip(1); break;
                                    case VAR_UINT16: stream->Skip(2); break;
                                    case VAR_UINT32: stream->Skip(4); break;
                                    case VAR_INT8: stream->Skip(1); break;
                                    case VAR_INT16: stream->Skip(2); break;
                                    case VAR_INT32: stream->Skip(4); break;
                                    // Enum
                                    case VAR_ENUM: stream->Skip(4); break;
                                    // bool
                                    case VAR_BOOL: stream->Skip(4); break;
                                    // String
                                    case VAR_STRING: stream->Skip(stream->ReadUInt16() * 2); break;
                                    // Position
                                    case VAR_VECTOR2: stream->Skip(8); break;
                                    // Unknown
                                    case 10: stream->Skip(4); break;
                                    // Color
                                    case VAR_COLOR: stream->Skip(4); break;
                                }
                            }
                        }

                        if (classIndex > -1) {
                            if (!currentEntity->Filter)
                                currentEntity->Filter = 0xFF;
                        }
                    }
                }

                // Filter out the entites that are unneeded
                int validSlot = MAX_RESERVED_ENTITIES;
                for (int i = MAX_RESERVED_ENTITIES; i < MAX_RESERVED_ENTITIES + MAX_SLOT_ENTITIES; i++) {
                    EntitySlot* slot = &EntitySlots[i];

                    if (slot->Filter & Game::State.SceneFilter) {
                        if (i > validSlot) {
                            if (validSlot < MAX_RESERVED_ENTITIES + MAX_SLOT_ENTITIES) {
                                memcpy(&EntitySlots[validSlot], slot, sizeof(EntitySlot));
                                memset(slot, 0, sizeof(EntitySlot));
                            }
                        }
                        // Move the valid slot to the next one, since this one is occupied.
                        validSlot++;
                    }
                    else {
                        memset(slot, 0, sizeof(EntitySlot));
                    }
                }
                // Add any spillover entities
                for (int i = 0; i < MAX_SLOT_ENTITIES; i++) {
                    EntitySlot* slot = &EntitySlotsSpillover[i];

                    if (slot->Filter & Game::State.SceneFilter) {
                        if (validSlot < MAX_RESERVED_ENTITIES + MAX_SLOT_ENTITIES) {
                            memcpy(&EntitySlots[validSlot], slot, sizeof(EntitySlot));
                        }
                        validSlot++;
                    }
                }

                EntitySlotsSpillover = NULL;

                #ifdef MEASURE_PERF
                Clock::CounterFinish(&counter);
                printf("LoadScene (Objects) took %.1f ms\n", Clock::CounterGetElapsed(&counter));
                #endif
            }

            stream->Close();
        }
        else {
            fprintf(stderr, "Could not open '%s'!\n", bufferString);
        }
    }

    void StartScene() {
        Game::State.CurrentEntityIndex = 0;
        Game::State.FreeEntityIndex = MAX_RESERVED_ENTITIES + MAX_SLOT_ENTITIES;

        UpdateBoundCount = 0;

        for (Uint32 i = 0; i < ClassIndexCount; i++) {
            auto onLoad = GameLinker::ClassList[ClassIndexList[i]].onStageLoad;
            if (onLoad)
                onLoad();
        }

        for (Game::State.CurrentEntityIndex = 0;
            Game::State.CurrentEntityIndex < MAX_ENTITIES;
            Game::State.CurrentEntityIndex++) {
            CurrentEntity = &EntitySlots[Game::State.CurrentEntityIndex];
            if (CurrentEntity->ClassID > 0) {
                auto onCreate = GameLinker::ClassList[ClassIndexList[CurrentEntity->ClassID]].onCreate;
                if (onCreate) {
                    CurrentEntity->Interactable = true;
                    onCreate(0);
                }
            }
        }

        Game::State.EngineState = ENGINESTATE_UNPAUSED;

        if (UpdateBoundCount == 0) {
            UpdateBounds[0].Focus = (Vector2*)&Graphics::Views[0].X;
            UpdateBounds[0].Range.X = Graphics::Views[0].WidthHalf << 16;
            UpdateBounds[0].Range.Y = Graphics::Views[0].HeightHalf << 16;
            UpdateBounds[0].IsPremultipliedCoords = false;
            UpdateBoundCount++;
        }
    }

    void Update() {
        // Clear DrawGroups' entity lists
        for (int d = 0; d < MAX_DRAWGROUPS; d++)
            Graphics::DrawGroups[d].EntityCount = 0;

        // Run class static updates
        for (Uint32 i = 0; i < ClassIndexCount; i++) {
            auto onStaticUpdate = GameLinker::ClassList[ClassIndexList[i]].onStaticUpdate;
            if (onStaticUpdate)
                onStaticUpdate();
        }

        // Update all UpdateRanges
        for (int i = 0; i < UpdateBoundCount; i++) {
            if (UpdateBounds[i].Focus) {
                if (UpdateBounds[i].IsPremultipliedCoords) {
                    UpdateBounds[i].Position = *UpdateBounds[i].Focus;
                }
                else {
                    UpdateBounds[i].Position.X.Whole = UpdateBounds[i].Focus->X.Fract;
                    UpdateBounds[i].Position.Y.Whole = UpdateBounds[i].Focus->Y.Fract;
                }
            }
        }

        // Do Entity Updates
        for (int i = 0; i < MAX_ENTITIES; i++) {
            Game::State.CurrentEntity = CurrentEntity = &EntitySlots[i];
            Game::State.CurrentEntityIndex = i;

            if (CurrentEntity->ClassID > 0) {
                // Determine if the object should Update & Draw
                switch (CurrentEntity->UpdateType) {
                    case UpdateType_None:
                        CurrentEntity->CanUpdate = false;
                        break;
                    case UpdateType_Always:
                        CurrentEntity->CanUpdate = true;
                        break;
                    case UpdateType_Unpaused:
                        CurrentEntity->CanUpdate = Game::State.EngineState == ENGINESTATE_UNPAUSED || Game::State.EngineState == ENGINESTATE_UNPAUSED_STEP;
                        break;
                    case UpdateType_Paused:
                        CurrentEntity->CanUpdate = Game::State.EngineState == ENGINESTATE_PAUSED || Game::State.EngineState == ENGINESTATE_PAUSED_STEP;
                        break;
                    case UpdateType_Ranged: {
                        for (int v = 0; v < UpdateBoundCount; v++) {
                            ::UpdateBounds* range = &UpdateBounds[v];
                            int distanceX = M_ABS(CurrentEntity->Position.X.Whole - range->Position.X.Whole) - range->Range.X.Whole;
                            int distanceY = M_ABS(CurrentEntity->Position.Y.Whole - range->Position.Y.Whole) - range->Range.Y.Whole;
                            if (distanceX <= CurrentEntity->UpdateRange.X.Whole &&
                                distanceY <= CurrentEntity->UpdateRange.Y.Whole)
                                goto UpdateType_CanUpdate;
                        }
                        CurrentEntity->CanUpdate = false;
                        break;
                    }
                    case UpdateType_RangedHorizontal: {
                        for (int v = 0; v < UpdateBoundCount; v++) {
                            ::UpdateBounds* range = &UpdateBounds[v];
                            int distanceX = M_ABS(CurrentEntity->Position.X.Whole - range->Position.X.Whole) - range->Range.X.Whole;
                            if (distanceX <= CurrentEntity->UpdateRange.X.Whole)
                                goto UpdateType_CanUpdate;
                        }
                        CurrentEntity->CanUpdate = false;
                        break;
                    }
                    case UpdateType_RangedVertical: {
                        for (int v = 0; v < UpdateBoundCount; v++) {
                            ::UpdateBounds* range = &UpdateBounds[v];
                            int distanceY = M_ABS(CurrentEntity->Position.Y.Whole - range->Position.Y.Whole) - range->Range.Y.Whole;
                            if (distanceY <= CurrentEntity->UpdateRange.Y.Whole)
                                goto UpdateType_CanUpdate;
                        }
                        CurrentEntity->CanUpdate = false;
                        break;
                    }
                    default:
                    UpdateType_CanUpdate:
                        CurrentEntity->CanUpdate = Game::State.EngineState == ENGINESTATE_UNPAUSED || Game::State.EngineState == ENGINESTATE_UNPAUSED_STEP;
                        break;
                }

                if (CurrentEntity->CanUpdate) {
                    auto onUpdate = GameLinker::ClassList[ClassIndexList[CurrentEntity->ClassID]].onUpdate;
                    if (onUpdate)
                        onUpdate();

                    auto drawGroup = CurrentEntity->DrawGroup;
                    if (drawGroup >= 0 && drawGroup < MAX_DRAWGROUPS)
                        Graphics::DrawGroups[drawGroup].EntityIndices[Graphics::DrawGroups[drawGroup].EntityCount++] = i;
                }
            }
            else {
                CurrentEntity->CanUpdate = false;
            }
        }

        // Reset the entity count of each StageClass slot list
        for (int i = 0; i < MAX_CLASS_SLOTLISTS; i++)
            ClassSlotLists[i].SlotCount = 0;

        // Add each entity to their StageClass slot list, including subclasses (if the entity CanUpdate & is Interactable)
        for (int i = 0; i < MAX_ENTITIES; i++) {
            Game::State.CurrentEntity = CurrentEntity = &EntitySlots[i];
            Game::State.CurrentEntityIndex = i;

            if (CurrentEntity->CanUpdate && CurrentEntity->Interactable) {
                auto slotList = &ClassSlotLists[CurrentEntity->ClassID];
                slotList->SlotIndexes[slotList->SlotCount++] = i;

                if (CurrentEntity->GroupClassID >= MAX_CLASSES) {
                    slotList = &ClassSlotLists[CurrentEntity->GroupClassID];
                    slotList->SlotIndexes[slotList->SlotCount++] = i;
                }
            }
        }

        // Do Entity Late Updates
        for (int i = 0; i < MAX_ENTITIES; i++) {
            Game::State.CurrentEntity = CurrentEntity = &EntitySlots[i];
            Game::State.CurrentEntityIndex = i;

            if (CurrentEntity->CanUpdate) {
                auto onUpdateLate = GameLinker::ClassList[ClassIndexList[CurrentEntity->ClassID]].onUpdateLate;
                if (onUpdateLate)
                    onUpdateLate();
            }
            CurrentEntity->DidDraw = false;
        }
    }

    bool FindNextClassEntity(ClassID classID, Entity** entity) {
        if (!*entity) {
            SearchStackTop++;
            (*SearchStackTop) = 0;
        }
        else (*SearchStackTop)++;

        while (*SearchStackTop < MAX_ENTITIES) {
            Entity* entPtr = &EntitySlots[*SearchStackTop];
            if (entPtr->ClassID == classID) {
                *entity = entPtr;
                return true;
            }
            (*SearchStackTop)++;
        }

        SearchStackTop--;
        return false;
    }
    bool FindNextClassEntityInteractable(ClassID classID, Entity** entity) {
        if (!*entity) {
            SearchStackTop++;
            (*SearchStackTop) = 0;
        }
        else (*SearchStackTop)++;

        while (*SearchStackTop < ClassSlotLists[classID].SlotCount) {
            Entity* entPtr = &EntitySlots[ClassSlotLists[classID].SlotIndexes[*SearchStackTop]];
            if (entPtr->ClassID == classID) {
                *entity = entPtr;
                return true;
            }
            (*SearchStackTop)++;
        }

        SearchStackTop--;
        return false;
    }
    void FindNextClassEntityBreak() {
        SearchStackTop--;
    }

    Entity* Get(int index) {
        if (index >= MAX_ENTITIES || index < 0)
            return NULL;
        return &EntitySlots[index];
    }
    int     GetIndex(Entity* entity) {
        int index = (int)((EntitySlot*)entity - EntitySlots);
        if (index >= MAX_ENTITIES)
            return 0;

        return index;
    }
    Entity* GetFromDrawGroup(int drawGroup, int index) {
        if (drawGroup >= MAX_DRAWGROUPS || drawGroup < 0)
            return NULL;

        DrawGroup* dg = &Graphics::DrawGroups[drawGroup];
        if (index >= dg->EntityCount || index < 0)
            return NULL;

        return (Entity*)&EntitySlots[dg->EntityIndices[index]];
    }
    int     GetIndexFromDrawGroup(int drawGroup, int index) {
        if (drawGroup >= MAX_DRAWGROUPS || drawGroup < 0)
            return 0;

        DrawGroup* dg = &Graphics::DrawGroups[drawGroup];
        if (index >= dg->EntityCount || index < 0)
            return 0;

        return dg->EntityIndices[index];
    }
    void    Reset(Entity* entity, ClassID classID, CreateFlag flag) {
        if (entity) {
            Class* objectClass = &GameLinker::ClassList[ClassIndexList[classID]];
            memset(entity, 0, objectClass->EntitySize);

            if (objectClass->onCreate) {
                Entity* temp = CurrentEntity;
                CurrentEntity = entity;

                entity->Interactable = true;
                objectClass->onCreate(flag);

                CurrentEntity = temp;
            }

            entity->ClassID = classID;
        }
    }
    void    ResetAtIndex(int index, ClassID classID, CreateFlag flag) {
        Entity* entity = &EntitySlots[index];
        if (entity) {
            Class* objectClass = &GameLinker::ClassList[ClassIndexList[classID]];
            memset(entity, 0, objectClass->EntitySize);

            if (objectClass->onCreate) {
                Entity* temp = CurrentEntity;
                CurrentEntity = entity;

                entity->Interactable = true;
                objectClass->onCreate(flag);

                CurrentEntity = temp;
            }

            entity->ClassID = classID;
        }
    }
    Entity* Create(ClassID classID, CreateFlag flag, int x, int y) {
        Class* objectClass = &GameLinker::ClassList[ClassIndexList[classID]];
        EntitySlot* entity = &EntitySlots[Game::State.FreeEntityIndex];

        int protectCount = 0;
        int attemptCount = 0;
        while (entity->ClassID) {
            if (protectCount >= 0x100)
                break;

            if (entity->Protect)
                protectCount++;
            else if (attemptCount >= 16)
                break;

            Game::State.FreeEntityIndex++;
            attemptCount++;

            if (Game::State.FreeEntityIndex >= MAX_ENTITIES) {
                Game::State.FreeEntityIndex -= MAX_SPAWN_ENTITIES;
                entity = &EntitySlots[Game::State.FreeEntityIndex];
            }
            else
                entity++;
        }


        memset(entity, 0, objectClass->EntitySize);
        entity->Position.X = x;
        entity->Position.Y = y;
        entity->Interactable = true;

        if (objectClass->onCreate) {
            Entity* temp = CurrentEntity;
            CurrentEntity = entity;

            objectClass->onCreate(flag);

            CurrentEntity = temp;
        }
        else {
            entity->UpdateType = UpdateType_Unpaused;
            entity->CanDraw = true;
        }

        entity->ClassID = classID;

        return entity;
    }
    void    Copy(Entity* dest, Entity* src) {
        if (dest == src)
            return;

        memcpy(dest, src, sizeof(EntitySlot));
    }
    void    Move(Entity* dest, Entity* src) {
        if (dest == src)
            return;

        memcpy(dest, src, sizeof(EntitySlot));
        memset(src, 0, sizeof(EntitySlot));
    }
    bool    IsOnScreen(Entity* entity, Vector2* size) {
        if (!entity)
            return false;

        Vector2 comparisonSize = entity->UpdateRange;
        if (size)
            comparisonSize = *size;

        for (Uint32 v = 0; v < Game::State.ViewCount; v++) {
            View* view = &Graphics::Views[v];
            int cameraCenterX = view->X + view->WidthHalf;
            int cameraCenterY = view->Y + view->HeightHalf;
            int distanceX = M_ABS(entity->Position.X.Whole - cameraCenterX) - view->WidthHalf;
            int distanceY = M_ABS(entity->Position.Y.Whole - cameraCenterY) - view->HeightHalf;
            if (distanceX <= comparisonSize.X.Whole &&
                distanceY <= comparisonSize.Y.Whole) {
                return true;
            }
        }
        return false;
    }
    bool    IsPointOnScreen(Vector2* point, Vector2* size) {
        if (!point)
            return false;
        if (!size)
            return false;

        for (Uint32 v = 0; v < Game::State.ViewCount; v++) {
            View* view = &Graphics::Views[v];
            int cameraCenterX = view->X + view->WidthHalf;
            int cameraCenterY = view->Y + view->HeightHalf;
            int distanceX = M_ABS(point->X.Whole - cameraCenterX) - view->WidthHalf;
            int distanceY = M_ABS(point->Y.Whole - cameraCenterY) - view->HeightHalf;
            if (distanceX <= size->X.Whole &&
                distanceY <= size->Y.Whole)
                return true;
        }
        return false;
    }

    int    GetLayerIndexByName(CString name) {
        Hash hash = MD5_HashString(name);
        for (Uint32 i = 0; i < MAX_LAYERS; i++) {
            Layer* layer = &Layers[i];
            if (layer->Name == hash)
                return i;
        }
        return -1;
    }
    int    GetLayerIndexByLayer(Layer* layer) {
        return (int)(layer - &Layers[0]);
    }
    Layer* GetLayerByName(CString name) {
        Hash hash = MD5_HashString(name);
        for (Uint32 i = 0; i < MAX_LAYERS; i++) {
            Layer* layer = &Layers[i];
            if (layer->Name == hash)
                return layer;
        }
        return NULL;
    }
    Layer* GetLayerByIndex(int layerIndex) {
        return &Layers[layerIndex];
    }
    void   GetLayerSize(int layerIndex, int* width, int* height) {
        if (layerIndex < 0 || layerIndex >= MAX_LAYERS)
            return;

        Layer* layer = &Layers[layerIndex];
        *width = (int)layer->Width;
        *height = (int)layer->Height;
    }
}

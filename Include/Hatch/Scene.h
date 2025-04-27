#pragma once

namespace Scene {
    enum DrawBehavior {
        DRAW_HORIZONTAL = 0,
        DRAW_VERTICAL,
        DRAW_SCANLINES,
    };

    struct StageClassSlotList {
        Uint16 SlotIndexes[MAX_ENTITIES];
        int    SlotCount;
    };

    extern ::UpdateBounds UpdateBounds[MAX_VIEWPORTS];
    extern int            UpdateBoundCount;

    extern char    CurrentStage[16];
    extern bool    ReloadStage;

    extern Layer   static_Layers[MAX_LAYERS];
    extern Uint8   static_TileImageData[MAX_TILE_COUNT * TILE_SIZE * TILE_SIZE * 4];
    extern Uint16  static_ClassIndexList[MAX_CLASSES];
    extern EntitySlot static_EntitySlots[MAX_ENTITIES];

    extern Layer*  Layers;
    extern Uint8*  TileImageData;

    extern Uint32  Frame;
    extern Uint16* ClassIndexList;

    extern Entity* CurrentEntity;
    extern EntitySlot* EntitySlots;
    extern StageClassSlotList ClassSlotLists[MAX_CLASS_SLOTLISTS];

    extern Uint16  SearchStack[0x10];
    extern Uint16* SearchStackTop;

    extern Uint16 GlobalClassIndexList[MAX_CLASSES];
    extern Uint32 GlobalClassIndexCount;

    extern Pixel GameConfigPalette[MAX_PALETTE_COUNT][0x100];
    extern int UsedGameConfigPaletteLines[MAX_PALETTE_COUNT];
    extern Pixel StageConfigPalette[MAX_PALETTE_COUNT][0x100];
    extern int UsedStageConfigPaletteLines[MAX_PALETTE_COUNT];

    void Init();
    void LoadStage(const char* filename);
    void LoadScene(const char* filename);
    void StartScene();
    void Update();
    void Draw();

    bool FindNextClassEntity(ClassID classID, Entity** entity);
    bool FindNextClassEntityInteractable(ClassID classID, Entity** entity);
    void FindNextClassEntityBreak();

    Entity* Get(int index);
    int     GetIndex(Entity* entity);
    Entity* GetFromDrawGroup(int drawGroup, int index);
    int     GetIndexFromDrawGroup(int drawGroup, int index);
    void    Reset(Entity* entity, ClassID classID, CreateFlag flag);
    void    ResetAtIndex(int index, ClassID classID, CreateFlag flag);
    Entity* Create(ClassID classID, CreateFlag flag, int x, int y);
    void    Copy(Entity* dest, Entity* src);
    void    Move(Entity* dest, Entity* src);
    bool    IsOnScreen(Entity* entity, Vector2* size);
    bool    IsPointOnScreen(Vector2* point, Vector2* size);

    int    GetLayerIndexByName(CString name);
    int    GetLayerIndexByLayer(Layer* layer);
    Layer* GetLayerByName(CString name);
    Layer* GetLayerByIndex(int layerIndex);
    void   GetLayerSize(int layerIndex, int* width, int* height);
}

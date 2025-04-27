#pragma once

// Must be subclassed by the game source.
struct Globals { };

struct Image {
    Uint16  Width;
    Uint16  Height;
    Pixel*  Palette;
    Uint8*  Data;
};

struct Frame {
    int      SourceX;
    int      SourceY;

    int      Width;
    int      Height;
    int      OffsetX;
    int      OffsetY;

    int      Duration;
    int      ID;

    Resource Image;

    Hitbox   Hitboxes[8];
};

struct Animation {
    Hash   Name;
    int    StartFrameIndex;
    int    FrameCount;
    int    LoopFrameIndex;
    int    Speed;
    Uint8  RotationFlag;
};

struct Sprite {
    Animation* Animations;
    int        AnimationCount;

    Frame*     Frames;
};

struct Sound {
    void*   SampleBuffer;
    int     SampleCount;
    int     CurrentPlays;
    Uint8   MaxConcurrentPlay;
};

struct Mesh {
    Vector3* Positions;
    Vector2* UVs;
    Color*   Colors;

    Uint16   VertexCount;

    Sint16*  VertexIndices;
    Uint16   VertexIndexCount;
    Uint16   FrameCount;

    Uint8    VertexType;
    Uint8    FaceVertexCount;
};

struct ScanLine {
    Subpixels SourceX;
    Subpixels SourceY;
    Subpixels DeltaX;
    Subpixels DeltaY;
    ScanLine() {
        SourceX.Full = SourceY.Full = DeltaX.Full = DeltaY.Full = 0;
    }
};

struct Parallax {
    Subpixels RelativeParallax;
    Subpixels ConstantParallax;
    Subpixels ParallaxPosition;
    Subpixels ParallaxOffset;
    bool      CanDeform;
};

struct Layer {
    Hash          Name;
    size_t        Width;
    size_t        Height;
    size_t        DataWidth;
    size_t        DataHeight;
    int           WidthInBits;
    int           HeightInBits;

    Tile*         Tiles;
    size_t        TilePitch;
    size_t        TilePitchMask;
    size_t        TileDataSize; // Size of the tile buffer, in bytes.

    int           DeformOffsetA;
    int           DeformOffsetB;
    int           DeformSetA[MAX_DEFORM_LINES];
    int           DeformSetB[MAX_DEFORM_LINES];

    int           DrawBehavior;
    void          (*ScanLineFunction)(ScanLine* scanLineBuffer);
    int           DrawGroup[MAX_VIEWPORTS];
    bool          Hidden[MAX_VIEWPORTS];

    Parallax*     ParallaxInfos;
    int           ParallaxInfoCount;
    Uint8*        ParallaxIndexLines;

    Subpixels     RelativeScroll;
    Subpixels     ConstantScroll;
    Subpixels     ScrollPosition;
    Subpixels     ScrollOffset;

    Subpixels     CameraOffsetX;
    Subpixels     CameraOffsetY;

    Vector2       CollideOffset;
};

struct View {
    int     X;
    int     Y;
    int     Width;
    int     Height;
    int     WidthHalf;
    int     HeightHalf;
    Uint32  Pitch;
    bool    DirtySize;
    int     DeformSplitLine;
    int     ClipStartX;
    int     ClipStartY;
    int     ClipEndX;
    int     ClipEndY;
    Pixel*  Pixels;
};

struct DrawGroup {
    int     EntityIndices[MAX_ENTITIES];
    int     EntityCount;
    bool    EntityDepthSortingEnabled;
    int     LayerIndices[MAX_LAYERS];
    int     LayerCount;
    void    (*PrefixFunction)();
};

struct Matrix4x4 {
    int Column[4][4];
};
struct VertexAttribute {
    Vector3 Position;
    Vector3 Normal;
    Vector2 UV;
    ::Color Color;
};
struct FaceInfo {
    int Depth;
    int VerticesStartIndex;
};
struct ArrayBuffer {
    VertexAttribute* VertexBuffer;      // count = max vertex count
    FaceInfo*        FaceInfoBuffer;    // count = max face count
    Uint8*           FaceSizeBuffer;    // count = max face count
    Uint32           PerspectiveBitshiftX;
    Uint32           PerspectiveBitshiftY;
    Uint32           LightingAmbientR;
    Uint32           LightingAmbientG;
    Uint32           LightingAmbientB;
    Uint32           LightingDiffuseR;
    Uint32           LightingDiffuseG;
    Uint32           LightingDiffuseB;
    Uint32           LightingSpecularR;
    Uint32           LightingSpecularG;
    Uint32           LightingSpecularB;
    Uint16           VertexCapacity;
    Uint16           VertexCount;
    Uint16           FaceCount;
    Uint8            DrawMode;
    bool             Initialized;
};

struct TileConfig {
    Uint8 Behavior;
    Uint8 AngleTop;
    Uint8 AngleLeft;
    Uint8 AngleRight;
    Uint8 AngleBottom;
    Sint8 CollisionTop[TILE_SIZE];
    Sint8 CollisionLeft[TILE_SIZE];
    Sint8 CollisionRight[TILE_SIZE];
    Sint8 CollisionBottom[TILE_SIZE];
};

union CreateFlag {
    enum {
        EMPTY = 0,
    };

    void* Pointer;
    int Number;
    // void* pointer;
    // int number;
    // StaticStatus staticStatus;
    // Status status;

    operator int() const { return Number; }
    CreateFlag(const int n) : Number(n) { }
    CreateFlag& operator=(const int& other) { Number = other; return *this; }

    template <typename M>
    operator M*() const { return (M*)Pointer; }
    template <typename M>
    CreateFlag(const M* n) { Pointer = (void*)n; }
    template <typename M>
    CreateFlag& operator=(const M*& other) { Pointer = (void*)other; return *this; }
};

struct Entity {
    Vector2   Position;
    Vector2   Scale;
    Vector2   Speed;
    Vector2   UpdateRange;
    int       Angle;
    int       Opacity;
    Uint32    Rotation;
    Subpixels GroundSpeed;
    int       Depth;
    ::ClassID ClassID;
    ::ClassID GroupClassID;
    bool      CanUpdate;
    bool      Protect; // Protects the newly spawned entity slot from being overwritten by another newly spawned entity slot.
    bool      TileCollision;
    bool      Interactable;
    bool      Grounded;
    Uint8     UpdateType;
    Uint8     Filter;
    Uint8     FlipFlag;
    Uint8     DrawGroup;
    Uint8     LayerCollisionFlag;
    Uint8     PlaneIndex;
    Uint8     AngleMode;
    Uint8     TransformFlag;
    Uint8     BlendFlag;
    Uint8     CanDraw;
    Uint8     DidDraw;

    static void OnStageLoad() { }
    static void OnEditorLoad() { }
    static void OnStaticUpdate() { }
    void OnCreate(CreateFlag flag) { }
    void OnUpdate() { }
    void OnUpdateLate() { }
    void OnStageDraw() { }
    void OnEditorDraw() { }
    static void OnSetup() { }
};
struct EntitySlot : Entity {
    Uint8 Padding[0x500];
};

struct StaticObject {
    ::ClassID StageClassID;
    int       UpdateFlag;
};

struct Class {
    Hash   Name;
    void** StaticObjectPtr;
	size_t EntitySize;
	size_t StaticObjectSize;
    void (*onStageLoad)();
    void (*onEditorLoad)();
    void (*onStaticUpdate)();
    void (*onCreate)(CreateFlag flag);
    void (*onUpdate)();
    void (*onUpdateLate)();
    void (*onStageDraw)();
    void (*onEditorDraw)();
    void (*onSetup)();
    void (*onStaticConstructor)(void* staticObject);
};

struct Animator {
    Frame* StartFrame;
    int    FrameIndex;
    int    AnimationIndex;
    int    PrevAnimationIndex;
    int    Speed;
    int    Time;
    int    FrameDuration;
    int    FrameCount;
    int    FrameLoop;
    int    RotationFlag;
};

struct Sensor {
    int X;
    int Y;
    bool Collided;
    int Angle;
    Uint8 Flags;
};

struct Quad {
    Vector2 Vertices[4];
};

struct Status {
    operator bool() const { return rawPtr != NULL; }
    void Call(void* ent) {
        if (rawPtr != NULL)
            ((Entity*)ent->*(rawPtr))();
    }

    // Interfacing with other statuses
    bool operator==(const Status& other) { return rawPtr == other.rawPtr; }
    bool operator!=(const Status& other) { return rawPtr != other.rawPtr; }
    Status& operator=(const Status& other) { rawPtr = other.rawPtr; return *this; }

    // Interfacing with functions
    template <typename M> bool operator==(const M& other) { return rawPtr == RawCast(other); }
    template <typename M> bool operator!=(const M& other) { return rawPtr != RawCast(other); }
    template <typename M> Status& operator=(const M& other) { rawPtr = RawCast(other); return *this; }

private:
    typedef void (Entity::* StatusFunc)();

    StatusFunc rawPtr;

    template <typename M>
    static inline StatusFunc RawCast(M functionPtr) {
        union { StatusFunc rawPtr; M funcPtr; } caster;
        caster.funcPtr = functionPtr;
        return caster.rawPtr;
    }
};
struct StaticStatus {
    typedef void (*StatusFunc)();

    StatusFunc rawPtr;

    StaticStatus() { rawPtr = NULL; }
    StaticStatus(StatusFunc f) { rawPtr = f; }

    operator bool() const { return rawPtr != NULL; }
    void Call() {
        if (rawPtr)
            rawPtr();
    }

    // Interfacing with other statuses
    bool operator==(const StaticStatus& other) { return rawPtr == other.rawPtr; }
    bool operator!=(const StaticStatus& other) { return rawPtr != other.rawPtr; }
    StaticStatus& operator=(const StaticStatus& other) { rawPtr = other.rawPtr; return *this; }

    // Interfacing with functions
    bool operator==(const StatusFunc& other) { return rawPtr == (other); }
    bool operator!=(const StatusFunc& other) { return rawPtr != (other); }
    StaticStatus& operator=(const StatusFunc& other) { rawPtr = (other); return *this; }
};

struct InputState {
    bool Down;
    bool Pressed;
    bool Released;
};
struct InputPad {
    InputState Up;
    InputState Down;
    InputState Left;
    InputState Right;
    InputState A;
    InputState B;
    InputState C;
    InputState X;
    InputState Y;
    InputState Z;
    InputState Start;
    InputState Select;
};
struct InputTouch {
    InputState State;
    float X;
    float Y;
};

struct SceneCategory {
    Hash NameHash;
    char Name[32];
    Uint16 FirstSceneIndex;
    Uint16 LastSceneIndex;
    Uint16 SceneCount;
};
struct SceneInfo {
    Hash NameHash;
    char Name[32];
    char Zone[16];
    char SceneID[4];
    Uint8 Filter;
};

struct GameState {
    SceneInfo*     Scenes;
    SceneCategory* Categories;
    Uint8          CategoryStartIndex;
    Uint8          CategoryCount;

    Entity*        CurrentEntity;
    int            CurrentEntityIndex;
    int            FreeEntityIndex;
    int            CurrentDrawGroup;
    int            CurrentSceneIndex;

    int            StageClassCount;
    Uint8          SceneFilter;

    bool           IsEditor;
    Uint8          EngineState;

    View*          Views;
    int            CurrentViewIndex;
    Uint32         ViewCount;

    InputPad*      Inputs;
    InputTouch*    TouchInputs;

    bool           TimerActive;
    int            TimerMinutes;
    int            TimerSeconds;
    int            TimerCentiseconds;
    int            TimerTicks;
};

struct UpdateBounds {
    Vector2* Focus;
    Vector2  Position;
    Vector2  Range;
    bool     IsPremultipliedCoords;
};

struct ViewOutput {
    bool Active;
    bool IsMovie;
    int  ViewIndex;
    int  ScaleType;

    // These are in window coords.
    int  X;
    int  Y;
    int  Width;
    int  Height;
};

enum ViewOutputScaleType {
    VOSCALE_NONE, // Does not scale the output to the window.
    VOSCALE_FIT_TO_SCREEN, // Changes the output size to fit inside the window size, letterboxing/pillarboxing may occur.
    VOSCALE_COVER_TO_SCREEN, // Changes the output size to cover the window size, some parts of image will be offscreen.
    VOSCALE_STRETCH_TO_SCREEN, // Changes the output size to the window size.
    VOSCALE_RESIZE_TO_SCREEN, // Changes the view size to fit the aspect ratio of the window size, sets output size to the window size.
};

// Constants
static const int UNLOAD_STAGE_END = 1;
static const int UNLOAD_GAME_END = 2;
static const Vector2 VECTOR2_ZERO = Vector2();
static const Vector2 VECTOR2_ONE = Vector2(0x10000, 0x10000);
static const Vector3 VECTOR3_ZERO = Vector3();
static const Vector3 VECTOR3_ONE = Vector3(0x10000, 0x10000, 0x10000);

enum VariableTypes {
    VAR_UINT8 = 0,
    VAR_UINT16 = 1,
    VAR_UINT32 = 2,
    VAR_INT8 = 3,
    VAR_INT16 = 4,
    VAR_INT32 = 5,
    VAR_ENUM = 6,
    VAR_BOOL = 7,
    VAR_STRING = 8,
    VAR_VECTOR2 = 9,
    VAR_COLOR = 11,
};
enum UpdateType : Uint8 {
    UpdateType_None = 0,
    UpdateType_Always = 1,
    UpdateType_Unpaused = 2,
    UpdateType_Paused = 3,
    UpdateType_Ranged = 4,
    UpdateType_RangedHorizontal = 5,
    UpdateType_RangedVertical = 6,
    UpdateType_RangedRadius = 7,
};
enum RotationFlags : Uint8 {
    RotationFlag_NoRotation = 0,
    RotationFlag_RotationFull = 1,
    RotationFlag_RotationEighth = 2,
    RotationFlag_RotationQuarter = 3,
    RotationFlag_RotationHalf = 4,
};
enum FlipFlags : Uint8 {
    FLIPXY_NONE = 0,
    FLIPXY_X = 1,
    FLIPXY_Y = 2,
    FLIPXY_XY = 3,
};
enum TransformFlags : Uint8 {
    TRANSFORM_NONE = 0,
    TRANSFORM_ALLOW_FLIP = 1,
    TRANSFORM_ALLOW_ROTATE = 2,
    TRANSFORM_ALLOW_SCALE = 4,
};
enum VertexType {
    VertexType_Position = 0,
    VertexType_Normal = 1,
    VertexType_UV = 2,
    VertexType_Color = 4,
};
enum BlendFlags {
    BLEND_NONE,
    BLEND_TRANSPARENT,
    BLEND_ADDITIVE,
    BLEND_SUBTRACT,
    BLEND_MATCH,
    BLEND_NON_MATCH,
    BLEND_FILTERED,
};
enum V3DTypes {
    V3D_LINES = 0,
    V3D_POLYGONS = 1,

    V3D_SOLID = 0,
    V3D_FLAT = 2,
    V3D_SMOOTH = 4,

    V3D_ORTHO = 0,
    V3D_PERSPECTIVE = 8,
};
enum SolidCollideSide {
    COLLSIDE_NONE,
    COLLSIDE_TOP,
    COLLSIDE_LEFT,
    COLLSIDE_RIGHT,
    COLLSIDE_BOTTOM,
};
enum TextAlignment {
    TEXT_ALIGN_LEFT = 0,
    TEXT_ALIGN_CENTER = 1,
    TEXT_ALIGN_RIGHT = 2,

    TEXT_VALIGN_TOP = 0x10,
    TEXT_VALIGN_MIDDLE = 0x20,
    TEXT_VALIGN_BOTTOM = 0x30,
    TEXT_VALIGN_BASELINE = 0x40,
};

enum ENGINESTATE {
    ENGINESTATE_SCENELOAD = 0x0,
    ENGINESTATE_UNPAUSED = 0x1,
    ENGINESTATE_PAUSED = 0x2,
    ENGINESTATE_FULLUPDATE = 0x3,
    ENGINESTATE_STEP = 0x4,
    ENGINESTATE_UNPAUSED_STEP = 0x5,
    ENGINESTATE_PAUSED_STEP = 0x6,
    ENGINESTATE_FULLUPDATE_STEP = 0x7,
    ENGINESTATE_DEVMENU = 0x8,
    ENGINESTATE_VIDEO = 0x9,
    ENGINESTATE_0xA = 0xA,
    ENGINESTATE_0xB = 0xB,
    ENGINESTATE_0xC = 0xC,
    ENGINESTATE_UNPAUSED_StepForward = 0xD,
};

enum USERSTORAGE_STATUS {
    STATUS_OK = 200,
    STATUS_ERR = 500,
};
enum LOCALE {
    LOCALE_EN,
    LOCALE_FR,
};
enum LANGUAGE {
    LANG_EN,
    LANG_FR,
    LANG_SP,
    LANG_IT,
    LANG_GE,
    LANG_JP,
};

#define M_ABS(v) ((v) < 0 ? -(v) : (v))
#define M_MAX(a, b) ((a) > (b) ? (a) : (b))
#define M_MIN(a, b) ((a) < (b) ? (a) : (b))
#define M_CLAMP(v, v_min, v_max) M_MAX(v_min, M_MIN(v, v_max))
#define M_SWAP(a, b) { auto temp = a; a = b; b = temp; }
#define M_BIT_GET(field, n) (((field) >> (n)) & 1)

typedef struct {
    void (*AllocateGlobals)(Globals** globals, size_t offset);

    // ===========
    // Objects
    // ===========
    struct {
        Entity* (*Get)(int index);
        int     (*GetIndex)(Entity* entity);

        Entity* (*GetFromDrawGroup)(int drawGroup, int index);
        int     (*GetIndexFromDrawGroup)(int drawGroup, int index);

        void    (*Reset)(Entity* entity, ClassID classID, CreateFlag flag);
        void    (*ResetAtIndex)(int index, ClassID classID, CreateFlag flag);

        Entity* (*Create)(ClassID classID, CreateFlag flag, int x, int y);
        void    (*Copy)(Entity* dest, Entity* src);
        void    (*Move)(Entity* dest, Entity* src);

        bool    (*IsOnScreen)(Entity* entity, Vector2* size);
        bool    (*IsPointOnScreen)(Vector2* point, Vector2* size);
    } Entity;

    struct {
        void (*Add)(CString className,
            void** staticObjectPtr,
            size_t entitySize,
            size_t staticObjectSize,
            void (*onStageLoad)(),
            void (*onEditorLoad)(),
            void (*onStaticUpdate)(),
            void (*onCreate)(CreateFlag flag),
            void (*onUpdate)(),
            void (*onUpdateLate)(),
            void (*onStageDraw)(),
            void (*onEditorDraw)(),
            void (*onSetup)(),
            void (*onStaticConstructor)(void* staticObject));
        void (*CreateGlobalClass)(CString className, void** staticObjectPtr, size_t staticObjectSize, void (*onStaticConstructor)(void* staticObject));
        void (*SetupAttribute)(int attributeType, CString name, size_t offset);
        void (*AddEnumValue)(CString name, int value);
        void (*LoadStaticObject)(CString filename);
        bool (*SearchEntity)(ClassID classID, ::Entity** entity);
        bool (*SearchInteractableEntity)(ClassID classID, ::Entity** entity);
        void (*EndSearch)();
        int  (*GetEntityCount)(ClassID classID, bool interactable);
    } Class;

    struct {
        bool (*EntitiesAABB)(::Entity* entityA, Hitbox* hitboxA, ::Entity* entityB, Hitbox* hitboxB);
        bool (*EntitiesCircular)(::Entity* entityA, Subpixels radiusA, ::Entity* entityB, Subpixels radiusB);
        int  (*EntitiesSolid)(::Entity* entitySolid, Hitbox* hitboxSolid, ::Entity* entity, Hitbox* hitbox, bool adjustSpeeds);
        bool (*EntitiesPlatform)(::Entity* entityPlatform, Hitbox* hitboxPlatform, ::Entity* entity, Hitbox* hitbox, bool adjustSpeeds);
        void (*ApplyTile360)(::Entity* entity, Hitbox* outerHitbox, Hitbox* innerHitbox);
        bool (*TileHit)(Vector2* position, Uint16 layerCollisionFlag, Uint8 angleMode, Uint8 planeIndex, Subpixels offsetX, Subpixels offsetY, bool grip);
        bool (*TileGrip)(Vector2* position, Uint16 layerCollisionFlag, Uint8 angleMode, Uint8 planeIndex, Subpixels offsetX, Subpixels offsetY, int tolerance);
    } Collision;

    struct {
        void (*Add)(Vector2* focusPosition, int focusRangeX, int focusRangeY, bool isPremultipliedCoords);
        void (*ClearAll)();
    } UpdateBounds;

    struct {
        void (*ConvertStringToSpriteText)(Resource spriteIndex, int animIndex, String* string);
        int  (*MeasureSpriteTextWidth)(Resource spriteIndex, int animIndex, String* string, int startIndex, int endIndex, int spacing);
    } Sprites;


    // ===========
    // Drawing
    // ===========
    struct {
        void (*Sprite)(Resource sprite, int animation, int frame, Vector2* position);
        void (*SpritePart)(Resource sprite, int animation, int frame, Vector2* position, int partX, int partY, int partW, int partH, bool flipX, bool flipY, Vector2* scale, int rotation);
        void (*Animation)(Animator* animator, Vector2* position);
        void (*Image)(Resource image, Vector2* position);
        void (*ImagePart)(Resource image, Vector2* position, int srcX, int srcY, int srcW, int srcH);
        void (*SpriteText)(String* string, Vector2* position, Resource sprite, int animation, int startIndex, int endIndex, int alignment, int spacing, Vector2* offsets);

        void (*DebugText)(CString string, Vector2* position, Color color);

        void (*Tile)(Tile tile, Vector2* position, bool flipX, bool flipY);
        void (*CopyImageToTiles)(Resource image, int startTileID, int srcX, int srcY, int srcW, int srcH);

        void (*SetCompareColor)(Color color);
        void (*SetPixelFilter)(Uint16* filter);
        void (*SetDrawToScreen)(bool drawToScreen);

        void (*Line)(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Color color, int blendFlag);
        void (*Circle)(Subpixels x, Subpixels y, Subpixels radius, Color color, int blendFlag);
        void (*CircleStroke)(Subpixels x, Subpixels y, Subpixels radius, Color color, int blendFlag);
        void (*Ring)(Subpixels x, Subpixels y, Subpixels innerRadius, Subpixels outerRadius, Color color, int blendFlag);
        void (*Ellipse)(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Color color, int blendFlag);
        void (*Rectangle)(Subpixels x, Subpixels y, Subpixels w, Subpixels h, Color color, int blendFlag);
        void (*Triangle)(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Subpixels x3, Subpixels y3, Color color, int blendFlag);
        void (*Polygon)(Vector2* positions, Color color, int vertexCount, int blendFlag);
        void (*PolygonBlend)(Vector2* positions, Color* colors, int vertexCount, int blendFlag);
        void (*FadeScreen)(Color color, int rMult, int gMult, int bMult);
    } Draw;

    struct {
        bool (*Set)(Animator* animator, Resource sprite, int animationIndex, int frameIndex, bool resetFrame);
        bool (*Set3D)(Animator* animator, Resource mesh, int animationSpeed, int frameLoopIndex, int frameIndex, bool resetFrame);
        void (*Update)(Animator* animator);
        Hitbox* (*GetHitbox)(Animator* animator, int hitbox);
        int (*GetFrameID)(Animator* animator);
    } Animator;

    struct {
        void (*SetAmbientLighting)(Resource view3dIndex, Uint32 r, Uint32 g, Uint32 b);
        void (*SetDiffuseLighting)(Resource view3dIndex, Uint32 r, Uint32 g, Uint32 b);
        void (*SetSpecularLighting)(Resource view3dIndex, Uint32 r, Uint32 g, Uint32 b);
        void (*DrawBegin)(Resource view3dIndex);
        void (*DrawFinish)(Resource view3dIndex, Uint32 drawMode);
        void (*DrawModel)(Resource view3dIndex, Resource meshIndex, int frame, Matrix4x4* viewMatrix, Matrix4x4* normalMatrix, Color color);
    } View3D;

    struct {
        void  (*Load)(CString filename);
        Color (*GetColor)(int paletteIndex, int colorIndex);
        void  (*SetColor)(int paletteIndex, int colorIndex, Color color);
        void  (*MixPalettes)(int destPaletteIndex, int paletteIndexA, int paletteIndexB, int mixRatio, int colorIndexStart, int colorCount);
        void  (*RotateColorsLeft)(int paletteIndex, int colorIndexStart, int colorCount);
        void  (*RotateColorsRight)(int paletteIndex, int colorIndexStart, int colorCount);
        void  (*CopyColors)(int srcPaletteIndex, int srcColorIndexStart, int destPaletteIndex, int destColorIndexStart, int colorCount);
        void  (*SetPaletteIndexLines)(int paletteIndex, int lineStart, int lineEnd);
    } Palette;

    struct {
        void (*AddEntity)(int drawGroup, ::Entity* entity);
        void (*ReorderEntities)(int drawGroup, int entityIndexAbove, int entityIndexBelow, int maxEntityCount);
        void (*SetPrefixFunction)(int drawGroup, void (*prefixFunction)());
        void (*SetSorting)(int drawGroup, bool doSort);
    } DrawGroup;

    struct {
        void (*SetSize)(int viewIndex, int width, int height);
        void (*SetClip)(int viewIndex, int x, int y, int width, int height);
        void (*ResetClip)(int viewIndex);
    } View;

    // ===========
    // Resource Loading
    // ===========
    struct {
        Resource (*LoadSprite)(CString filename, int unloadPolicy);
        Resource (*LoadImage)(CString filename, int unloadPolicy);
        Resource (*LoadMesh)(CString filename, int unloadPolicy);
        Resource (*LoadView3D)(CString filename, int vertexCapacity, int unloadPolicy);
        Resource (*LoadSound)(CString filename);
    } Resources;

    // ===========
    // Scenes
    // ===========
    struct {
        int    (*GetCount)();
        Layer* (*GetLayerFromIndex)(int index);
        Layer* (*GetLayerFromName)(CString name);
        int    (*GetIndexFromLayer)(Layer* layer);
        int    (*GetIndexFromName)(CString name);
        Tile   (*GetTile)(Layer* layer, int x, int y);
        Tile*  (*GetTileLine)(Layer* layer, int y);
        void   (*GetSize)(int index, int* width, int* height);
    } Layer;

    struct {
        Uint8 (*GetAngle)(int tileID, int plane, int side);
        void  (*SetAngle)(int tileID, int plane, int side, Uint8 angle);
        Uint8 (*GetBehaviorFlag)(int tileID, int plane);
        void  (*SetBehaviorFlag)(int tileID, int plane, Uint8 flag);
    } TileConfig;

    struct {
        void    (*SetNext)(CString filename);
        void    (*SetNextFromCategory)(CString categoryName, CString sceneName);
        void    (*GotoNext)();
        CString (*GetName)();
    } Scene;

    struct {
        bool    (*MatchCurrentStageName)(CString substring);
        CString (*GetCurrentStageName)();
    } Stage;

    // ===========
    // Math
    // ===========
    struct {
        int  (*Sin256)(int n);
        int  (*Cos256)(int n);
        int  (*Tan256)(int n);
        int  (*Asin256)(int n);
        int  (*Acos256)(int n);
        int  (*Atan256)(int x, int y);

        int  (*Sin512)(int n);
        int  (*Cos512)(int n);
        int  (*Tan512)(int n);
        int  (*Asin512)(int n);
        int  (*Acos512)(int n);

        int  (*Sin1024)(int n);
        int  (*Cos1024)(int n);
        int  (*Tan1024)(int n);
        int  (*Asin1024)(int n);
        int  (*Acos1024)(int n);

        int  (*Sqrt)(Uint32 n);
        int  (*Distance)(int x1, int y1, int x2, int y2);
        int  (*Abs)(int n);
        int  (*Min)(int a, int b);
        int  (*Max)(int a, int b);
        int  (*Clamp)(int n, int min, int max);
        int  (*GetRandom)(int min, int max);
        void (*SetRandomSeed)(int seed);
        int  (*GetRandomSeeded)(int min, int max, int* seed);
    } Math;

    struct {
        void (*Identity)(Matrix4x4* matrix);
        void (*Multiply)(Matrix4x4* out, Matrix4x4* a, Matrix4x4* b);
        void (*Translate)(Matrix4x4* matrix, int x, int y, int z, bool resetToIdentity);
        void (*IdentityScale)(Matrix4x4* matrix, int x, int y, int z);
        void (*IdentityRotationX)(Matrix4x4* matrix, int x);
        void (*IdentityRotationY)(Matrix4x4* matrix, int y);
        void (*IdentityRotationZ)(Matrix4x4* matrix, int z);
        void (*IdentityRotationXYZ)(Matrix4x4* matrix, int x, int y, int z);
    } Matrix;

    // ===========
    // Audio
    // ===========
    struct {
        int      (*PlayStream)(CString filename, int streamIndex, int playbackIndex, int startAtSampleIndex, int loopAtSampleIndex);
        int      (*PlaySoundFX)(Resource sound, int loopAtSampleIndex, Uint8 priority);
        void     (*StopSoundFX)(Resource sound);
        bool     (*IsSoundFXPlaying)(Resource sound);
        void     (*PlaybackAlter)(int playbackIndex, float volume, float panning, float speed);
        bool     (*PlaybackIsValid)(int playbackIndex);
        int      (*PlaybackGetSamplePosition)(int playbackIndex);
        void     (*PlaybackStop)(int playbackIndex);
        void     (*PlaybackPause)(int playbackIndex);
        void     (*PlaybackResume)(int playbackIndex);
        void     (*PlaybackPauseAll)();
        void     (*PlaybackResumeAll)();
    } Audio;

    // ===========
    // Video
    // ===========
    struct {
        bool     (*PlayStream)(CString filename, double position, bool (*stateFunction)());
    } Video;

    // ===========
    // Input
    // ===========
    struct {
        int  (*GetX)(int touchIndex);
        int  (*GetY)(int touchIndex);
        bool (*IsDown)(int touchIndex);
        bool (*IsPressed)(int touchIndex);
        bool (*IsReleased)(int touchIndex);
    } Touch;

    struct {
        void (*Init)(::String* string, size_t length);
        void (*FromUnicode)(::String* string, Uint8 unicode);
        void (*FromCString)(::String* string, CString str, size_t length);
        void (*FromResource)(::String* string, CString filename, Uint8 encoding);
        void (*Copy)(::String* dst, ::String* src);
        void (*Concat)(::String* string, ::String* suffix);
        bool (*Match)(::String* stringA, ::String* stringB, bool caseSensitive);
        void (*ToCString)(char* str, ::String* string);
    } String;

    // ===========
    // Networking
    // ===========
    struct {
        int    (*Create)(); // Defaults to TCP, can be used with UDP, Bluetooth? UDP Broadcasting?
        int    (*Close)();
        // For clients
        int    (*Connect)(CString address);
        // For servers
        int    (*Bind)();
        int    (*Listen)();
        int    (*Accept)();

        size_t (*Read)(void* buffer, size_t size);
        size_t (*Write)(void* buffer, size_t size);
    } Socket;
} HatchFunctionSet;

typedef struct {
    struct {
        int  (*GetLocale)();
        bool (*GetConfirmButtonFlip)();

        bool (*IsMobile)();

        bool (*CanExitGame)();
        void (*ExitGame)();

        void (*Run)();

        void (*LaunchManual)();

        void (*GetSafeViewMargins)(int* x1, int* y1, int* x2, int* y2);

        bool (*ShowInputDeviceConfigOverlay)(Uint32 deviceID);

        bool (*IsEntitlementEnabled)(Uint32 extensionID);
        bool (*ShowEntitlementOverlay)(Uint32 extensionID);
    } Core;

    struct {
        bool (*ShowUserProfileOverlay)(CString userID, CString username);

        void (*UnlockAchievement)(void* infoStructPtr);
        bool (*GetAchievementsEnabled)();
        void (*SetAchievementsEnabled)(bool enabled);

        void (*UpdateRichPresence)(CString state, CString details, CString image, Sint64 timeStart, Sint64 timeEnd);
        void (*ClearRichPresence)();
    } UserData;

    struct {
        bool (*TryInitStorage)();
        int  (*GetStorageStatus)();
        int  (*GetStoragePermission)();
        void (*StoragePermissionReset)();
        void (*StoragePermissionRequestBegin)();
        void (*StoragePermissionRequestGrant)();
        void (*StoragePermissionRequestDeny)();
        void (*StoragePermissionRequestErrorOut)();
        void (*NoSaveModeEnable)(bool noSave);
        bool (*IsNoSaveModeEnabled)();
        bool (*ReadSaveFile)(CString filename, void* data, Uint32 dataSize, void (*resolve)(int code));
        bool (*WriteSaveFile)(CString filename, void* data, Uint32 dataSize, void (*resolve)(int code), bool compress);
        bool (*DeleteSaveFile)(CString filename, void (*resolve)(int code));
    } UserStorage;

    struct {
        int  (*IsSupported)();
        int  (*Init)();
        int  (*DiscoverPeers)(void (*onSuccess)(), void (*onFailure)(int errorCode));
        int  (*RequestPeers)(void (*onSuccess)());
        int  (*ConnectToPeer)(void (*onSuccess)());
    } WifiP2P;
} ServicesFunctionSet;

typedef struct {
    HatchFunctionSet* HatchFuncs;
    ServicesFunctionSet* ServiceFuncs;
    Entity** CurrentEntityPtr;
    GameState* GameStatePtr;
} LinkData;

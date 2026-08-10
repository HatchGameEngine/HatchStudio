#pragma once

#include <SDL2/SDL.h>

#include <UI/Components/Collections.hpp>

struct Stream;

namespace Studio {
    namespace Textures {
        bool CreateTextureFromImage(SDL_Texture** texture, Image* image);
        bool UpdateTextureFromImage(SDL_Texture** texture, Image* image);
        bool UpdateTextureFromData(SDL_Texture** texture, Uint8* data, Pixel* palette, int width, int height, SDL_Rect* rect = NULL);
        bool CreateTextureFromData(SDL_Texture** texture, Uint8* data, Pixel* palette, int width, int height);
        bool UpdateTextureFromSTBI(SDL_Texture** texture, unsigned char* data, int w, int h);
        bool CreateTextureFromSTBI(SDL_Texture** texture, unsigned char* data, int w, int h);
        bool CreateTextureFromFilePNG(SDL_Texture** texture, CString filename);
    }
}

enum {
    VAR_FLOAT = 15,
};

namespace Hatch {
    inline const char* GetPropertyTypeString(int type) {
        switch (type) {
            case VAR_BOOL:
                return "TOGGLE";
            case VAR_ENUM:
                return "OPTION";
            case VAR_INT8:
            case VAR_INT16:
            case VAR_INT32:
            case VAR_UINT8:
            case VAR_UINT16:
            case VAR_UINT32:
                return "INTEGER";
            case VAR_FLOAT:
                return "DECIMAL";
            case VAR_COLOR:
                return "COLOR";
            case VAR_VECTOR2:
                return "VECTOR2D";
            case VAR_STRING:
                return "TEXT";
        }
        return "UNKNOWN";
    }
}

namespace Resources {
    extern SDL_Texture* ImageTextures[MAX_IMAGES];

    Resource LoadImageWrapper(CString filename, int unloadPolicy);
    Resource LoadSpriteWrapper(CString filename, int unloadPolicy);
}

namespace Classes {
    struct EnumPair {
        CString name;
        int value;
    };
    struct ClassAttribute {
        Hash Name = { 0, 0, 0, 0 };
        char* NameString = NULL;
        size_t StructOffset = 0;
        int Using = true;
        int AttributeType = 0;
        List<EnumPair> EnumPairs;

        ClassAttribute() { }
        ClassAttribute(CString name);

        inline bool operator== (ClassAttribute& b) {
            return false;
        }
    };

    struct LinkedClass {
        Class* ObjectClass = NULL;
        List<ClassAttribute> Properties;

        inline bool operator== (LinkedClass& b) {
            return false;
        }
    };

    extern ClassAttribute ClassAttributes[0x100];
    extern int            ClassAttributeCount;

    extern List<LinkedClass*> LinkedClasses;
    extern LinkedClass* FocusedLinkedClass;

    void Add(CString className, void** staticObjectPtr, size_t entitySize, size_t staticObjectSize, void (*onStageLoad)(), void (*onEditorLoad)(), void (*onStaticUpdate)(), void (*onCreate)(CreateFlag flag), void (*onUpdate)(), void (*onUpdateLate)(), void (*onStageDraw)(), void (*onEditorDraw)(), void (*onSetup)(), void (*onStaticConstructor)(void* staticObject));
    void SetupAttribute(int attributeType, CString name, size_t offset);
    void AddEnumValue(CString name, int value);
}

namespace GameLinker {
    extern CString ClassNames[MAX_CLASSES];
}

struct ConfigPalette {
    Color  Palettes[MAX_PALETTE_COUNT][0x100];
    Uint32 UsedLines[MAX_PALETTE_COUNT];
};

struct NameIdentifier {
    char* Name;
    Hash  NameHash;
};

struct UsedClass {
    char* Name;
    Hash  NameHash;

    int LinkedClassIndex;

    List<Classes::ClassAttribute> Properties;
};
struct UsedSound {
    CString Name;
    int MaxPlaybacks;
};

namespace GameLinker {
    extern HatchFunctionSet HatchFuncs;
    extern ServicesFunctionSet ServiceFuncs;
    extern GameState State;
    extern Entity* CurrentEntity;

    extern void* GameLogicSharedObject;

    extern Class   ClassList[MAX_CLASSES];
    extern CString ClassNames[MAX_CLASSES];
    extern int     ClassCount;

    void Init();
    void Load(const char* projectFolder);
    void LinkExternalGameLogic(LinkData* linkData, const char* projectFolder);
}
namespace Graphics {
    enum Solidity {
        SOLID_NONE = 0,
        SOLID_PLATFORM = 1,
        SOLID_FALLTHROUGH = 2,
        SOLID_FULL = 3,
    };

    extern Vector2 DrawMinPos;
    extern Vector2 DrawMaxPos;

    extern int DrawCollision;

    extern SDL_Texture** TileImageData;
    extern SDL_Texture** TileCollisionImageData;

    void Init();
    void ResetHighlightBounds(Vector2 pos);
    void SetHighlightBounds(SDL_Rect dst);
    void View_SetSize(int viewIndex, int width, int height);
    void View_GetSize(int viewIndex, int* width, int* height);
    void DrawSprite(Resource sprite, int animation, int frame, Vector2* position);
    void DrawAnimation(Animator* animator, Vector2* position);
    void DrawImage(Resource image, Vector2* position);
    void DrawRectangle(Subpixels x, Subpixels y, Subpixels w, Subpixels h, Color color, int blendFlag);
    void DrawLine(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Color color, int blendFlag);
    void DrawCircle(Subpixels x, Subpixels y, Subpixels radius, Color color, int blendFlag);
    void DrawRing(Subpixels x, Subpixels y, Subpixels innerRadius, Subpixels outerRadius, Color color, int blendFlag);
    void DrawEllipse(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Color color, int blendFlag);
    void DrawTriangle(Subpixels x1, Subpixels y1, Subpixels x2, Subpixels y2, Subpixels x3, Subpixels y3, Color color, int blendFlag);
    void DrawPolygonBlend(Vector2* positions, Color* colors, int vertexCount, int blendFlag);
    void DrawPolygon(Vector2* positions, Color color, int vertexCount, int blendFlag);
    void DrawTile(Subpixels x, Subpixels y, Tile tile);
    void LayerDraw_Editor(Layer* layer);
    void DrawAll_Editor(int layerCount);
}

namespace Scene {
    extern Uint16* ClassIndexList;
    extern Uint32  ClassIndexCount;
};

namespace Game {
    extern GameState State;
}

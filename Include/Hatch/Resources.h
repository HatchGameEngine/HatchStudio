#pragma once

#include <Hatch/IO/Stream.h>

#define PREFIX_FILENAME(filename, folder) const size_t prefixLen = strlen(folder); \
    memcpy(Resources::BufferString, folder, prefixLen); \
    strncpy(&Resources::BufferString[prefixLen], filename, sizeof(Resources::BufferString) - prefixLen - 1);

namespace Resources {
    struct ResImage {
        Hash Name;
        int UnloadPolicy;
        Image ImageData;
    };
    struct ResSprite {
        Hash Name;
        int UnloadPolicy;
        Sprite SpriteData;
    };
    struct ResSound {
        Hash Name;
        int UnloadPolicy;
        Sound SoundData;
    };
    struct ResMesh {
        Hash Name;
        int UnloadPolicy;
        Mesh MeshData;
    };
    struct ResView3D {
        Hash Name;
        int UnloadPolicy;
        ArrayBuffer View3DData;
    };

    struct ResPack;
    struct ResPackFile {
        Hash     Filename;
        ResPack* Pack;

        Uint64   Offset;
        Uint64   Size;
        Uint32   DataFlag;
        Uint64   CompressedSize;
    };
    struct ResPack {
        ResPackFile* Files;
        int          FileCount;
        int          Priority;

        ::Stream*    Stream;
        bool         Loaded;
    };

    enum FileFlags {
        FILEFLAG_NONE,
        FILEFLAG_COMPRESSED,
        FILEFLAG_ENCRYPTED,
    };

    extern ResPack ResourcePacks[MAX_RESOURCE_PACKS];

    extern ResImage ResourceImages[MAX_IMAGES];
    extern ResSprite ResourceSprites[MAX_SPRITES];
    extern ResSound ResourceSounds[MAX_SOUNDS];
    extern ResMesh ResourceMeshes[MAX_MESHES];
    extern ResView3D ResourceView3Ds[MAX_ARRAY_BUFFERS];

    extern char BufferString[0x400];

    extern bool UseResourceFolder;
    extern const char* ResourceFolderPrefix;

    // Platform dependent
    bool PlatformInit();
    void PlatformDispose();

    // Common
    bool Init();
    void Dispose();

    Resource LoadImage(CString filename, int unloadPolicy);
    Resource LoadSprite(CString filename, int unloadPolicy);
    Resource LoadMesh(CString filename, int unloadPolicy);
    Resource LoadView3D(CString filename, int vertexCapacity, int unloadPolicy);
    Resource LoadSound(CString filename);
}

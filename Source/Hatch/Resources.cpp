#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Resources.h>

#include <Hatch/Hashing/MD5.h>
#include <Hatch/ImageFormats/GIF.h>
#include <Hatch/IO/FileStream.h>
#include <Hatch/IO/MemoryStream.h>
#include <Hatch/IO/ResourceStream.h>
#include <Hatch/Diagnostics.h>
#include <Hatch/Memory.h>

namespace Resources {
    ResPack ResourcePacks[MAX_RESOURCE_PACKS];

    ResImage ResourceImages[MAX_IMAGES];
    ResSprite ResourceSprites[MAX_SPRITES];
    ResSound ResourceSounds[MAX_SOUNDS];
    ResMesh ResourceMeshes[MAX_MESHES];
    ResView3D ResourceView3Ds[MAX_ARRAY_BUFFERS];
    char BufferString[0x400];

    bool UseResourceFolder;

    bool OpenResourceBundle(CString filename, int priority) {
        int emptyIndex = -1;
        for (int index = 0; index < MAX_RESOURCE_PACKS; index++) {
            ResPack* resource = &ResourcePacks[index];
            if (emptyIndex < 0 && !resource->Loaded) {
                emptyIndex = index;
                break;
            }
        }

        if (emptyIndex == -1) {
            Diagnostics::SetError("Too many resource packs in use!");
            return false;
        }

        Stream* stream = FileStream::New(filename, FileStream::READ_ACCESS);
        if (stream) {
            Uint8 magicHATCH[8];
            stream->ReadBytes(magicHATCH, 8);
            if (memcmp(magicHATCH, "HTCHLITE", 8)) {
                Diagnostics::SetError("Invalid HTCHLITE data file \"%s\"!", filename);
                stream->Close();
                return false;
            }

            // Uint8 major, minor, pad, pad;
            stream->ReadByte();
            stream->ReadByte();
            stream->ReadByte();
            stream->ReadByte();
            stream->ReadUInt16();

            ResPack* resource = &ResourcePacks[emptyIndex];
            resource->Priority = priority;
            resource->Loaded = true;

            resource->FileCount = stream->ReadUInt16();
            Memory::Alloc(&resource->Files, resource->FileCount * sizeof(ResPackFile), Memory::MEMPOOL_STAGE, false);

            for (int i = 0; i < resource->FileCount; i++) {
                ResPackFile* file = &resource->Files[i];
                file->Pack = resource;

                file->Filename.A = stream->ReadUInt32();
                file->Filename.B = stream->ReadUInt32();
                file->Filename.C = stream->ReadUInt32();
                file->Filename.D = stream->ReadUInt32();

                file->Offset = stream->ReadUInt64();
                file->Size = stream->ReadUInt64();
                file->DataFlag = stream->ReadUInt32();
                file->CompressedSize = stream->ReadUInt64();
            }

            resource->Stream = stream;
        }
        else {
            fprintf(stderr, "Couldn't open stream for %s!\n", filename);
            return false;
        }
		return true;
    }

    bool Init() {
        ZERO_OUT(ResourcePacks);

        ZERO_OUT(ResourceImages);
        ZERO_OUT(ResourceSprites);
        ZERO_OUT(ResourceSounds);
        ZERO_OUT(ResourceMeshes);
        ZERO_OUT(ResourceView3Ds);

        UseResourceFolder = true;
        #ifdef _IPHONE
        UseResourceFolder = false;
        #endif

        if (!PlatformInit())
            return false;

        if (!UseResourceFolder && !OpenResourceBundle("Data.hatch", 0))
            return false;

        return true;
    }
    void Dispose() {
        for (int index = 0; index < MAX_RESOURCE_PACKS; index++) {
            ResPack* resource = &ResourcePacks[index];
            if (resource->Loaded)
                resource->Stream->Close();

            resource->Files = NULL;
            resource->Stream = NULL;
            resource->Loaded = false;
        }

        PlatformDispose();
    }

    Resource LoadImage(CString filename, int unloadPolicy) {
        if (unloadPolicy < 0 || unloadPolicy > 2)
            return -1;

        Hash name = MD5_HashString(filename);

        int emptyIndex = -1;
        for (int index = 0; index < MAX_IMAGES; index++) {
            ResImage* resource = &ResourceImages[index];
            if (emptyIndex < 0 && !resource->UnloadPolicy)
                emptyIndex = index;

            if (resource->UnloadPolicy && resource->Name == name) {
                // Upgrade unload policy if needed.
                resource->UnloadPolicy = M_MAX(resource->UnloadPolicy, unloadPolicy);
                return index;
            }
        }

        if (emptyIndex > -1) {
            ResImage* resource = &ResourceImages[emptyIndex];
            resource->Name = name;

            PREFIX_FILENAME(filename, "Sprites/");

            Stream* stream = ResourceStream::New(BufferString);
            if (stream) {
                if (!GIF_Load(stream, &resource->ImageData)) {
                    fprintf(stderr, "Could not load sprite \"%s\"! (%s)\n", filename, Diagnostics::ErrorString);
                    stream->Close();
                    return -1;
                }
                stream->Close();
            }
            else {
                fprintf(stderr, "Couldn't open stream for %s!\n", filename);
                return -1;
            }

            resource->UnloadPolicy = unloadPolicy;
        }
        return emptyIndex;
    }
    Resource LoadSprite(CString filename, int unloadPolicy) {
        if (unloadPolicy < 0 || unloadPolicy > 2)
            return -1;

        Hash name = MD5_HashString(filename);

        int emptyIndex = -1;
        for (int index = 0; index < MAX_SPRITES; index++) {
            ResSprite* resource = &ResourceSprites[index];
            if (emptyIndex < 0 && !resource->UnloadPolicy)
                emptyIndex = index;

            if (resource->UnloadPolicy && resource->Name == name) {
                // Upgrade unload policy if needed.
                resource->UnloadPolicy = M_MAX(resource->UnloadPolicy, unloadPolicy);
                return index;
            }
        }

        if (emptyIndex > -1) {
            Resource sheets[16];
            ResSprite* resource = &ResourceSprites[emptyIndex];
            resource->Name = name;

            PREFIX_FILENAME(filename, "Sprites/");

            Stream* stream = ResourceStream::New(BufferString);
            if (stream) {
                char streamStringBuffer[256];
                if (stream->ReadUInt32() == 0x00525053) {
                    int totalFrameCount = stream->ReadInt32();
                    Memory::Alloc(&resource->SpriteData.Frames, totalFrameCount * sizeof(Frame), Memory::MEMPOOL_STAGE, false);

                    int totalSheetCount = stream->ReadByte();
                    for (int i = 0; i < totalSheetCount; i++) {
                        stream->ReadHeaderedString(streamStringBuffer);
                        sheets[i] = LoadImage(streamStringBuffer, unloadPolicy);
                    }

                    int hitboxCount = stream->ReadByte();
                    for (int i = 0; i < hitboxCount; i++) {
                        stream->Skip(stream->ReadByte()); // Skip over hitbox names
                    }

                    int animationCount = stream->ReadUInt16();
                    Memory::Alloc(&resource->SpriteData.Animations, animationCount * sizeof(Animation), Memory::MEMPOOL_STAGE, false);
                    resource->SpriteData.AnimationCount = animationCount;

                    Frame* currentFrame = resource->SpriteData.Frames;
                    Animation* currentAnimation = resource->SpriteData.Animations;

                    for (int a = 0; a < animationCount; a++) {
                        stream->ReadHeaderedString(streamStringBuffer);
                        currentAnimation->Name = MD5_HashString(streamStringBuffer);

                        currentAnimation->FrameCount = stream->ReadUInt16();
                        currentAnimation->Speed = stream->ReadUInt16();
                        currentAnimation->LoopFrameIndex = stream->ReadByte();

                        // 0: No rotation
                        // 1: Full rotation
                        // 2: Round to 45 degrees
                        // 3: Round to 90 degrees
                        // 4: Round to 180 degrees
                        // 5: Player rotation using extra frames
                        currentAnimation->RotationFlag = stream->ReadByte(); // Rotation Flags

                        currentAnimation->StartFrameIndex = (int)(currentFrame - resource->SpriteData.Frames);
                        for (int i = 0; i < currentAnimation->FrameCount; i++) {
                            currentFrame->Image = sheets[stream->ReadByte()];

                            currentFrame->Duration = stream->ReadInt16();
                            currentFrame->ID = stream->ReadUInt16();
                            currentFrame->SourceX = stream->ReadUInt16();
                            currentFrame->SourceY = stream->ReadUInt16();
                            currentFrame->Width = stream->ReadUInt16();
                            currentFrame->Height = stream->ReadUInt16();
                            currentFrame->OffsetX = stream->ReadInt16();
                            currentFrame->OffsetY = stream->ReadInt16();

                            if (hitboxCount) {
                                for (int h = 0; h < hitboxCount; h++) {
                                    currentFrame->Hitboxes[h].Left = stream->ReadInt16();
                                    currentFrame->Hitboxes[h].Top = stream->ReadInt16();
                                    currentFrame->Hitboxes[h].Right = stream->ReadInt16();
                                    currentFrame->Hitboxes[h].Bottom = stream->ReadInt16();
                                }
                            }
                            currentFrame++;
                        }
                        currentAnimation++;
                    }
                }

                stream->Close();
            }
            else {
                fprintf(stderr, "Couldn't open stream for %s!\n", filename);
                return -1;
            }

            resource->UnloadPolicy = unloadPolicy;
        }
        return emptyIndex;
    }
    Resource LoadMesh(CString filename, int unloadPolicy) {
        if (unloadPolicy < 0 || unloadPolicy > 2)
            return -1;

        Hash name = MD5_HashString(filename);

        int emptyIndex = -1;
        for (int index = 0; index < MAX_MESHES; index++) {
            ResMesh* resource = &ResourceMeshes[index];
            if (emptyIndex < 0 && !resource->UnloadPolicy)
                emptyIndex = index;

            if (resource->UnloadPolicy && resource->Name == name) {
                // Upgrade unload policy if needed.
                resource->UnloadPolicy = M_MAX(resource->UnloadPolicy, unloadPolicy);
                return index;
            }
        }

        if (emptyIndex > -1) {
            ResMesh* resource = &ResourceMeshes[emptyIndex];
            resource->Name = name;

            PREFIX_FILENAME(filename, "Meshes/");

            Stream* stream = ResourceStream::New(BufferString);
            if (stream) {
                if (stream->ReadUInt32SE() != 0x4D444C00) { // MDL0
                    stream->Close();
                    fprintf(stderr, "Model %s not of correct type!\n", filename);
                    return -1;
                }

                resource->MeshData.VertexType = stream->ReadByte();
                resource->MeshData.FaceVertexCount = stream->ReadByte();
                resource->MeshData.VertexCount = stream->ReadUInt16();
                resource->MeshData.FrameCount = stream->ReadUInt16();

                Uint8 VertexType = resource->MeshData.VertexType;
                Uint16 VertexCount = resource->MeshData.VertexCount;
                Uint16 FrameCount = resource->MeshData.FrameCount;

                if (VertexType & VertexType_Normal)
                    Memory::Alloc(&resource->MeshData.Positions, VertexCount * FrameCount * 2 * sizeof(Vector3), Memory::MEMPOOL_STAGE, false);
                else
                    Memory::Alloc(&resource->MeshData.Positions, VertexCount * FrameCount * sizeof(Vector3), Memory::MEMPOOL_STAGE, false);

                if (VertexType & VertexType_UV)
                    Memory::Alloc(&resource->MeshData.UVs, VertexCount * FrameCount * sizeof(Vector2), Memory::MEMPOOL_STAGE, false);

                if (VertexType & VertexType_Color)
                    Memory::Alloc(&resource->MeshData.Colors, VertexCount * FrameCount * sizeof(Color), Memory::MEMPOOL_STAGE, false);

                // Read UVs
                if (VertexType & VertexType_UV) {
                    int uvX, uvY;
                    for (int i = 0; i < VertexCount; i++) {
                        Vector2* uv = &resource->MeshData.UVs[i];
                        uv->X = uvX = (int)(stream->ReadFloat() * 0x10000);
                        uv->Y = uvY = (int)(stream->ReadFloat() * 0x10000);
                        // Copy the values to other frames
                        for (int f = 1; f < FrameCount; f++) {
                            uv += VertexCount;
                            uv->X = uvX;
                            uv->Y = uvY;
                        }
                    }
                }
                // Read Colors
                if (VertexType & VertexType_Color) {
                    stream->ReadBytes(resource->MeshData.Colors, VertexCount * sizeof(Color));

                    // Copy the value to other frames
                    for (int f = 1; f < FrameCount; f++) {
                        memcpy(&resource->MeshData.Colors[f * VertexCount], resource->MeshData.Colors, VertexCount * sizeof(Color));
                    }
                }

                resource->MeshData.VertexIndexCount = stream->ReadInt16();

                Sint16 VertexIndexCount = resource->MeshData.VertexIndexCount;
                Memory::Alloc(&resource->MeshData.VertexIndices, (VertexIndexCount + 1) * sizeof(Sint16), Memory::MEMPOOL_STAGE, false);

                stream->ReadBytes(resource->MeshData.VertexIndices, VertexIndexCount * sizeof(Sint16));
                resource->MeshData.VertexIndices[VertexIndexCount] = -1;

                if (VertexType & VertexType_Normal) {
                    Vector3* vert = resource->MeshData.Positions;
                    int totalVertexCount = VertexCount * FrameCount;

                    for (int v = 0; v < totalVertexCount; v++) {
                        vert->X = (int)(stream->ReadFloat() * 0x100);
                        vert->Y = (int)(stream->ReadFloat() * 0x100);
                        vert->Z = (int)(stream->ReadFloat() * 0x100);
                        vert++;

                        vert->X = (int)(stream->ReadFloat() * 0x10000);
                        vert->Y = (int)(stream->ReadFloat() * 0x10000);
                        vert->Z = (int)(stream->ReadFloat() * 0x10000);
                        vert++;
                    }
                }
                else {
                    Vector3* vert = resource->MeshData.Positions;
                    int totalVertexCount = VertexCount * FrameCount;
                    for (int v = 0; v < totalVertexCount; v++) {
                        vert->X = (int)(stream->ReadFloat() * 0x100);
                        vert->Y = (int)(stream->ReadFloat() * 0x100);
                        vert->Z = (int)(stream->ReadFloat() * 0x100);
                        vert++;
                    }
                }

                stream->Close();
            }
            else {
                fprintf(stderr, "Couldn't open stream for %s!\n", filename);
                return -1;
            }

            resource->UnloadPolicy = unloadPolicy;
        }
        return emptyIndex;
    }
    Resource LoadView3D(CString filename, int vertexCapacity, int unloadPolicy) {
        if (unloadPolicy < 0 || unloadPolicy > 2)
            return -1;

        Hash name = MD5_HashString(filename);

        int emptyIndex = -1;
        for (int index = 0; index < MAX_ARRAY_BUFFERS; index++) {
            ResView3D* resource = &ResourceView3Ds[index];
            if (emptyIndex < 0 && !resource->UnloadPolicy)
                emptyIndex = index;

            if (resource->UnloadPolicy && resource->Name == name) {
                // Upgrade unload policy if needed.
                resource->UnloadPolicy = M_MAX(resource->UnloadPolicy, unloadPolicy);
                return index;
            }
        }

        if (emptyIndex > -1) {
            ResView3D* resource = &ResourceView3Ds[emptyIndex];
            resource->Name = name;

            vertexCapacity = M_MIN(vertexCapacity, 0x4000);

            resource->View3DData.VertexCapacity = vertexCapacity;
            resource->View3DData.PerspectiveBitshiftX = 8;
            resource->View3DData.PerspectiveBitshiftY = 8;
            Memory::Alloc(&resource->View3DData.VertexBuffer, vertexCapacity * sizeof(VertexAttribute), Memory::MEMPOOL_STAGE, true);
            Memory::Alloc(&resource->View3DData.FaceSizeBuffer, (vertexCapacity >> 1) * sizeof(Uint8), Memory::MEMPOOL_STAGE, true);
            Memory::Alloc(&resource->View3DData.FaceInfoBuffer, (vertexCapacity >> 1) * sizeof(FaceInfo), Memory::MEMPOOL_STAGE, true);

            resource->UnloadPolicy = unloadPolicy;
        }
        return emptyIndex;
    }
    Resource LoadSound(CString filename) {
        Hash name = MD5_HashString(filename);
        for (int index = 0; index < MAX_SOUNDS; index++) {
            ResSound* resource = &ResourceSounds[index];
            if (resource->UnloadPolicy && resource->Name == name) {
                return index;
            }
        }
        return -1;
    }
}

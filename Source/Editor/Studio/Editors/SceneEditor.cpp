#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Hashing/MD5.h>
#include <Hatch/Hashing/Murmur.h>
#include <Hatch/IO/Stream.h>
#include <Hatch/IO/FileStream.h>

#include <Hatch/Diagnostics.h>
#include <Hatch/GameLinker.h>
#include <Hatch/Graphics.h>
#include <Hatch/Math.h>
#include <Hatch/Memory.h>
#include <Hatch/Scene.h>
#include <Hatch/Strings.h>

#include <Studio/Impl.hpp>

#include <UI/Graphics/Font.hpp>
#include <UI/Graphics/Renderer.hpp>

// Control imports
#include <UI/Controls/Control.hpp>

#include <UI/Controls/Button.hpp>
#include <UI/Controls/ComboBox.hpp>
#include <UI/Controls/Container.hpp>
#include <UI/Controls/Form.hpp>
#include <UI/Controls/Label.hpp>
#include <UI/Controls/ListView.hpp>
#include <UI/Controls/PropertyGrid.hpp>
#include <UI/Controls/SplitContainer.hpp>
#include <UI/Controls/ToolStrip.hpp>
#include <UI/Filesystem/Paths.hpp>
#include <UI/System/SystemDialog.hpp>

#include <Studio/Subcontrols/TileCollisionEditorPanel.hpp>
#include <Studio/Subcontrols/TileSelector.hpp>

#include <Studio/Editors/SceneEditor.hpp>

#define MAGIC_SCENE_HSCN 0x4E435348
#define MAGIC_SCENE_RSDK 0x004E4353

// Reset the state for a new file
void SceneEditor::Init() {
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
void SceneEditor::New() {
    Init();

    LinkedStage = new Stage();
    tileSelector->SetTileset(&LinkedStage->Tileset);
    tileCollisionEditor->SetTileset(&LinkedStage->Tileset);

    LayerNew(0);
    tilePlacementField->CurrentLayer = 0;

    // Update UI
    objectClasses->UpdateClassList();

    Strings::FromCString(&FilePath, "CurrentScene.HSCN", 0);
    SetTitle("CurrentScene.HSCN");

    SetChangesSaved();
    JustCreated = true;
}

bool SceneEditor::Read_RSDK(Stream* stream) {
    // Scene.BIN
    // Check loaded Stage list by filename for matching <path>/StageConfig.bin, if not found,
    // Load Stage at <path>/StageConfig.bin and add to loaded Stage list, if not found,
    // Prompt "StageConfig not found, create new StageConfig!", do it and link if yes, if no,
    // Give up. Do not load the scene.
    Uint32 objectDefinitionCount;
    char streamStringBuffer[256];

    // Signature checking
    if (stream->ReadUInt32() == MAGIC_SCENE_RSDK) {
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
bool SceneEditor::Read_HatchTiled(Stream* stream) {
    // .TMX
    // Creates its own LinkedStage
    return false;
}
bool SceneEditor::Read_HatchLite(Stream* stream) {
    char streamStringBuffer[256];
    // .HSCN
    // Read String to get the resource path of the stage.
    // Check loaded Stage list by resource path, if not found,
    // Load Stage at resource path and add to loaded Stage list, if not found,
    // Prompt "StageInfo not found, create new StageInfo!", do it and link if yes, if no,
    // Give up. Do not load the scene.

    // Read magic
    if (stream->ReadUInt32() != MAGIC_SCENE_HSCN) {
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
bool SceneEditor::Write_HatchLite(Stream* stream) {
    char streamStringBuffer[256];
    // .HSCN

    // Read magic
    stream->WriteUInt32(MAGIC_SCENE_HSCN);

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

bool SceneEditor::Open() {
    char filename[256];
    char stringBuffer[256];
    Strings::ToCString(filename, &FilePath);

    Init();

    // If RSDK
    if (FileType == ResourceFileType::Scene_RSDKv5) {
        LinkedStage = new Stage();

        // Should load GameConfig before StageConfig

        // Load Stage info
        if (!LinkedStage->LoadConfig(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "StageConfig.bin"))) {
            fprintf(stderr, "LoadConfig failed with reason: %s\n", Diagnostics::ErrorString);
            return false;
        }

        // Load tile collisions & info
        if (!LinkedStage->Tileset.OpenTileConfig(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "TileConfig.bin"))) {
            fprintf(stderr, "OpenTileConfig failed with reason: %s\n", Diagnostics::ErrorString);
            return false;
        }

        // Load tile image data & hashes
        if (!LinkedStage->Tileset.LoadTileset_RSDK(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "16x16Tiles.gif"))) {
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
    else if (FileType == ResourceFileType::Scene_Tiled) {
        LinkedStage = new Stage();
    }
    // If HatchLite
    else if (FileType == ResourceFileType::Scene_HatchLite) {
        // If cannot find LinkedStage in memory, make new LinkedStage
        LinkedStage = new Stage();

        // Load tile collisions & info "TileInfo.HCOL"
        if (!LinkedStage->Tileset.OpenTileConfig(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "TileCol.bin"))) {
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
    tileSelector->SetTileset(&LinkedStage->Tileset);
    tileCollisionEditor->SetTileset(&LinkedStage->Tileset);

    if (LayerCount > 0) {
        if (tilePlacementField->CurrentLayer >= 0)
            layerControls->listViewLayers->Select(tilePlacementField->CurrentLayer);
        else
            layerControls->listViewLayers->Select(0);
    }

    return true;
}
bool SceneEditor::Save() {
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

    LinkedStage->Tileset.SaveTileConfig(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "TileCol.bin"));
    StampCollectionSave(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "Stamps.HSTM"));
    TilesetSave(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "Tileset.png"));

    SetChangesSaved();
    JustCreated = false;
    return true;
}

ResourceFileType SceneEditor::GetFileType(Stream* stream) {
    Uint32 magic = stream->ReadUInt32();

    stream->Skip(-sizeof(Uint32));

    switch (magic) {
    case MAGIC_SCENE_HSCN:
        return ResourceFileType::Scene_HatchLite;
    case MAGIC_SCENE_RSDK:
        return ResourceFileType::Scene_RSDKv5;
    default:
        break;
    }

    return ResourceFileType::Unknown;
}

int SceneEditor::GetEditorType() {
    return EditorTypes::SCENE;
}

bool SceneEditor::PromptImportTileset() {
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

bool SceneEditor::TilesetImport(List<char*>& filenames) {
    if (LinkedStage->Tileset.Import(filenames, &Stamps)) {
        // Update tileSelector
        tileSelector->ResizeChildren();

        StampCollectionUpdateUI();

        return true;
    }

    return false;
}
bool SceneEditor::TilesetOpen(CString filename) {
    List<char*> filenames;
    filenames.Add((char*)filename);
    return TilesetImport(filenames);
}
bool SceneEditor::TilesetSave(CString filename) {
    return LinkedStage->Tileset.Save(filename);
}

// Data Functions
void SceneEditor::LayerNew(int layerIndex) {
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
void SceneEditor::LayerRemove(int layerIndex, bool shift) {
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
void SceneEditor::LayerShiftDown(int startLayerIndex, int endLayerIndex) {
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
void SceneEditor::LayerCopy(int dstIndex, int srcIndex) {
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
void SceneEditor::LayerSwap(int dstIndex, int srcIndex) {
    LayerCopy(LayerCapacity, srcIndex);
    LayerCopy(srcIndex, dstIndex);
    LayerCopy(dstIndex, LayerCapacity);
}
void SceneEditor::LayerRename(int layerIndex, CString name) {
    Layer* layer = &Layers[layerIndex];

    Strings::FromCString(&LayerNames[layerIndex], name, 0);
    layer->Name = MD5_HashString(name);

    layerControls->UpdateList();
}
void SceneEditor::LayerRename(int layerIndex, String* name) {
    char nameBuffer[128];
    Strings::ToCString(nameBuffer, name);

    Layer* layer = &Layers[layerIndex];

    Strings::FromCString(&LayerNames[layerIndex], nameBuffer, 0);
    layer->Name = MD5_HashString(nameBuffer);



    layerControls->UpdateList();
}
void SceneEditor::LayerResize(int layerIndex, int width, int height) {
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
void SceneEditor::LayerResizeParallaxInfoCount(int layerIndex, int count) {
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
void SceneEditor::LayerRemapAllTiles() {
    for (int l = 0; l < LayerCount; l++) {
        Layer* layer = &Layers[l];
        int rowLength = layer->Width;
        for (int row = 0; row < layer->Height; row++) {
            Tile* tileRow = &layer->Tiles[row << layer->WidthInBits];
            for (int col = 0; col < layer->Width; col++) {
                if (tileRow[col] == TILE_EMPTY)
                    continue;

                int newID = LinkedStage->Tileset.TileRemapArray[tileRow[col].ID];
                if (newID == -1)
                    tileRow[col] = TILE_EMPTY;
                else
                    tileRow[col].ID = newID;
            }
        }
    }

    tilePlacementField->UpdateRenderTarget = true;
}

void SceneEditor::StampCollectionUpdateUI() {
    stampCollection->UpdateList();
}
void SceneEditor::StampCollectionAdd(const char* title, Stamp* stamp) {
    SavedStamp* savedStamp = new SavedStamp();
    Strings::FromCString(&savedStamp->Title, title, 0);
    savedStamp->Data = stamp;
    Stamps.Add(savedStamp);

    StampCollectionUpdateUI();
}
void SceneEditor::StampCollectionDuplicate(int index) {
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
void SceneEditor::StampCollectionClear() {
    Stamps.Clear();

    StampCollectionUpdateUI();
}
void SceneEditor::StampCollectionOpen(CString filename) {
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
void SceneEditor::StampCollectionSave(CString filename) {
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

void SceneEditor::EntityUpdateUI() {
    entityProperties->UpdateEntityList();

    tilePlacementField->UpdateRenderTarget = true;
}
void SceneEditor::EntityAdd(int classID) {
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
void SceneEditor::EntityRemove(int slot) {
    if (entityProperties->propertyGridEntity->SelectedEntity == &EntitySlots[slot])
        entityProperties->propertyGridEntity->SelectedEntity = NULL;

    ActionStack_Do(new EntityRemoveCommand(this, slot), tilePlacementField->ActionSiblingKeyID << 8);
}
void SceneEditor::EntityRemapClasses() {
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
void SceneEditor::EntitySelectAllOfClass(int classID) {
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
void SceneEditor::EntitySelectAll() {
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
int  SceneEditor::EntityGetSlot(Entity* entity) {
    return (EntitySlot*)entity - &EntitySlots[0];
}

void SceneEditor::ClassUpdateUI() {
    objectClasses->UpdateClassList();
}
void SceneEditor::ClassRemove(int classID) {
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

void SceneEditor::ClassUpdatePropertyUI() {
    entityProperties->propertyGridEntity->UpdatePropertyUI();
    objectClasses->UpdatePropertyList();
}
bool SceneEditor::ClassHasProperty(int classID, CString propertyName) {
    UsedClass* usedClass = LinkedStage->Classes[classID];
    Hash propertyNameHash = MD5_HashString(propertyName);
    for (int i = 0; i < usedClass->Properties.Count(); i++) {
        if (usedClass->Properties[i].Name == propertyNameHash)
            return true;
    }
    return false;
}
void SceneEditor::ClassAddProperty(int classID, CString propertyName, int propertyType) {
    UsedClass* usedClass = LinkedStage->Classes[classID];
    usedClass->Properties.Add(Classes::ClassAttribute { });

    Classes::ClassAttribute* newProperty = new (&usedClass->Properties.Items[usedClass->Properties.Count() - 1]) Classes::ClassAttribute(propertyName);
    newProperty->AttributeType = propertyType;

    ClassUpdatePropertyUI();
}
void SceneEditor::ClassRemoveProperty(int classID, Hash propertyNameHash) {
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
void SceneEditor::ClassRemoveProperty(int classID, CString propertyName) {
    ClassRemoveProperty(classID, MD5_HashString(propertyName));
}

// Action / Command Stack Functions
UndoRedoStack* actions = NULL;
void SceneEditor::ActionStack_Do(Command* cmd, int siblingID) {
    actions->Do(cmd, siblingID);

    SetChangesUnsaved();
}
void SceneEditor::ActionStack_Undo() {
    actions->Undo();
}
void SceneEditor::ActionStack_Redo() {
    actions->Redo();
}
void SceneEditor::ActionStack_Clear() {
    actions->Reset();
}

// UI Functions
SceneEditor::SceneEditor() : ResourceEditor() {
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
    tileSelector = new TileSelector(NULL);
    stampCollection = new StampCollection(this);
    tileCollisionEditor = new TileCollisionEditorPanel(this);
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
            tileCollisionEditor->SetTileset(&LinkedStage->Tileset);
            LinkedStage->Tileset.RemapTileConfig();
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
    splitterMain->SplitterDistance = tileSelector->Padding.Horizontal() + tileSelector->TileSpaceH * 16 + 16;
    splitterMain->BackColor = Color(0x000000, 0x00);
    splitterMain->Panel1->BackColor = Color(0x000000, 0x00);
    splitterMain->Panel2->BackColor = Color(0x000000, 0x00);
    splitterMain->FixedPanel = SplitPanelFix::Panel1;

    splitterMain->Panel1->Controls.Add(leftTab);
    splitterMain->Panel2->Controls.Add(splitterField);
    splitterMain->Panel1MinSize = tileSelector->Padding.Horizontal() + tileSelector->TileWidth + 16;

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
SceneEditor::~SceneEditor() {
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

void SceneEditor::LinkScene() {
	// Link currently active scene
	Scene::Layers = this->Layers;

	if (this->LinkedStage) {
		Graphics::TileImageData = this->LinkedStage->Tileset.TileImageTexture;
		Graphics::TileCollisionImageData = this->LinkedStage->Tileset.TileCollisionTextures;
	}

	Scene::CurrentEntity = this->CurrentEntity;
	Scene::EntitySlots = this->EntitySlots;
	Scene::ClassIndexList = this->ClassIndexList;
	Scene::ClassIndexCount = this->ClassIndexCount;
}

void SceneEditor::Update() {
	LinkScene();

    Control::Update();
}
void SceneEditor::Render() {
	LinkScene();
    Control::Render();
}

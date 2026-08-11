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
#include <Studio/StageTileset.hpp>

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
    char CurrentStage[16];
    bool UseGlobalClasses = false;

    ConfigPalette StageConfigPalette;

    std::vector<UsedClass*> Classes;
    std::vector<UsedSound> Sounds;

    StageTileset Tileset;

    Stage() {
        Classes.clear();
        Sounds.clear();
    }
    ~Stage() {
        for (int i = 0; i < Classes.size(); i++) {
            delete Classes[i];
        }
        Classes.clear();
        Sounds.clear();
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

    // "Load" is for the first time the Stage is loaded

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

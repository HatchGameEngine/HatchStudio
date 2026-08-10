#pragma once

#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/IO/Stream.h>
#include <Hatch/IO/FileStream.h>

#include <Hatch/Diagnostics.h>
#include <Hatch/GameLinker.h>
#include <Hatch/Graphics.h>
#include <Hatch/Math.h>
#include <Hatch/Memory.h>
#include <Hatch/Resources.h>
#include <Hatch/Scene.h>
#include <Hatch/Strings.h>

#include <vector>

#include <Studio/Impl.hpp>
#include <Studio/Structs.hpp>

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
#include <UI/Controls/NumericUpDownBox.hpp>
#include <UI/Controls/PropertyGrid.hpp>
#include <UI/Controls/RadialKnob.hpp>
#include <UI/Controls/ScrollBar.hpp>
#include <UI/Controls/SplitContainer.hpp>
#include <UI/Controls/Textbox.hpp>
#include <UI/Controls/ToolStrip.hpp>
#include <UI/Controls/ToolTip.hpp>
#include <UI/Filesystem/Paths.hpp>
#include <UI/System/Application.hpp>

#include <Studio/Subcontrols/TileCollisionEditorPanel.hpp>
#include <Studio/Subcontrols/TileSelector.hpp>

#include <Studio/Editors/ResourceEditor.hpp>
#include <Studio/Project.hpp>

struct SceneEditor : Studio::ResourceEditor {
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
    void Init();

    // For creating a new scene file from scratch
    void New();

    const Version HSCN_VERSION = { 0, 1, 1 };
    bool Read_RSDK(Stream* stream);
    bool Read_HatchTiled(Stream* stream);
    bool Read_HatchLite(Stream* stream);
    bool Write_HatchLite(Stream* stream);

    bool Open();
    bool Save();

    int GetEditorType();

    bool PromptImportTileset();

    bool TilesetImport(List<char*>& filenames);
    bool TilesetOpen(CString filename);
    bool TilesetSave(CString filename);

    // Data Functions
    void LayerNew(int layerIndex);
    void LayerRemove(int layerIndex, bool shift);
    void LayerShiftDown(int startLayerIndex, int endLayerIndex);
    void LayerCopy(int dstIndex, int srcIndex);
    void LayerSwap(int dstIndex, int srcIndex);
    void LayerRename(int layerIndex, CString name);
    void LayerRename(int layerIndex, String* name);
    void LayerResize(int layerIndex, int width, int height);
    void LayerResizeParallaxInfoCount(int layerIndex, int count);
    void LayerRemapAllTiles();

    void StampCollectionUpdateUI();
    void StampCollectionAdd(const char* title, Stamp* stamp);
    void StampCollectionDuplicate(int index);
    void StampCollectionClear();
    void StampCollectionOpen(CString filename);
    void StampCollectionSave(CString filename);

    void EntityUpdateUI();
    void EntityAdd(int classID);
    void EntityRemove(int slot);
    void EntityRemapClasses();
    void EntitySelectAllOfClass(int classID);
    void EntitySelectAll();
    int  EntityGetSlot(Entity* entity);

    void ClassUpdateUI();
    void ClassRemove(int classID);

    void ClassUpdatePropertyUI();
    bool ClassHasProperty(int classID, CString propertyName);
    void ClassAddProperty(int classID, CString propertyName, int propertyType);
    void ClassRemoveProperty(int classID, Hash propertyNameHash);
    void ClassRemoveProperty(int classID, CString propertyName);

    // Action / Command Stack Functions
    UndoRedoStack* actions = NULL;
    void ActionStack_Do(Command* cmd, int siblingID);
    void ActionStack_Undo();
    void ActionStack_Redo();
    void ActionStack_Clear();

    // UI stuffs
    TileSelector* tileSelector;
    ObjectClasses* objectClasses;
    StampCollection* stampCollection;
    TilePlacementField* tilePlacementField;
    TileCollisionEditorPanel* tileCollisionEditor;
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
    SceneEditor();
    ~SceneEditor();

	void LinkScene();

    void Update();
    void Render();
};

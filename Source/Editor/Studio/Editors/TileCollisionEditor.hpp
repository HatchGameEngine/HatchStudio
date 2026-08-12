#pragma once

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/IO/Stream.h>

#include <vector>

#include <Studio/Enums.hpp>
#include <Studio/Impl.hpp>
#include <Studio/StageTileset.hpp>
#include <Studio/Structs.hpp>

#include <UI/Controls/Button.hpp>
#include <UI/Controls/Label.hpp>

#include <Studio/Subcontrols/TileCollisionEditorPanel.hpp>

#include <Studio/Editors/ResourceEditor.hpp>

struct TileCollisionEditor : Studio::ResourceEditor {
    /// File IO functions

    void New();

    bool Open();
    bool Save();

    static ResourceFileType GetFileType(Stream* stream);

    int GetEditorType();

    bool PromptSetImage();

    // UI stuffs
    TileCollisionEditorPanel* tileCollisionEditorPanel = NULL;
    Label* labelOptions = NULL;
    Button* buttonSetImage = NULL;
    Button* buttonTileCount = NULL;

    // UI Functions
    TileCollisionEditor();
    ~TileCollisionEditor();

    void Render();

private:
    StageTileset* Tileset = NULL;
};

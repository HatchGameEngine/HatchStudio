#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/IO/Stream.h>
#include <Hatch/IO/FileStream.h>

#include <Hatch/Diagnostics.h>
#include <Hatch/Graphics.h>
#include <Hatch/Strings.h>

#include <Studio/Impl.hpp>

#include <UI/Filesystem/Paths.hpp>
#include <UI/System/SystemDialog.hpp>

#include <Studio/Subcontrols/TileCollisionEditorPanel.hpp>

#include <Studio/Editors/TileCollisionEditor.hpp>

void TileCollisionEditor::New() {
    Strings::FromCString(&FilePath, "TileCol.bin", 0);
    SetTitle("TileCol.bin");

    Tileset = new StageTileset();
    Tileset->TileCount = 128;

    tileCollisionEditorPanel->SetTileset(Tileset);

    SetChangesSaved();
    JustCreated = true;
}

bool TileCollisionEditor::Open() {
    char filename[256];
    char stringBuffer[256];
    Strings::ToCString(filename, &FilePath);

    Tileset = new StageTileset();

    Stream* stream = FileStream::New(filename, FileStream::READ_ACCESS);
    if (!stream) {
        fprintf(stderr, "TileCollisionEditor::Open failed with reason: %s\n", Diagnostics::ErrorString);
        return false;
    }

    bool didLoad = false;

    switch (FileType) {
    case ResourceFileType::TileCol_Hatch:
        didLoad = Tileset->ReadTileConfig_Hatch(stream);
        break;
    case ResourceFileType::TileCol_RSDKv5:
        didLoad = Tileset->ReadTileConfig_RSDK(stream);
        break;
    }

    stream->Close();

    if (!didLoad || !Tileset->UpdateTileCollisionTexture_All()) {
        return false;
    }

    // TODO: This should check if Tileset.png exists before trying to load it
    Tileset->Load(UI::Filesystem::Paths::GetSiblingFilePath(stringBuffer, filename, "Tileset.png"));

    tileCollisionEditorPanel->SetTileset(Tileset);

    return true;
}
bool TileCollisionEditor::Save() {
    char filename[256];
    Strings::ToCString(filename, &FilePath);

    if (Tileset && !Tileset->SaveTileConfig(filename)) {
        fprintf(stderr, "SaveTileConfig failed with reason: %s\n", Diagnostics::ErrorString);
        return false;
    }

    SetChangesSaved();
    JustCreated = false;
    return true;
}

ResourceFileType TileCollisionEditor::GetFileType(Stream* stream) {
    Uint32 magic = stream->ReadUInt32();

    stream->Skip(-sizeof(Uint32));

    switch (magic) {
    case MAGIC_TILESET_RSDK:
        return ResourceFileType::TileCol_RSDKv5;
    case MAGIC_TILESET_HATCH:
        return ResourceFileType::TileCol_Hatch;
    default:
        break;
    }

    return ResourceFileType::Unknown;
}

int TileCollisionEditor::GetEditorType() {
    return EditorTypes::TILECONFIG;
}

// UI Functions
TileCollisionEditor::TileCollisionEditor() : ResourceEditor() {
    Dock = DOCK_FILL;
    Padding = 0;

    BackColor = Color(0x21252B, 0xFF);

    tileCollisionEditorPanel = new TileCollisionEditorPanel((StageTileset*)NULL);

    int optionsLabelPos = tileCollisionEditorPanel->tilePreviewWindow->Location.Y + tileCollisionEditorPanel->tilePreviewWindow->Size.Get().H;

    labelOptions = new Label("Options:");
    labelOptions->Location = { 8, optionsLabelPos + 28 };

    // TODO: Center the buttons
    buttonSetImage = new Button();
    buttonSetImage->Location = { 8, labelOptions->Location.Y + 28 };
    buttonSetImage->Size = { 200, 25 };
    buttonSetImage->SetText("Set Image...");
    buttonSetImage->onClick += [this](auto* a, auto* d) -> void {
        if (Tileset != NULL && PromptSetImage()) {
            tileCollisionEditorPanel->buttonSetCollisionForSelectedRange->Enabled = true;
        }
    };

    // TODO: Implement this.
    buttonTileCount = new Button();
    buttonTileCount->Location = { 8, buttonSetImage->Location.Y + 28 };
    buttonTileCount->Size = { 200, 25 };
    buttonTileCount->SetText("Set Tile Count...");
    buttonTileCount->Enabled = false;

    tileCollisionEditorPanel->splitter->Panel2->Controls.Add(labelOptions);
    tileCollisionEditorPanel->splitter->Panel2->Controls.Add(buttonSetImage);
    tileCollisionEditorPanel->splitter->Panel2->Controls.Add(buttonTileCount);
    tileCollisionEditorPanel->splitter->Orientation = SplitOrientation::Horizontal;
    tileCollisionEditorPanel->splitter->SplitterWidth = 4;
    tileCollisionEditorPanel->splitter->IsSplitterFixed = false;
    tileCollisionEditorPanel->splitter->Panel2MinSize = 300;

    Controls.Add(tileCollisionEditorPanel);
}
TileCollisionEditor::~TileCollisionEditor() {
    delete tileCollisionEditorPanel;
    delete labelOptions;
    delete buttonSetImage;
    delete buttonTileCount;
    delete Tileset;
}

bool TileCollisionEditor::PromptSetImage() {
    UI::SystemDialog::OpenFileData ofd;
    ofd.Title = "Open Tileset Image Files...";
    ofd.FilterPatterns.Add("*.gif");
    ofd.FilterPatterns.Add("*.png");
    ofd.Multiselect = false;

    if (UI::SystemDialog::OpenFile(&ofd) && Tileset->Load(ofd.Filenames[0])) {
        // TODO: This should ask the user if they want to change the tile count
        // TODO: Tile count changes should properly bound TileSelector's current selection.
        tileCollisionEditorPanel->SetTileset(Tileset);

        SetChangesUnsaved();

        return true;
    }

    return false;
}

void TileCollisionEditor::Render() {
    if (Tileset) {
        Graphics::TileImageData = Tileset->TileImageTexture;
        Graphics::TileCollisionImageData = Tileset->TileCollisionTextures;
    }

    Control::Render();
}

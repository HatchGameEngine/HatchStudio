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

#include <UI/Controls/DialogBox.hpp>
#include <UI/Filesystem/Paths.hpp>
#include <UI/System/Application.hpp>
#include <UI/System/SystemDialog.hpp>

#include <Studio/Subcontrols/TileCollisionEditorPanel.hpp>

#include <Studio/Editors/TileCollisionEditor.hpp>

void TileCollisionEditor::New() {
    Strings::FromCString(&FilePath, "TileCol.bin", 0);
    SetTitle("TileCol.bin");

    Tileset = new StageTileset();
    Tileset->TileCount = 128;

    tileCollisionEditorPanel->SetTileset(Tileset);
    UpdateTileCountLabel();

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

    tileCollisionEditorPanel->SetTileset(Tileset);
    UpdateTileCountLabel();

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
struct Form_TileCountDialog : Form {
    Label* labelTileCount;
    NumericUpDown* numericUpDownBoxTileCount;
    Button* buttonOK;
    Button* buttonCancel;

    FlowLayoutPanel* mainPanel;

    Form_TileCountDialog() : Form(250, 140, "") {
        mainPanel = new FlowLayoutPanel();
        mainPanel->BackColor = Color(0x000000, 0x00);
        mainPanel->Dock = DOCK_FILL;
        mainPanel->FlowDirection = FlowDirection::LEFT_TO_RIGHT;
        mainPanel->Padding = 10;
        mainPanel->WrapContents = true;

        labelTileCount = new Label("Tile Count:");
        labelTileCount->Anchor = ANCHOR_TOP;
        labelTileCount->Margin.Top = 5;
        labelTileCount->Margin.Right = 10;
        mainPanel->Controls.Add(labelTileCount);

        numericUpDownBoxTileCount = new NumericUpDown();
        numericUpDownBoxTileCount->Anchor = ANCHOR_TOP;
        numericUpDownBoxTileCount->Minimum = 1.0f;
        numericUpDownBoxTileCount->Maximum = 4096.0f;
        numericUpDownBoxTileCount->Size = { 100, 25 };
        numericUpDownBoxTileCount->LineBreak = true;
        mainPanel->Controls.Add(numericUpDownBoxTileCount);


        buttonOK = new Button("OK");
        buttonOK->Anchor = ANCHOR_TOP;
        buttonOK->Size = { 100, 25 };
        buttonOK->Margin.Right = 5;
        buttonOK->Margin.Top = 15;
        buttonOK->onClick += [this](auto object, auto e) -> void {
            this->Result = DialogResult::OK;
            this->Close();
        };
        mainPanel->Controls.Add(buttonOK);

        buttonCancel = new Button("Cancel");
        buttonCancel->Anchor = ANCHOR_TOP;
        buttonCancel->Size = { 100, 25 };
        buttonCancel->Margin.Top = 15;
        buttonCancel->onClick += [this](auto object, auto e) -> void {
            this->Result = DialogResult::Cancel;
            this->Close();
        };
        mainPanel->Controls.Add(buttonCancel);


        this->Controls.Add(mainPanel);
        this->AdjustSize(mainPanel);
    }
    ~Form_TileCountDialog() {
        delete labelTileCount;
        delete numericUpDownBoxTileCount;
        delete buttonOK;
        delete buttonCancel;

        delete mainPanel;
    }
};

TileCollisionEditor::TileCollisionEditor() : ResourceEditor() {
    Dock = DOCK_FILL;
    Padding = 0;

    BackColor = Color(0x21252B, 0xFF);

    tileCollisionEditorPanel = new TileCollisionEditorPanel((StageTileset*)NULL);

    int optionsLabelPos = tileCollisionEditorPanel->tilePreviewWindow->Location.Y + tileCollisionEditorPanel->tilePreviewWindow->Size.Get().H;

    labelOptions = new Label("Options:");
    labelOptions->Location = { 8, optionsLabelPos + 28 };
    tileCollisionEditorPanel->splitter->Panel2->Controls.Add(labelOptions);

    buttonSetImage = new Button();
    buttonSetImage->Location = { 8, labelOptions->Location.Y + 28 };
    buttonSetImage->Size = { 200, 25 };
    buttonSetImage->SetText("Set Image...");
    buttonSetImage->onClick += [this](auto* a, auto* d) -> void {
        if (Tileset != NULL && PromptSetImage()) {
            tileCollisionEditorPanel->buttonSetCollisionForSelectedRange->Enabled = true;
        }
    };
    tileCollisionEditorPanel->splitter->Panel2->Controls.Add(buttonSetImage);

    buttonTileCount = new Button();
    buttonTileCount->Location = { 8, buttonSetImage->Location.Y + 28 };
    buttonTileCount->Size = { 200, 25 };
    buttonTileCount->SetText("Set Tile Count...");
    buttonTileCount->onClick += [this](auto* a, auto* d) -> void {
        if (!Tileset) {
            return;
        }

        Form_TileCountDialog* dialog = new Form_TileCountDialog();
        dialog->numericUpDownBoxTileCount->Value = Tileset->TileCount;
        dialog->BackColor = BackColor;

        UI::System::Application::ShowDialog(dialog, [this, dialog](DialogResult result) -> void {
            if (result == DialogResult::OK) {
                SetTileCount((int)dialog->numericUpDownBoxTileCount->Value);
            }
        });
    };
    tileCollisionEditorPanel->splitter->Panel2->Controls.Add(buttonTileCount);

    labelTileCount = new Label();
    labelTileCount->Location = { 8, buttonTileCount->Location.Y + 28 + 8 };
    tileCollisionEditorPanel->splitter->Panel2->Controls.Add(labelTileCount);

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
    delete labelTileCount;
    delete Tileset;
}

void TileCollisionEditor::SetTileCount(int tileCount) {
    Tileset->TileCount = tileCount;

    TileSelector* tileSelector = tileCollisionEditorPanel->tileSelector;
    tileSelector->SelectRange(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
    tileSelector->Select(tileSelector->SelectedTileID);

    UpdateTileCountLabel();
}

void TileCollisionEditor::UpdateTileCountLabel() {
    char stringBuffer[24];
    snprintf(stringBuffer, sizeof stringBuffer, "Tile Count: %d", Tileset->TileCount);
    labelTileCount->SetText(stringBuffer);
}

bool TileCollisionEditor::PromptSetImage() {
    UI::SystemDialog::OpenFileData ofd;
    ofd.Title = "Open Tileset Image Files...";
    ofd.FilterPatterns.Add("*.gif");
    ofd.FilterPatterns.Add("*.png");
    ofd.Multiselect = false;

    if (UI::SystemDialog::OpenFile(&ofd) && Tileset->Load(ofd.Filenames[0])) {
        tileCollisionEditorPanel->SetTileset(Tileset);

        if (Tileset->ImageTileCount != Tileset->TileCount) {
            char stringBuffer[256];
            snprintf(stringBuffer,
                sizeof stringBuffer,
                "Change tile count?\nOld tile count: %d\nNew tile count: %d",
                Tileset->TileCount,
                Tileset->ImageTileCount);

            DialogBox* dialog = new DialogBox(250, 140, "Set Image", stringBuffer);
            dialog->BackColor = BackColor;

            UI::System::Application::ShowDialog(dialog, [this, dialog](DialogResult result) -> void {
                if (result == DialogResult::Yes) {
                    SetTileCount(Tileset->ImageTileCount);
                }
            });
        }
        else {
            UpdateTileCountLabel();
        }

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

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Studio/Impl.hpp>
#include <Studio/Structs.hpp>

#include <Studio/Editors/SceneEditor.hpp>

// Control imports
#include <UI/Controls/Control.hpp>

#include <UI/Controls/RadialKnob.hpp>
#include <UI/Controls/Button.hpp>
#include <UI/Controls/ComboBox.hpp>
#include <UI/Controls/Container.hpp>
#include <UI/Controls/Label.hpp>
#include <UI/Controls/NumericUpDownBox.hpp>
#include <UI/Controls/SplitContainer.hpp>

#include <Studio/Subcontrols/TileCollisionEditorPanel.hpp>
#include <Studio/Subcontrols/TileDrawingWidget.hpp>
#include <Studio/Subcontrols/TileSelector.hpp>

void TileCollisionEditorPanel::Init() {
    DoHScroll = false;
    DoVScroll = true;

    BackColor = Color(0x282C34, 0xFF);

    splitter = new SplitContainer();
    splitter->Dock = DOCK_FILL;
    splitter->BackColor = Color(0x000000, 0xFF);
    splitter->Panel1->BackColor = Color(0x000000, 0x00);
    splitter->Panel2->BackColor = BackColor;
    splitter->Orientation = SplitOrientation::Vertical;
    splitter->IsSplitterFixed = true;
    splitter->FixedPanel = SplitPanelFix::Panel2;
    splitter->SplitterWidth = 1;
    splitter->SplitterDistance = 2000;
    Controls.Add(splitter);

    tileSelector = new TileSelector(Tileset);
    tileSelector->ShowTileCollision = true;
    tileSelector->onSelectedTileIDChanged += std::bind(&TileCollisionEditorPanel::tileSelector_onSelectedTileIDChanged, this, std::placeholders::_1, std::placeholders::_2);
    splitter->Panel1->Controls.Add(tileSelector);

    checkBoxShowTile = new CheckBox("Show Tile:");
    checkBoxShowTile->CheckState = CheckState::Unchecked;
    checkBoxShowTile->Location = { 8, 8 };
    checkBoxShowTile->onCheckedChanged += std::bind(&TileCollisionEditorPanel::checkBoxShowTile_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
    checkBoxShowTile->CheckAlign = TEXT_ALIGN_RIGHT | TEXT_VALIGN_MIDDLE;
    checkBoxShowTile->Padding = 0;
    checkBoxShowTile->Padding.Left = 8;
    splitter->Panel2->Controls.Add(checkBoxShowTile);

    checkBoxShowCollision = new CheckBox("Show Collision:");
    checkBoxShowCollision->CheckState = CheckState::Checked;
    checkBoxShowCollision->Location = { checkBoxShowTile->Location.X + 100, checkBoxShowTile->Location.Y };
    checkBoxShowCollision->onCheckedChanged += std::bind(&TileCollisionEditorPanel::checkBoxShowTile_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
    checkBoxShowCollision->CheckAlign = TEXT_ALIGN_RIGHT | TEXT_VALIGN_MIDDLE;
    checkBoxShowCollision->Padding = 0;
    checkBoxShowCollision->Padding.Left = 8;
    splitter->Panel2->Controls.Add(checkBoxShowCollision);

    labelShowPlane = new Label("Show Plane:");
    labelShowPlane->Location = { 8, 8 + 28 };
    splitter->Panel2->Controls.Add(labelShowPlane);

    radioButtonShowA = new RadioButton("A");
    radioButtonShowA->Checked = false;
    radioButtonShowA->Location = { 8 + 80, 8 + 28 };
    radioButtonShowA->onCheckedChanged += std::bind(&TileCollisionEditorPanel::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
    radioButtonShowA->SelectionGroup = &SelectionGroup1;
    splitter->Panel2->Controls.Add(radioButtonShowA);

    radioButtonShowB = new RadioButton("B");
    radioButtonShowB->Checked = false;
    radioButtonShowB->Location = { 8 + 80 + 50, 8 + 28 };
    radioButtonShowB->onCheckedChanged += std::bind(&TileCollisionEditorPanel::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
    radioButtonShowB->SelectionGroup = &SelectionGroup1;
    splitter->Panel2->Controls.Add(radioButtonShowB);

    labelEditAngle = new Label("Edit Angle:");
    labelEditAngle->Location = { 8, 8 + 28 + 28 + 14 };
    splitter->Panel2->Controls.Add(labelEditAngle);

    radioButtonAngleTop = new RadioButton("Main"); // "Top"
    radioButtonAngleTop->Checked = false;
    radioButtonAngleTop->Location = { 8 + 65, 8 + 28 + 28 };
    radioButtonAngleTop->onCheckedChanged += std::bind(&TileCollisionEditorPanel::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
    radioButtonAngleTop->SelectionGroup = &SelectionGroup2;
    splitter->Panel2->Controls.Add(radioButtonAngleTop);

    radioButtonAngleBottom = new RadioButton("Unused"); // "Bottom"
    radioButtonAngleBottom->Checked = false;
    radioButtonAngleBottom->Location = { 8 + 65 + 80, 8 + 28 + 28 };
    radioButtonAngleBottom->onCheckedChanged += std::bind(&TileCollisionEditorPanel::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
    radioButtonAngleBottom->SelectionGroup = &SelectionGroup2;
    splitter->Panel2->Controls.Add(radioButtonAngleBottom);

    radioButtonAngleLeft = new RadioButton("Unused"); // "Left"
    radioButtonAngleLeft->Checked = false;
    radioButtonAngleLeft->Location = { 8 + 65, 8 + 28 + 28 + 28 };
    radioButtonAngleLeft->onCheckedChanged += std::bind(&TileCollisionEditorPanel::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
    radioButtonAngleLeft->SelectionGroup = &SelectionGroup2;
    splitter->Panel2->Controls.Add(radioButtonAngleLeft);

    radioButtonAngleRight = new RadioButton("Unused"); // "Right"
    radioButtonAngleRight->Checked = false;
    radioButtonAngleRight->Location = { 8 + 65 + 80, 8 + 28 + 28 + 28 };
    radioButtonAngleRight->onCheckedChanged += std::bind(&TileCollisionEditorPanel::radioButtonShow_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
    radioButtonAngleRight->SelectionGroup = &SelectionGroup2;
    splitter->Panel2->Controls.Add(radioButtonAngleRight);

    radioButtonAngleBottom->Enabled = false;
    radioButtonAngleLeft->Enabled = false;
    radioButtonAngleRight->Enabled = false;

    radialKnobAngle = new RadialKnob();
    radialKnobAngle->Location = { radioButtonAngleBottom->Location.X + 80, radioButtonAngleBottom->Location.Y };
    radialKnobAngle->Size = { 48, 48 };
    radialKnobAngle->MaxAngle = 256.0;
    radialKnobAngle->Bias = 64.0;
    radialKnobAngle->onDialTurn += std::bind(&TileCollisionEditorPanel::radialKnobAngle_onDialTurn, this, std::placeholders::_1, std::placeholders::_2);
    radialKnobAngle->onValueChanged += std::bind(&TileCollisionEditorPanel::radialKnobAngle_onValueChanged, this, std::placeholders::_1, std::placeholders::_2);
    splitter->Panel2->Controls.Add(radialKnobAngle);

    labelRawAngleValue = new Label("Angle:  0 degrees (0x00)");
    labelRawAngleValue->ForeColor = Color(0xFFFFFF, 0x7F);
    labelRawAngleValue->Location = { 8 + 65, 8 + 28 + 28 + 28 + 28 };
    splitter->Panel2->Controls.Add(labelRawAngleValue);

    labelAutoCollision = new Label("AutoCollision (Loose Fit)");
    labelAutoCollision->Location = { 8, labelRawAngleValue->Location.Y + 28 };
    splitter->Panel2->Controls.Add(labelAutoCollision);

    buttonSetCollisionForSelectedRange = new Button("Set Collision For Selected Range");
    buttonSetCollisionForSelectedRange->Location = { 8, labelAutoCollision->Location.Y + 25 };
    buttonSetCollisionForSelectedRange->Margin.Top = 4;
    buttonSetCollisionForSelectedRange->Size = { 200, 25 };
    buttonSetCollisionForSelectedRange->onMouseClick += std::bind(&TileCollisionEditorPanel::buttonSetCollisionForSelectedRange_onMouseClick, this, std::placeholders::_1, std::placeholders::_2);
    buttonSetCollisionForSelectedRange->Enabled = false;
    splitter->Panel2->Controls.Add(buttonSetCollisionForSelectedRange);

    labelAutoCollisionNote = new Label("*sets the values based on the tile image.");
    labelAutoCollisionNote->ForeColor = Color(0xFFFFFF, 0x7F);
    labelAutoCollisionNote->Location = { 8, buttonSetCollisionForSelectedRange->Location.Y + 29 };
    splitter->Panel2->Controls.Add(labelAutoCollisionNote);

    labelManualSettings = new Label("Manual Settings");
    labelManualSettings->Location = { 8, labelAutoCollisionNote->Location.Y + 28 };
    splitter->Panel2->Controls.Add(labelManualSettings);

    //*
    tilePreviewWindow = new TileDrawingWidget(this);
    tilePreviewWindow->Dock = DOCK_NONE;
    tilePreviewWindow->Location = { 8, labelManualSettings->Location.Y + 28 };
    tilePreviewWindow->Size = { 176, 176 };
    splitter->Panel2->Controls.Add(tilePreviewWindow);
    //*/

    labelOrientation = new Label("Orientation:");
    labelOrientation->Location = { tilePreviewWindow->Location.X + tilePreviewWindow->Size.Get().W + 8, tilePreviewWindow->Location.Y };
    splitter->Panel2->Controls.Add(labelOrientation);

    comboboxOrientation = new ComboBox();
    comboboxOrientation->Location = { labelOrientation->Location.X, labelOrientation->Location.Y + 20 };
    comboboxOrientation->Size = { 100, 25 };
    comboboxOrientation->Items.Add("FLOOR");
    comboboxOrientation->Items.Add("CEILING");
    comboboxOrientation->Select(0);
    comboboxOrientation->onSelectedIndexChanged += std::bind(&TileCollisionEditorPanel::comboboxOrientation_onSelectedIndexChanged, this, std::placeholders::_1, std::placeholders::_2);
    splitter->Panel2->Controls.Add(comboboxOrientation);

    labelBehaviorFlag = new Label("Behavior Flag:");
    labelBehaviorFlag->Location = { comboboxOrientation->Location.X, comboboxOrientation->Location.Y + 28 };
    splitter->Panel2->Controls.Add(labelBehaviorFlag);

    numericUpDownBoxBehaviorFlag = new NumericUpDown();
    numericUpDownBoxBehaviorFlag->Hexadecimal = true;
    numericUpDownBoxBehaviorFlag->Minimum = 0.0f;
    numericUpDownBoxBehaviorFlag->Maximum = 255.0f;
    numericUpDownBoxBehaviorFlag->Location = { labelBehaviorFlag->Location.X, labelBehaviorFlag->Location.Y + 20 };
    numericUpDownBoxBehaviorFlag->Size = { 100, 25 };
    numericUpDownBoxBehaviorFlag->onValueChanged += std::bind(&TileCollisionEditorPanel::numericUpDownBoxBehaviorFlag_onValueChanged, this, std::placeholders::_1, std::placeholders::_2);
    splitter->Panel2->Controls.Add(numericUpDownBoxBehaviorFlag);

    checkBoxShowGrid = new CheckBox("Show Grid:");
    checkBoxShowGrid->CheckState = CheckState::Unchecked;
    checkBoxShowGrid->Location = { numericUpDownBoxBehaviorFlag->Location.X, numericUpDownBoxBehaviorFlag->Location.Y + 28 };
    // checkBoxShowGrid->onCheckedChanged += std::bind(&TileCollisionEditorPanel::checkBoxShowTile_onCheckedChanged, this, std::placeholders::_1, std::placeholders::_2);
    checkBoxShowGrid->CheckAlign = TEXT_ALIGN_RIGHT | TEXT_VALIGN_MIDDLE;
    checkBoxShowGrid->Padding = 0;
    checkBoxShowGrid->Padding.Left = 8;
    splitter->Panel2->Controls.Add(checkBoxShowGrid);

    ///
    splitter->Panel2MinSize = tilePreviewWindow->Location.Y + tilePreviewWindow->Size.Get().H + 8;

    ///
    radioButtonShowA->Check();
    radioButtonAngleTop->Check();
}

TileCollisionEditorPanel::TileCollisionEditorPanel(SceneEditor* editor) : Panel() {
    Editor = editor;
    if (Editor->LinkedStage) {
        Tileset = &Editor->LinkedStage->Tileset;
    }

    Init();
}
TileCollisionEditorPanel::TileCollisionEditorPanel(StageTileset* tileset) : Panel() {
    Tileset = tileset;

    Init();
}
TileCollisionEditorPanel::~TileCollisionEditorPanel() {
    delete tileSelector;
    delete splitter;
    delete tilePreviewWindow;
    delete checkBoxShowTile;
    delete checkBoxShowCollision;
    delete labelShowPlane;
    delete radioButtonShowA;
    delete radioButtonShowB;
    delete labelEditAngle;
    delete radioButtonAngleTop;
    delete radioButtonAngleBottom;
    delete radioButtonAngleLeft;
    delete radioButtonAngleRight;
	delete radialKnobAngle;
    delete labelRawAngleValue;
    delete labelAutoCollision;
    delete buttonSetCollisionForSelectedRange;
    delete labelAutoCollisionNote;
    delete labelManualSettings;
    delete labelOrientation;
    delete comboboxOrientation;
    delete labelBehaviorFlag;
    delete numericUpDownBoxBehaviorFlag;
    delete checkBoxShowGrid;
}

void TileCollisionEditorPanel::SetTileset(StageTileset* tileset) {
    Tileset = tileset;

    tileSelector->SetTileset(Tileset);
    tilePreviewWindow->Tileset = Tileset;
}

void TileCollisionEditorPanel::UpdateAngleLabel(int newAngle) {
    char textBuffer[256];
    int newAngleDeg = (int)(newAngle * 360.0 / radialKnobAngle->MaxAngle);
    snprintf(textBuffer, 255, "Angle:  %d degrees (0x%02X)", newAngleDeg, newAngle);
    labelRawAngleValue->SetText(textBuffer);
}
void TileCollisionEditorPanel::UpdateTileInfoUI() {
	if (tileSelector->SelectedTileID < 0)
		return;
    if (!Tileset)
        return;

	int p = tilePreviewWindow->GetPlane();
	int t = tileSelector->SelectedTileID;
    EditableTileConfig* tileData = &Tileset->TileCfg[p][t];

	int newAngle = -1;
    switch (GetAngleEditSide()) {
    case 0: newAngle = tileData->AngleTop; break;
    case 1: newAngle = tileData->AngleLeft; break;
    case 2: newAngle = tileData->AngleRight; break;
    case 3: newAngle = tileData->AngleBottom; break;
    }

    comboboxOrientation->CanRaiseEvents = false;
    comboboxOrientation->Select(tileData->Orientation);
    comboboxOrientation->CanRaiseEvents = true;

    radialKnobAngle->CanRaiseEvents = false;
	radialKnobAngle->Angle = newAngle;
    radialKnobAngle->CanRaiseEvents = true;

    numericUpDownBoxBehaviorFlag->CanRaiseEvents = false;
    numericUpDownBoxBehaviorFlag->Value = tileData->Behavior;
    numericUpDownBoxBehaviorFlag->CanRaiseEvents = true;

    UpdateAngleLabel(newAngle);
}

int TileCollisionEditorPanel::GetAngleEditSide() {
    if (radioButtonAngleTop->Checked)
        return 0;
    if (radioButtonAngleLeft->Checked)
        return 1;
    if (radioButtonAngleRight->Checked)
        return 2;
    if (radioButtonAngleBottom->Checked)
        return 3;
    return -1;
}

void TileCollisionEditorPanel::DoAutoTile(int i) {
    if (!Tileset || Tileset->TileImagePixelData == NULL)
        return;

    int p = tilePreviewWindow->GetPlane();
    EditableTileConfig* tileData = &Tileset->TileCfg[p][i];

    int tileSize = 16;
    int columnCount = 64;
    int tileSheetWidth = tileSize * columnCount;

    int tileImageX = (i % columnCount) * tileSize;
    int tileImageY = (i / columnCount) * tileSize;
    bool isCeiling = false;
    bool hasCollision = true;
    Uint32* pxData = Tileset->TileImagePixelData;

    // Determine whether is ceiling or not.
    int topCount = 0;
    int bottomCount = 0;
    for (int p = tileImageX; p < tileImageX + tileSize; p++) {
        int px;

        px = (p + (tileImageY) * tileSheetWidth);
        if ((pxData[px] & 0xFF000000) > 0)
            topCount++;

        px = (p + (tileImageY + tileSize - 1) * tileSheetWidth);
        if ((pxData[px] & 0xFF000000) > 0)
            bottomCount++;
    }
    if (topCount > bottomCount)
        isCeiling = true;

    tileData->Orientation = isCeiling;
    tileData->AngleTop = 0x00;

    int firstValue = -1;
    int lastValue = -1;
    int slopeRun = 0;

    // Get space lengths.
    if (isCeiling) {
        // If ceiling, start checking from bottom and vice-versa
        int fx = 0;
        for (int p = tileImageX; p < tileImageX + tileSize; p++) {
            int value = 0xFF;
            for (int c = 0, pY = (p + (tileImageY + tileSize - 1) * tileSheetWidth);
                c < tileSize;
                c++, pY -= tileSheetWidth) {
                if ((pxData[pY] & 0xFF000000) > 0) {
                    value = tileSize - 1 - c;
                    break;
                }
            }

            tileData->Collision[fx] = value;

            if (value != 0xFF && value != tileSize - 1) {
                if (firstValue == -1)
                    firstValue = value + 1;

                lastValue = value + 1;
                slopeRun++;
            }
            fx++;
        }
    }
    else {
        int fx = 0;
        for (int p = tileImageX; p < tileImageX + tileSize; p++) {
            int value = 0xFF;
            for (int c = 0, pY = (p + (tileImageY) * tileSheetWidth);
                c < tileSize;
                c++, pY += tileSheetWidth) {
                if ((pxData[pY] & 0xFF000000) > 0) {
                    value = c;
                    break;
                }
            }

            tileData->Collision[fx] = value;

            if (value != 0xFF && value != 0) {
                if (firstValue == -1)
                    firstValue = value - 1;

                lastValue = value - 1;
                slopeRun++;
            }
            fx++;
        }
    }

    if (firstValue > -1 && lastValue > -1) {
        int slopeRise = lastValue - firstValue;
        if (slopeRise < 0) slopeRise--; else slopeRise++;

        int angle = Math::ATan(slopeRun, slopeRise);
        // printf("Tile %d: F %d L %d Run %d > 0x%02X -> 0x%02X\n", i, firstValue, lastValue, slopeRun, angle, (angle + 2) & 0xFC);
        tileData->AngleTop = (angle + 2) & 0xFC;
    }

    if (Tileset) {
        Tileset->UpdateTileCollisionTexture(p, i);
    }
    if (Editor) {
        Editor->tilePlacementField->UpdateRenderTarget = true;
    }
}

void TileCollisionEditorPanel::buttonSetCollisionForSelectedRange_onMouseClick(void* sender, MouseEventArgs* e) {
    if (tileSelector->SelectedTileID < 0)
        return;

    int p = tilePreviewWindow->GetPlane();
    int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
    int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

    for (int t = tS; t <= tE; t++) {
        DoAutoTile(t);
    }

    UpdateTileInfoUI();
}
void TileCollisionEditorPanel::numericUpDownBoxBehaviorFlag_onValueChanged(void* sender, EventArgs* e) {
    if (tileSelector->SelectedTileID < 0)
        return;
    if (!Tileset)
        return;

    int p = tilePreviewWindow->GetPlane();
    int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
    int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

    for (int t = tS; t <= tE; t++) {
        EditableTileConfig* tileData = &Tileset->TileCfg[p][t];
        tileData->Behavior = (int)numericUpDownBoxBehaviorFlag->Value;
    }

    UpdateTileInfoUI();
}
void TileCollisionEditorPanel::comboboxOrientation_onSelectedIndexChanged(void* sender, EventArgs* args) {
    if (tileSelector->SelectedTileID < 0)
        return;
    if (!Tileset)
        return;
    if (comboboxOrientation->SelectedIndex < 0)
        return;

    int p = tilePreviewWindow->GetPlane();
    int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
    int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

    for (int t = tS; t <= tE; t++) {
        EditableTileConfig* tileData = &Tileset->TileCfg[p][t];
        tileData->Orientation = comboboxOrientation->SelectedIndex;
    }

    UpdateTileInfoUI();
}
void TileCollisionEditorPanel::tileSelector_onSelectedTileIDChanged(void* sender, EventArgs* args) {
    UpdateTileInfoUI();
}
void TileCollisionEditorPanel::checkBoxShowTile_onCheckedChanged(void* sender, EventArgs* args) {
    tileSelector->ShowTileGraphics = checkBoxShowTile->GetChecked();
    tileSelector->ShowTileCollision = checkBoxShowCollision->GetChecked();
}
void TileCollisionEditorPanel::radioButtonShow_onCheckedChanged(void* sender, EventArgs* args) {
    tileSelector->TileCollisionPlane = tilePreviewWindow->GetPlane();

	UpdateTileInfoUI();
}
void TileCollisionEditorPanel::radialKnobAngle_onValueChanged(void* sender, DialValueChangedArgs* args) {
    if (tileSelector->SelectedTileID < 0)
        return;
    if (!Tileset)
        return;

    int newAngle = (int)args->Value;

    int p = tilePreviewWindow->GetPlane();
    int tS = M_MIN(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);
    int tE = M_MAX(tileSelector->SelectedTileRange_Start, tileSelector->SelectedTileRange_End);

    for (int t = tS; t <= tE; t++) {
        EditableTileConfig* tileData = &Tileset->TileCfg[p][t];

        switch (GetAngleEditSide()) {
        case 0: tileData->AngleTop = newAngle; break;
        case 1: tileData->AngleLeft = newAngle; break;
        case 2: tileData->AngleRight = newAngle; break;
        case 3: tileData->AngleBottom = newAngle; break;
        }
    }

	UpdateTileInfoUI();
}
void TileCollisionEditorPanel::radialKnobAngle_onDialTurn(void* sender, DialTurnedArgs* args) {
    UpdateAngleLabel(args->Value);
}

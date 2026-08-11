#pragma once

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Studio/Impl.hpp>
#include <Studio/Structs.hpp>

#include <UI/Graphics/Renderer.hpp>

// Control imports
#include <UI/Controls/Control.hpp>

#include <UI/Controls/RadialKnob.hpp>
#include <UI/Controls/Button.hpp>
#include <UI/Controls/ComboBox.hpp>
#include <UI/Controls/Container.hpp>
#include <UI/Controls/Label.hpp>
#include <UI/Controls/NumericUpDownBox.hpp>
#include <UI/Controls/SplitContainer.hpp>

#include <Studio/Subcontrols/TileDrawingWidget.hpp>
#include <Studio/Subcontrols/TileSelector.hpp>

struct SceneEditor;

struct TileCollisionEditorPanel : Panel {
    SceneEditor* Editor = NULL;
    StageTileset* Tileset = NULL;

    TileSelector* tileSelector = NULL;
    SplitContainer* splitter = NULL;
    TileDrawingWidget* tilePreviewWindow = NULL;
    CheckBox* checkBoxShowTile = NULL;
    CheckBox* checkBoxShowCollision = NULL;
    Label* labelShowPlane = NULL;
    RadioButton* radioButtonShowA = NULL;
    RadioButton* radioButtonShowB = NULL;
    Label* labelEditAngle = NULL;
    RadioButton* radioButtonAngleTop = NULL;
    RadioButton* radioButtonAngleBottom = NULL;
    RadioButton* radioButtonAngleLeft = NULL;
    RadioButton* radioButtonAngleRight = NULL;
	RadialKnob* radialKnobAngle = NULL;
    Label* labelRawAngleValue = NULL;
    Label* labelAutoCollision = NULL;
    Button* buttonSetCollisionForSelectedRange = NULL;
    Label* labelAutoCollisionNote = NULL;
    Label* labelManualSettings = NULL;
    Label* labelOrientation = NULL;
    ComboBox* comboboxOrientation = NULL;
    Label* labelBehaviorFlag = NULL;
    NumericUpDown* numericUpDownBoxBehaviorFlag = NULL;
    CheckBox* checkBoxShowGrid = NULL;

    // These are not allocated, do not 'delete'!
    RadioButton* SelectionGroup1 = NULL;
    RadioButton* SelectionGroup2 = NULL;

    TileCollisionEditorPanel(SceneEditor* editor);
    TileCollisionEditorPanel(StageTileset* tileset);
    ~TileCollisionEditorPanel();

    void SetTileset(StageTileset* tileset);

    void UpdateAngleLabel(int newAngle);
	void UpdateTileInfoUI();

    int GetAngleEditSide();

    void DoAutoTile(int i);

    void buttonSetCollisionForSelectedRange_onMouseClick(void* sender, MouseEventArgs* e);
    void numericUpDownBoxBehaviorFlag_onValueChanged(void* sender, EventArgs* e);
    void comboboxOrientation_onSelectedIndexChanged(void* sender, EventArgs* args);
    void tileSelector_onSelectedTileIDChanged(void* sender, EventArgs* args);
    void checkBoxShowTile_onCheckedChanged(void* sender, EventArgs* args);
    void radioButtonShow_onCheckedChanged(void* sender, EventArgs* args);
	void radialKnobAngle_onValueChanged(void* sender, DialValueChangedArgs* args);
    void radialKnobAngle_onDialTurn(void* sender, DialTurnedArgs* args);

private:
    void Init();
};

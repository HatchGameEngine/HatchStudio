#include "DialogBox.hpp"
#include <UI/Graphics/Renderer.hpp>

#include <Hatch/Strings.h>

DialogBox::DialogBox(int w, int h, const char* title, const char* text) : Form(w, h, title, 0) {
    mainPanel = new FlowLayoutPanel();
    mainPanel->BackColor = Color(0x000000, 0x00);
    mainPanel->Dock = DOCK_FILL;
    mainPanel->FlowDirection = FlowDirection::LEFT_TO_RIGHT;
    mainPanel->Padding = 10;
    mainPanel->WrapContents = true;

    labelText = new Label(text);
    labelText->Size = { 100, 25 };
    labelText->WordWrap = true;
    labelText->MaxWrapWidth = 200;
    labelText->Anchor = ANCHOR_TOP;
    labelText->Margin.Top = 5;
    labelText->Margin.Right = 10;
    labelText->LineBreak = true;
    labelText->AlignFlags = TEXT_ALIGN_CENTER;
    mainPanel->Controls.Add(labelText);


    buttonYes = new Button("Yes");
    buttonYes->Anchor = ANCHOR_TOP;
    buttonYes->Size = { 100, 25 };
    buttonYes->Margin.Right = 5;
    buttonYes->Margin.Top = 15;
    buttonYes->onClick += [this](auto object, auto e) -> void {
        this->Result = DialogResult::Yes;
        this->Close();
    };
    mainPanel->Controls.Add(buttonYes);

    buttonNo = new Button("No");
    buttonNo->Anchor = ANCHOR_TOP;
    buttonNo->Size = { 100, 25 };
    buttonNo->Margin.Top = 15;
    buttonNo->onClick += [this](auto object, auto e) -> void {
        this->Result = DialogResult::No;
        this->Close();
    };
    mainPanel->Controls.Add(buttonNo);


    this->Controls.Add(mainPanel);
    this->AdjustSize(mainPanel);

    this->CloseCallback = NULL;

    CanFocus = false;
}

DialogBox::~DialogBox() {
    delete labelText;
    delete buttonYes;
    delete buttonNo;

    delete mainPanel;
}

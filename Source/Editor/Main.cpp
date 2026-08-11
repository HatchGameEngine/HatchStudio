#include <SDL2/SDL.h>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/IO/Stream.h>
#include <Hatch/IO/FileStream.h>

#define STB_IMAGE_IMPLEMENTATION
#include <Libraries/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Libraries/stb_image_write.h>

#include <Hatch/Diagnostics.h>
#include <Hatch/GameLinker.h>
#include <Hatch/Graphics.h>
#include <Hatch/Math.h>
#include <Hatch/Resources.h>
#include <Hatch/Scene.h>
#include <Hatch/Services.h>
#include <Hatch/Strings.h>

#include <chrono>

#include <Studio/Impl.hpp>
#include <Studio/Structs.hpp>

#include <UI/Graphics/Renderer.hpp>

// Control imports
#include <UI/Controls/Control.hpp>

#include <UI/Controls/Button.hpp>
#include <UI/Controls/ComboBox.hpp>
#include <UI/Controls/Form.hpp>
#include <UI/Controls/Label.hpp>
#include <UI/Controls/MenuBar.hpp>
#include <UI/Controls/Textbox.hpp>
#include <UI/Controls/ToolStrip.hpp>
#include <UI/Controls/ToolTip.hpp>
#include <UI/Filesystem/Paths.hpp>
#include <UI/System/Application.hpp>
#include <UI/System/Menu.hpp>
#include <UI/System/SystemDialog.hpp>

#include <Studio/Editors/ResourceEditor.hpp>
#include <Studio/Editors/SceneEditor.hpp>
#include <Studio/Editors/TileCollisionEditor.hpp>
#include <Studio/Project.hpp>

// using UI::Graphics;
using Studio::ResourceEditor;

// The main namespaces
namespace Hatch { }
namespace UI { }
namespace Studio { }

#define TOLOWER(ch) SDL_tolower(ch)

char* stristr(const char* str1, const char* str2) {
    const char* p1 = str1;
    const char* p2 = str2;
    const char* r = *p2 == 0 ? str1 : 0;

    while (*p1 != 0 && *p2 != 0) {
        if (TOLOWER((unsigned char)*p1) == TOLOWER((unsigned char)*p2)) {
            if (r == 0) {
                r = p1;
            }

            p2++;
        }
        else {
            p2 = str2;
            if (r != 0) {
                p1 = r + 1;
            }

            if (TOLOWER((unsigned char)*p1) == TOLOWER((unsigned char)*p2)) {
                r = p1;
                p2++;
            }
            else {
                r = 0;
            }
        }

        p1++;
    }
    return *p2 == 0 ? (char*)r : 0;
}

// .HSPR - Sprite File
// .HMSH - 3D Mesh File
// .HPAL - Palette File
// .HSTG - Stage File
// .HSCN - Scene File
// .HCOL - Tile Collision File
// .HTIL - Tile Image File (Un/compressed)
// .HATCH - Resource Pack File
// .HPROJ - Project Info File

struct Form_NewProjectWizard : Form {
    Label* labelProjectName;
    TextboxBase* textBoxProjectName;
    Label* labelEngineVersion;
    ComboBox* comboBoxEngineVersion;
    Label* labelShortName;
    TextboxBase* textBoxShortName;
    Button* buttonOK;
    Button* buttonCancel;

    FlowLayoutPanel* mainPanel;

    Form_NewProjectWizard() : Form(250, 140, "") {
        mainPanel = new FlowLayoutPanel();
        mainPanel->BackColor = Color(0x000000, 0x00);
        mainPanel->Dock = DOCK_FILL;
        mainPanel->FlowDirection = FlowDirection::LEFT_TO_RIGHT;
        mainPanel->Padding = 10;
        mainPanel->WrapContents = false;

        // Project Name
        labelProjectName = new Label("Project Name:");
        labelProjectName->Anchor = ANCHOR_TOP;
        labelProjectName->Margin.Top = 5;
        labelProjectName->Margin.Right = 10;
        mainPanel->Controls.Add(labelProjectName);

        textBoxProjectName = new TextboxBase("");
        textBoxProjectName->Size = { 180, 25 };
        textBoxProjectName->LineBreak = true;
        mainPanel->Controls.Add(textBoxProjectName);

        // Engine Version
        labelEngineVersion = new Label("Engine Version:");
        labelEngineVersion->Anchor = ANCHOR_TOP;
        labelEngineVersion->Margin.Top = 5;
        labelEngineVersion->Margin.Right = 10;
        mainPanel->Controls.Add(labelEngineVersion);

        comboBoxEngineVersion = new ComboBox();
        comboBoxEngineVersion->Anchor = ANCHOR_TOP;
        comboBoxEngineVersion->Size = { 180, 25 };
        comboBoxEngineVersion->LineBreak = true;
        comboBoxEngineVersion->Margin.Bottom = 5;
        comboBoxEngineVersion->Items.Add("Hatch Game Engine");
        comboBoxEngineVersion->Items.Add("HatchLite");
        comboBoxEngineVersion->Select(0);
        mainPanel->Controls.Add(comboBoxEngineVersion);

        // Short Name
        labelShortName = new Label("Short Name: (for mobile)");
        labelShortName->Anchor = ANCHOR_TOP;
        labelShortName->Margin.Top = 5;
        labelShortName->Margin.Right = 10;
        mainPanel->Controls.Add(labelShortName);

        textBoxShortName = new TextboxBase("Hatch");
        textBoxShortName->Size = { 180, 25 };
        textBoxShortName->LineBreak = true;
        mainPanel->Controls.Add(textBoxShortName);


        buttonOK = new Button("OK");
        buttonOK->Anchor = ANCHOR_TOP;
        buttonOK->Size = { 100, 25 };
        buttonOK->Margin.Right = 5;
        buttonOK->Margin.Top = 15;
        buttonOK->onClick += [this](auto object, auto e) -> void {
            this->Result = DialogResult::OK;
            if (this->textBoxProjectName->Text.Length > 0)
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
        this->UpdateLayout(); // This should theoretically happen during Controls.Add

        this->Size = { 500, buttonCancel->Location.Y + buttonCancel->Size.Get().H + mainPanel->Padding.Bottom };
    }
    ~Form_NewProjectWizard() {
        delete labelProjectName;
        delete textBoxProjectName;
        delete labelEngineVersion;
        delete comboBoxEngineVersion;
        delete labelShortName;
        delete textBoxShortName;
        delete buttonOK;
        delete buttonCancel;

        delete mainPanel;
    }
};

struct RecentProject {
    char* Name = NULL;
    char* Filepath = NULL;
};
struct HatchProject {
    enum HatchVersion : int {
        HatchGameEngine,
        HatchLite,
    };

    const char* MAGIC_HPROJ = "HPROJ";

    char* ProjectName;
    int EngineVersion;

    List<char*> LastOpenFiles;

    // Mobile settings
    char* ShortName;

    HatchProject() {
        ProjectName = NULL;
        EngineVersion = HatchVersion::HatchGameEngine;

        ShortName = NULL;
    }
    ~HatchProject() {
        free(ProjectName);
        free(ShortName);
    }

    void Read(Stream* stream) {
        char magic[5];
        char fileTypeVersion[3];

        // Read Magic
        stream->ReadBytes(magic, 5);
        if (memcmp(MAGIC_HPROJ, magic, 5) != 0)
            return;

        // Read File Type version
        stream->ReadBytes(fileTypeVersion, 3);

        // Others
        ProjectName = stream->ReadHeaderedString();
        EngineVersion = stream->ReadByte();

        ShortName = stream->ReadHeaderedString();

        // v0.0.2: Added last open files support
        if (fileTypeVersion[2] < 2) return;

        int count = stream->ReadInt32();
        for (int i = 0; i < count; i++) {
            LastOpenFiles.Add(stream->ReadHeaderedString());
        }
    }
    void Write(Stream* stream) {
        char fileTypeVersion[3] = { 0, 0, 2 };

        // Write Magic
        stream->WriteBytes((void*)MAGIC_HPROJ, 5);

        // Write File Type version
        stream->WriteBytes(fileTypeVersion, 3);

        // Others
        stream->WriteHeaderedString(ProjectName);
        stream->WriteByte(EngineVersion);

        stream->WriteHeaderedString(ShortName);

        stream->WriteInt32(LastOpenFiles.Count());
        for (int i = 0; i < LastOpenFiles.Count(); i++) {
            stream->WriteHeaderedString(LastOpenFiles[i]);
        }
    }
};
struct HatchStudioSettings {
    int WindowX = 0;
    int WindowY = 0;
    int WindowWidth = 1280;
    int WindowHeight = 720;
    bool Maximized = false;
    bool RunFromStartScene = true;
    bool ReopenLastProject = true;
    List<RecentProject> RecentProjects;

    ~HatchStudioSettings() {
        return;
    }

    void Read(Stream* stream) {
        WindowX = stream->ReadInt32();
        WindowY = stream->ReadInt32();
        WindowWidth = stream->ReadInt32();
        WindowHeight = stream->ReadInt32();
        Maximized = stream->ReadInt32();
        RunFromStartScene = stream->ReadInt32();

        int count = stream->ReadInt32();
        for (int i = 0; i < count; i++) {
            RecentProjects.Add(RecentProject { stream->ReadHeaderedString(), stream->ReadHeaderedString() });
        }
    }
    void Write(Stream* stream) {
        stream->WriteInt32(WindowX);
        stream->WriteInt32(WindowY);
        stream->WriteInt32(WindowWidth);
        stream->WriteInt32(WindowHeight);
        stream->WriteInt32(Maximized);
        stream->WriteInt32(RunFromStartScene);

        stream->WriteInt32(RecentProjects.Count());
        for (int i = 0; i < RecentProjects.Count(); i++) {
            stream->WriteHeaderedString(RecentProjects[i].Name);
            stream->WriteHeaderedString(RecentProjects[i].Filepath);
        }
    }
};

struct HatchStudioForm : Form {
    static HatchStudioForm* MainForm;

    UI::Menu* mainMenu = NULL;
    UI::Menu* menuFile = NULL;
    UI::Menu* menuRecentProjects = NULL;
    UI::Menu* menuNewResource = NULL;
    UI::Menu* menuProject = NULL;
    UI::Menu* menuRunFromScene = NULL;
    UI::Menu* menuHelp = NULL;
    UI::Menu* menuApple = NULL;
    UI::Menu* menuWindow = NULL;
    int menuIndex_SaveFile = -1;
    int menuIndex_SaveFileAs = -1;
    int menuIndex_SaveAllFile = -1;
    int menuIndex_CloseFile = -1;
    int menuIndex_CloseAllFiles = -1;
    int menuIndex_RunFromStartScene = -1;
    int menuIndex_RunFromCurrentScene = -1;

    #pragma region Shortcut definitions
    #if !defined(_MACOS) // #if defined(_WINDOWS)
    const int SHORTCUT_NEW_PROJECT = UI::Menu::SM_CONTROL | UI::Menu::SM_ALT | 'n';
    const int SHORTCUT_OPEN_PROJECT = UI::Menu::SM_CONTROL | UI::Menu::SM_ALT | 'o';
    const int SHORTCUT_CLOSE_PROJECT = UI::Menu::SM_CONTROL | UI::Menu::SM_ALT | 'w';

    const int SHORTCUT_NEW_FILE = UI::Menu::SM_CONTROL | 'n';
    const int SHORTCUT_OPEN_FILE = UI::Menu::SM_CONTROL | 'o';
    const int SHORTCUT_CLOSE_FILE = UI::Menu::SM_CONTROL | 'w';

    const int SHORTCUT_SAVE_FILE = UI::Menu::SM_CONTROL | 's';
    const int SHORTCUT_SAVE_FILE_AS = UI::Menu::SM_CONTROL | UI::Menu::SM_SHIFT | 's';
    const int SHORTCUT_SAVE_ALL = UI::Menu::SM_CONTROL | UI::Menu::SM_ALT | 's';

    const int SHORTCUT_CLOSE_ALL = UI::Menu::SM_NONE; // SM_CONTROL | SM_SHIFT | SM_ALT | 'w';

    const int SHORTCUT_BUILD_GAME_LOGIC = UI::Menu::SM_CONTROL | 'b'; // CMD+B on Mac, CTRL+B on Windows
    const int SHORTCUT_RUN_LOCALLY = UI::Menu::SM_CONTROL | 'r'; // CMD+R on Mac, F5 on Windows
    const int SHORTCUT_RUN_REMOTELY = UI::Menu::SM_NONE; // CMD+R on Mac, F5 on Windows
    #endif
    #if defined(_MACOS)
    const int SHORTCUT_NEW_PROJECT = UI::Menu::SM_COMMAND | UI::Menu::SM_ALT | 'n';
    const int SHORTCUT_OPEN_PROJECT = UI::Menu::SM_COMMAND | UI::Menu::SM_ALT | 'o';
    const int SHORTCUT_CLOSE_PROJECT = UI::Menu::SM_COMMAND | UI::Menu::SM_ALT | 'w';

    const int SHORTCUT_NEW_FILE = UI::Menu::SM_COMMAND | 'n';
    const int SHORTCUT_OPEN_FILE = UI::Menu::SM_COMMAND | 'o';
    const int SHORTCUT_CLOSE_FILE = UI::Menu::SM_COMMAND | 'w';

    const int SHORTCUT_SAVE_FILE = UI::Menu::SM_COMMAND | 's';
    const int SHORTCUT_SAVE_FILE_AS = UI::Menu::SM_COMMAND | UI::Menu::SM_SHIFT | 's';
    const int SHORTCUT_SAVE_ALL = UI::Menu::SM_COMMAND | UI::Menu::SM_ALT | 's';

    const int SHORTCUT_CLOSE_ALL = UI::Menu::SM_NONE; // SM_COMMAND | SM_SHIFT | SM_ALT | 'w';

    const int SHORTCUT_BUILD_GAME_LOGIC = UI::Menu::SM_COMMAND | 'b'; // CMD+B on Mac, CTRL+B on Windows
    const int SHORTCUT_RUN_LOCALLY = UI::Menu::SM_COMMAND | 'r'; // CMD+R on Mac, F5 on Windows
    const int SHORTCUT_RUN_REMOTELY = UI::Menu::SM_NONE; // CMD+R on Mac, F5 on Windows
    #endif
    #pragma endregion
    #pragma region "File" menu actions
    static void Action_NewProject() {
        if (!MainForm->CloseCurrentProject())
            return;

        MainForm->NewProject();
    }
    static void Action_OpenProject() {
        UI::SystemDialog::OpenFileData ofd;
        ofd.Title = "Open Hatch Project...";
        // ofd.InitialDirectory = ProjectDirectory;
        ofd.FilterPatterns.Add("*.HPROJ");
        ofd.Multiselect = false;

        if (UI::SystemDialog::OpenFile(&ofd)) {
            if (!MainForm->CloseCurrentProject())
                return;

            MainForm->OpenProject(ofd.Filenames[0]);
        }
    }
    static void Action_SaveProject() {
        MainForm->SaveProject(MainForm->CurrentProjectFilePath);
    }
    static void Action_CloseProject() {
        MainForm->CloseCurrentProject();
    }
    static void Action_ClearRecentProjects() {
        MainForm->ClearRecentProjects();
    }
    static void Action_NewSceneResource() {
        MainForm->NewSceneFile();
    }
    static void Action_NewTileCollisionResource() {
        MainForm->NewTileCollisionFile();
    }
    static void Action_OpenResource() {
        UI::SystemDialog::OpenFileData ofd;
        ofd.Title = "Open a Resource from the current project...";
        // ofd.InitialDirectory = MainForm->CurrentProjectFilePath; + "Scene.hscn"
        ofd.FilterPatterns.Add("*.hscn");
        // ofd.FilterPatterns.Add("*.tmx");
        ofd.FilterPatterns.Add("*.bin");
        ofd.Multiselect = false;

        if (UI::SystemDialog::OpenFile(&ofd)) {
            for (int i = 0; i < ofd.Filenames.Count(); i++)
                MainForm->OpenFile(ofd.Filenames[i]);
        }
    }
    static void Action_SaveResource() {
        int index = MainForm->MainTabControl->SelectedIndex;
        if (index < 0 || index >= MainForm->Editors.Count())
            return;

        if (MainForm->Editors[index]->JustCreated) {
            MainForm->Editors[index]->PromptSaveAs();
        }
        else {
            MainForm->Editors[index]->Save();
        }
    }
    static void Action_SaveResourceAs() {
        int index = MainForm->MainTabControl->SelectedIndex;
        if (index < 0 || index >= MainForm->Editors.Count())
            return;

        MainForm->Editors[index]->PromptSaveAs();
    }
    static void Action_SaveAllResources() {
        for (int index = 0; index < MainForm->Editors.Count(); index++) {
            MainForm->Editors[index]->Save();
        }
    }
    static void Action_CloseResource() {
        int index = MainForm->MainTabControl->SelectedIndex;
        if (index < 0 || index >= MainForm->Editors.Count())
            return;

        if (!MainForm->Editors[index]->CloseFile())
            return;

        delete MainForm->Editors[index];
        MainForm->Editors.RemoveAt(index);
        MainForm->MainTabControl->TabPages.RemoveAt(index);
        MainForm->ReflectCurrentFileEditorChange();
    }
    static void Action_CloseAllResources() {
        for (int index = 0; index < MainForm->Editors.Count(); index++) {
            if (!MainForm->Editors[index]->CloseFile()) {
                break;
            }

            delete MainForm->Editors[index];
            MainForm->Editors.RemoveAt(index);
            MainForm->MainTabControl->TabPages.RemoveAt(index);
            index--;
        }
        MainForm->ReflectCurrentFileEditorChange();
    }
    static void Action_Exit() {
        // MainForm->Close();
        SDL_Event e;
        e.type = SDL_QUIT;
        SDL_PushEvent(&e);
    }
    #pragma endregion
    #pragma region "Project" menu actions
    static void Action_BuildGameLogic() {

    }
    static void Action_RunLocally() {
        if (MainForm->CurrentProject == NULL)
            return;

        char filePath[256];
        char appPath[256];
        char cmdLine[512];

        int index = MainForm->MainTabControl->SelectedIndex;
        if (index >= 0 && index < MainForm->Editors.Count()) {
            String* title = &MainForm->Editors[index]->FilePath;
            if (title->Length > 0) {
                Strings::ToCString(filePath, title);

                snprintf(appPath, 256, "%s/%s.exe", MainForm->CurrentProjectFolderPath, MainForm->CurrentProject->ProjectName);
                if (MainForm->Preferences->RunFromStartScene)
                    snprintf(cmdLine, 512, "%s", appPath);
                else
                    snprintf(cmdLine, 512, "%s -s %s", appPath, filePath + strlen(MainForm->CurrentProjectFolderPath) + strlen("/Resources/"));

                UI::SystemDialog::StartProcess(appPath, cmdLine, MainForm->CurrentProjectFolderPath);
            }
        }
    }
    static void Action_RunRemotely() {

    }
    static void Action_PackResources() {

    }
    static void Action_PackAssetFolder() {

    }
    #pragma endregion
    #pragma region "Help" menu actions
    static void Action_Documentation() {

    }
    static void Action_AboutHatchStudio() {

    }
    #pragma endregion

    static void Action_Check_RunFromStartScene() {
        MainForm->menuRunFromScene->EditItem(MainForm->menuIndex_RunFromStartScene, "Run From Start Scene", Action_Check_RunFromStartScene, UI::Menu::SM_NONE, true, UI::Menu::ItemType::IT_RADIO_CHECKED);
        MainForm->menuRunFromScene->EditItem(MainForm->menuIndex_RunFromCurrentScene, "Run From Current Scene", Action_Check_RunFromCurrentScene, UI::Menu::SM_NONE, true, UI::Menu::ItemType::IT_RADIO_UNCHECKED);
        MainForm->Preferences->RunFromStartScene = true;
    }
    static void Action_Check_RunFromCurrentScene() {
        MainForm->menuRunFromScene->EditItem(MainForm->menuIndex_RunFromStartScene, "Run From Start Scene", Action_Check_RunFromStartScene, UI::Menu::SM_NONE, true, UI::Menu::ItemType::IT_RADIO_UNCHECKED);
        MainForm->menuRunFromScene->EditItem(MainForm->menuIndex_RunFromCurrentScene, "Run From Current Scene", Action_Check_RunFromCurrentScene, UI::Menu::SM_NONE, true, UI::Menu::ItemType::IT_RADIO_CHECKED);
        MainForm->Preferences->RunFromStartScene = false;
    }

    void MenuSetup() {
        mainMenu = new UI::Menu();
        menuFile = new UI::Menu();
        menuRecentProjects = new UI::Menu();
        menuNewResource = new UI::Menu();
        menuProject = new UI::Menu();
        menuRunFromScene = new UI::Menu();
        menuHelp = new UI::Menu();

        // "File" menu
        menuFile->AddItem("New Project...", Action_NewProject, SHORTCUT_NEW_PROJECT, true, UI::Menu::ItemType::IT_TEXT, 'N');
        menuFile->AddItem("Open Project...", Action_OpenProject, SHORTCUT_OPEN_PROJECT, true, UI::Menu::ItemType::IT_TEXT, 'O');
        menuFile->AddSubmenu("Recent Projects", menuRecentProjects, 'R');
        menuFile->AddItem("Close Project", Action_CloseProject, SHORTCUT_CLOSE_PROJECT, true, UI::Menu::ItemType::IT_TEXT, 'C');
        menuFile->AddSeparator();
        menuFile->AddSubmenu("New Resource", menuNewResource, SHORTCUT_NEW_FILE);
        menuFile->AddItem("Open Resource...", Action_OpenResource, SHORTCUT_OPEN_FILE, true);
        menuFile->AddSeparator();
        menuIndex_SaveFile = menuFile->AddItem("Save", Action_SaveResource, SHORTCUT_SAVE_FILE, true, UI::Menu::ItemType::IT_TEXT, 'S');
        menuIndex_SaveFileAs = menuFile->AddItem("Save As...", Action_SaveResourceAs, SHORTCUT_SAVE_FILE_AS, true, UI::Menu::ItemType::IT_TEXT, 'A');
        menuIndex_SaveAllFile = menuFile->AddItem("Save All", Action_SaveAllResources, SHORTCUT_SAVE_ALL, true);
        menuFile->AddSeparator();
        menuIndex_CloseFile = menuFile->AddItem("Close", Action_CloseResource, SHORTCUT_CLOSE_FILE, true);
        menuIndex_CloseAllFiles = menuFile->AddItem("Close All", Action_CloseAllResources, SHORTCUT_CLOSE_ALL, true);
#if defined(_WINDOWS)
        menuFile->AddSeparator();
        menuFile->AddItem("Exit", Action_Exit, UI::Menu::SM_NONE, true);
#endif

        // "Recent Projects" menu
        if (Preferences->RecentProjects.Count()) {
            for (int i = 0; i < Preferences->RecentProjects.Count(); i++) {
                menuRecentProjects->AddItem(Preferences->RecentProjects[i].Name, NULL, UI::Menu::SM_NONE, true);
            }
            menuRecentProjects->AddSeparator();
        }
        menuRecentProjects->AddItem("Clear Recent Projects", Action_ClearRecentProjects, UI::Menu::SM_NONE, true);

        // "New Resource" menu
        menuNewResource->AddItem("Scene", Action_NewSceneResource, UI::Menu::SM_NONE, true);
        menuNewResource->AddItem("Tile Collision", Action_NewTileCollisionResource, UI::Menu::SM_NONE, true);

        // "Project" menu
        menuProject->AddItem("Build Game Logic", Action_BuildGameLogic, SHORTCUT_BUILD_GAME_LOGIC, false);
        menuProject->AddSeparator();
        menuProject->AddItem("Run Locally", Action_RunLocally, SHORTCUT_RUN_LOCALLY, true);
        menuProject->AddItem("Run On Device...", Action_RunRemotely, SHORTCUT_RUN_REMOTELY, false);
        // Shows any devices on the local network that are actively running the HatchLite application (runs a broadcast or something on a thread)
        // OR any users that are connected to an open lobby
        menuProject->AddSubmenu("Set Run Start Scene", menuRunFromScene);
        menuProject->AddSeparator();
        menuProject->AddItem("Pack Resources", Action_PackResources, UI::Menu::SM_NONE, false);
        menuProject->AddItem("Pack Asset Folder...", Action_PackAssetFolder, UI::Menu::SM_NONE, false);
        menuProject->AddSeparator();
        menuProject->AddItem("Create Release Bundle...", NULL, UI::Menu::SM_NONE, false);

        // "Set Run Start Scene" menu
        menuIndex_RunFromStartScene = menuRunFromScene->AddItem("Run From Start Scene", Action_Check_RunFromStartScene, UI::Menu::SM_NONE, true,
            Preferences->RunFromStartScene ? UI::Menu::ItemType::IT_RADIO_CHECKED : UI::Menu::ItemType::IT_RADIO_UNCHECKED);
        menuIndex_RunFromCurrentScene = menuRunFromScene->AddItem("Run From Current Scene", Action_Check_RunFromCurrentScene, UI::Menu::SM_NONE, true,
            Preferences->RunFromStartScene ? UI::Menu::ItemType::IT_RADIO_UNCHECKED : UI::Menu::ItemType::IT_RADIO_CHECKED);

        // "Help" menu
        menuHelp->AddItem("Documentation", Action_Documentation, UI::Menu::SM_NONE, false);
#if defined(_WINDOWS)
        menuHelp->AddSeparator();
        menuHelp->AddItem("About HatchStudio", Action_AboutHatchStudio, UI::Menu::SM_NONE, false);
#endif

        // Main menu (MacOS)
#if defined(_MACOS)
        menuApple = new UI::Menu();
        menuWindow = new UI::Menu();
        mainMenu->AddSubmenu("HatchStudio", menuApple);
        mainMenu->AddSubmenu("File", menuFile);
        mainMenu->AddSubmenu("Project", menuProject);
        mainMenu->AddSubmenu("Window", menuWindow);
        mainMenu->AddSubmenu("Help", menuHelp);

        UI::Menu::SetAppleMenu(menuApple);
        UI::Menu::SetWindowMenu(menuWindow);
        UI::Menu::SetHelpMenu(menuHelp);
#else
        // Main menu (Anywhere else)
        mainMenu->AddSubmenu("File", menuFile, 'F');
        mainMenu->AddSubmenu("Project", menuProject, 'P');
        mainMenu->AddSubmenu("Help", menuHelp, 'H');
#endif

#ifdef USE_NATIVE_MENU
        UI::Menu::SetNativeMainMenu(mainMenu);
#else
        MenuBarControl->SetMenu(mainMenu);
#endif
    }

    HatchProject* CurrentProject = NULL;
    char* CurrentProjectFilePath = NULL;
    char* CurrentProjectFolderPath = NULL;

    CString ProjectName = NULL;
    CString PresenceState = "";
    Sint64  PresenceStartTime = 0;
    HatchStudioSettings* Preferences = NULL;
    ArrayList<ResourceEditor*> Editors;
    TabControl* MainTabControl = NULL;
    MenuBar* MenuBarControl = NULL;
    HatchStudioForm() : Form(100, 100, NULL) { }
    ~HatchStudioForm() {
        delete MainTabControl;
        delete MenuBarControl;
    }

    bool LoadSettings() {
        Stream* stream = FileStream::New("Preferences.pref", FileStream::READ_ACCESS);
        if (stream) {
            Preferences->Read(stream);
            stream->Close();

            return true;
        }
        return false;
    }
    void SaveSettings() {
        Preferences->Maximized = (SDL_GetWindowFlags(UI::Graphics::Renderer::Window) & SDL_WINDOW_MAXIMIZED);
        if (!Preferences->Maximized) {
            SDL_GetWindowSize(UI::Graphics::Renderer::Window, &Preferences->WindowWidth, &Preferences->WindowHeight);
            SDL_GetWindowPosition(UI::Graphics::Renderer::Window, &Preferences->WindowX, &Preferences->WindowY);
        }

        Stream* stream = FileStream::New("Preferences.pref", FileStream::WRITE_ACCESS);
        if (stream) {
            Preferences->Write(stream);
            stream->Close();
        }
    }

    void OnClosing(FormClosingEventArgs* e) {
        // TODO: Save last open files to project
        e->Cancel = !CloseCurrentProject();
        Form::OnClosing(e);
    }
    void OnClosed(FormClosedEventArgs* e) {
        SaveSettings();

        // Hide the window to make the exit *look* graceful
        SDL_HideWindow(UI::Graphics::Renderer::Window);

        Form::OnClosed(e);
    }

    void UpdatePresence() {
        char stringBuffer[256];
        if (CurrentProject != NULL) {
            sprintf(stringBuffer, "Working on \"%s\"", CurrentProject->ProjectName);
            GameLinker::ServiceFuncs.UserData.UpdateRichPresence(PresenceState, stringBuffer, "logo", PresenceStartTime, 0);
        }
        else {
            GameLinker::ServiceFuncs.UserData.UpdateRichPresence("", "", "logo", PresenceStartTime, 0);
        }
    }
    void ReflectProjectNameChange() {
        if (CurrentProject == NULL) {
            SetTitle("HatchStudio");
        }
        else {
            char stringBuffer[256];
            sprintf(stringBuffer, "%s - HatchStudio", CurrentProject->ProjectName);
            SetTitle(stringBuffer);
        }

        UpdatePresence();
    }
    void ReflectCurrentFileEditorChange() {
		char menuItemTitle[128];
        char currentFilename[128];

        menuFile->EditItem(menuIndex_SaveAllFile, "Save All", Action_SaveAllResources, SHORTCUT_SAVE_ALL, MainForm->Editors.Count() > 0);
        menuFile->EditItem(menuIndex_CloseAllFiles, "Close All", Action_CloseAllResources, SHORTCUT_CLOSE_ALL, MainForm->Editors.Count() > 0);

        if (CurrentProject != NULL) {
            int index = MainForm->MainTabControl->SelectedIndex;
            if (index >= 0 && index < MainForm->Editors.Count()) {
                String* title = &Editors[index]->Title;
                if (title->Length > 0) {
                    Strings::ToCString(currentFilename, title);

                    sprintf(menuItemTitle, "Save %s", currentFilename);
                    menuFile->EditItem(menuIndex_SaveFile, menuItemTitle, Action_SaveResource, SHORTCUT_SAVE_FILE, true);

                    sprintf(menuItemTitle, "Save %s As...", currentFilename);
                    menuFile->EditItem(menuIndex_SaveFileAs, menuItemTitle, Action_SaveResourceAs, SHORTCUT_SAVE_FILE_AS, true);

                    sprintf(menuItemTitle, "Close %s", currentFilename);
                    menuFile->EditItem(menuIndex_CloseFile, menuItemTitle, Action_CloseResource, SHORTCUT_CLOSE_FILE, true);
                    return;
                }
            }
        }

        menuFile->EditItem(menuIndex_SaveFile, "Save", Action_SaveResource, SHORTCUT_SAVE_FILE, false);
        menuFile->EditItem(menuIndex_SaveFileAs, "Save As...", Action_SaveResourceAs, SHORTCUT_SAVE_FILE_AS, false);
        menuFile->EditItem(menuIndex_CloseFile, "Close", Action_CloseResource, SHORTCUT_CLOSE_FILE, false);
    }
    void UpdateRecentProjectsMenu() {
        menuRecentProjects->ClearItems();

        if (Preferences->RecentProjects.Count()) {
            for (int i = 0; i < Preferences->RecentProjects.Count(); i++) {
                menuRecentProjects->AddItem(Preferences->RecentProjects[i].Name, NULL, UI::Menu::SM_NONE, true);
            }
            menuRecentProjects->AddSeparator();
        }
        menuRecentProjects->AddItem("Clear Recent Projects", Action_ClearRecentProjects, UI::Menu::SM_NONE, true);
    }
    void ClearRecentProjects() {
        Preferences->RecentProjects.Clear();
        UpdateRecentProjectsMenu();
    }

    static char* FromLiteral(const char* str) {
        size_t len = strlen(str);
        char* buf = (char*)malloc(len + 1);
        if (!buf)
            return NULL;
        memcpy(buf, str, len);
        buf[len] = 0;
        return buf;
    }

    void InitProject() {

    }
    void NewProject() {
        Form_NewProjectWizard* dialog = new Form_NewProjectWizard();
        dialog->BackColor = BackColor;

        UI::System::Application::ShowDialog(dialog, [this, dialog](DialogResult result) -> void {
            if (result == DialogResult::OK) {
                char stringBuf[256];
                auto oldProject = CurrentProject;

                CurrentProject = new HatchProject();

                Strings::ToCString(stringBuf, &dialog->textBoxProjectName->Text);
                CurrentProject->ProjectName = FromLiteral(stringBuf);

                CurrentProject->EngineVersion = dialog->comboBoxEngineVersion->SelectedIndex;

                Strings::ToCString(stringBuf, &dialog->textBoxShortName->Text);
                CurrentProject->ShortName = FromLiteral(stringBuf);

                UI::SystemDialog::SaveFileData sfd;
                sfd.Title = "Select a destination for the project file...";
                // sfd.InitialDirectory = MainForm->CurrentProjectFolderPath;
                sfd.FilterPatterns.Add("*.HPROJ");

                if (UI::SystemDialog::SaveFile(&sfd)) {
                    const char* filepath = sfd.Filename;

                    CurrentProjectFilePath = FromLiteral(filepath);
                    if (CurrentProjectFilePath) {
                        CurrentProjectFilePath = UI::Filesystem::Paths::SanitizePath(CurrentProjectFilePath);
                        CurrentProjectFolderPath = UI::Filesystem::Paths::GetEnclosingFolder(new char[256], filepath);

                        SaveProject(filepath);

                        Preferences->RecentProjects.Insert(0, RecentProject { FromLiteral(CurrentProject->ProjectName), CurrentProjectFilePath });
                        UpdateRecentProjectsMenu();
                    }
                }
                else {
                    delete CurrentProject;
                    CurrentProject = oldProject;
                }

                ReflectProjectNameChange();
            }
        });
    }
    bool OpenProject(const char* filepath) {
        Stream* stream;
        auto oldProj = CurrentProject;

        stream = FileStream::New(filepath, FileStream::READ_ACCESS);
        if (!stream)
            goto FailAndFree;

        CurrentProject = new HatchProject();
        CurrentProject->Read(stream);
        if (false)
            goto FailAndFree;

        stream->Close();
        stream = NULL;

        // Success
        CurrentProjectFilePath = FromLiteral(filepath);
        if (!CurrentProjectFilePath)
            goto FailAndFree;

        CurrentProjectFilePath = UI::Filesystem::Paths::SanitizePath(CurrentProjectFilePath);
        CurrentProjectFolderPath = UI::Filesystem::Paths::GetEnclosingFolder(new char[256], filepath);

        // Link the game logic before we even open any files
        ::GameLinker::Load(CurrentProjectFolderPath);

        for (int i = 0; i < CurrentProject->LastOpenFiles.Count(); i++) {
            OpenFile(CurrentProject->LastOpenFiles[i]);
        }

        ReflectProjectNameChange();

        {
            bool alreadyExists = false;
            for (int i = 0; i < Preferences->RecentProjects.Count(); i++) {
                auto rp = Preferences->RecentProjects[i];
                if (strcmp(rp.Filepath, CurrentProjectFilePath) == 0) {
                    Preferences->RecentProjects.RemoveAt(i);
                    Preferences->RecentProjects.Insert(0, rp);
                    alreadyExists = true;
                    break;
                }
            }
            if (!alreadyExists)
                Preferences->RecentProjects.Insert(0, RecentProject { FromLiteral(CurrentProject->ProjectName), CurrentProjectFilePath });
        }


        UpdateRecentProjectsMenu();
        return true;

    FailAndFree:
        if (CurrentProject) {
            delete CurrentProject;
            CurrentProject = NULL;
        }
        if (stream) {
            stream->Close();
        }
        CurrentProject = oldProj;
        return false;
    }
    void SaveProject(const char* filepath) {
        Stream* stream;

        stream = FileStream::New(filepath, FileStream::WRITE_ACCESS);
        if (!stream)
            goto FailAndFree;

        CurrentProject->Write(stream);
        if (false)
            goto FailAndFree;

        stream->Close();
        stream = NULL;

        // Success
        return;

    FailAndFree:
        if (stream) {
            stream->Close();
        }
    }
    bool CloseCurrentProject() {
        char stringBuffer[256];
        if (CurrentProject == NULL)
            return true;

        // Free old open file list, add current open files to it
        for (int i = 0; i < CurrentProject->LastOpenFiles.Count(); i++)
            free(CurrentProject->LastOpenFiles[i]);
        CurrentProject->LastOpenFiles.Clear();

        for (int i = 0; i < Editors.Count(); i++) {
            Strings::ToCString(stringBuffer, &Editors[i]->FilePath);
            CurrentProject->LastOpenFiles.Add(FromLiteral(stringBuffer));
        }

        // Try to close each file
        for (int i = 0; i < Editors.Count(); i++) {
            if (!Editors[i]->CloseFile()) {
                return false;
            }
            delete Editors[i];
        }
        Editors.Clear();
        MainTabControl->TabPages.Clear();

        SaveProject(CurrentProjectFilePath);
        delete CurrentProject;
        CurrentProject = NULL;

        ReflectProjectNameChange();
        ReflectCurrentFileEditorChange();
        return true;
    }

    void AddEditor(ResourceEditor* editor) {
        Editors.Insert(0, editor);
        MainTabControl->TabPages.Insert(0, editor);

        MainTabControl->Select(0);
    }

    void NewSceneFile() {
        ResourceEditor* editor = new SceneEditor();
        editor->New();

        AddEditor(editor);
    }
    void NewTileCollisionFile() {
        ResourceEditor* editor = new TileCollisionEditor();
        editor->New();

        AddEditor(editor);
    }
    bool OpenFile(const char* filepath) {
        char resourceFolder[1024];
        sprintf(resourceFolder, "%s/Resources", CurrentProjectFolderPath);

        size_t parentPathLen = strlen(resourceFolder);
        if (strncmp(filepath, resourceFolder, parentPathLen) != 0 || filepath[parentPathLen] != '/') {
            // TODO: Make this a UI::Form dialog
            const SDL_MessageBoxButtonData buttons[] = {
                { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "OK" },
            };
            const SDL_MessageBoxData messageboxdata = {
                SDL_MESSAGEBOX_INFORMATION, UI::Graphics::Renderer::Window,
                "Non-Project Resource",
                "Resource must be in project's Resource folder.",
                SDL_arraysize(buttons), buttons, NULL
            };

            int buttonid;
            if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) {
                SDL_Log("error displaying message box");
                return false;
            }
            return false;
        }

        return OpenEditorForFile(filepath);
    }
    bool OpenEditorForFile(const char* filepath) {
        ResourceEditor* editor = NULL;

        bool didOpen = false;

        // Figure out file format
        ResourceFileType fileType = ResourceFileType::Unknown;

        if (stristr(filepath, ".tmx") != NULL) {
            fileType = ResourceFileType::Scene_Tiled;
        }
        else {
            Stream* stream = FileStream::New(filepath, FileStream::READ_ACCESS);
            if (!stream) {
                Diagnostics::SetError("Could not open file: %s", Diagnostics::ErrorString);
                return false;
            }

            // Check if SceneEditor supports it
            fileType = SceneEditor::GetFileType(stream);
            if (fileType != ResourceFileType::Unknown) {
                editor = new SceneEditor();
                didOpen = editor->Open(filepath, fileType);
            }

            // Check if TileCollisionEditor supports it
            if (fileType == ResourceFileType::Unknown) {
                fileType = TileCollisionEditor::GetFileType(stream);
                if (fileType != ResourceFileType::Unknown) {
                    editor = new TileCollisionEditor();
                    didOpen = editor->Open(filepath, fileType);
                }
            }

            stream->Close();
        }

        if (!didOpen) {
            Diagnostics::SetError("Unknown or invalid file format.");
            delete editor;
            return false;
        }

        AddEditor(editor);
        return true;
    }

    void OnTabChange(void* sender, EventArgs* e) {
        int editorType = Editors[MainTabControl->SelectedIndex]->GetEditorType();

        switch (editorType) {
        case EditorTypes::SCENE:
            PresenceState = "Editing a scene";
            break;
        case EditorTypes::SPRITE:
            PresenceState = "Editing a sprite";
            break;
        case EditorTypes::TILECONFIG:
            PresenceState = "Editing tile collision";
            break;
        }

        ReflectCurrentFileEditorChange();
        UpdatePresence();
    }

    void Load() {
        Form::Load();
        MainForm = this;

        BackColor = Color(0x21252B, 0xFF);

        MainTabControl = new TabControl();
        MainTabControl->Dock = DOCK_FILL;
        MainTabControl->SelectedIndex = 0;
        MainTabControl->Alignment = TabAlignment::Top;
        MainTabControl->MaxSize = 200;
        MainTabControl->onSelected += std::bind(&HatchStudioForm::OnTabChange, this, std::placeholders::_1, std::placeholders::_2);
        Controls.Add(MainTabControl);

#ifndef USE_NATIVE_MENU
        MenuBarControl = new MenuBar();
        MenuBarControl->Size = { Size.Get().W, MenuBarControl->ItemHeight };
        Controls.Add(MenuBarControl);
#endif

        SDL_Rect displayBounds;
        if (SDL_GetDisplayBounds(0, &displayBounds) == 0) {
            // success
        }

        ::Size startSize = { 1280, 720 };
		startSize.W = M_MIN(startSize.W, displayBounds.w - 40);
		startSize.H = M_MIN(startSize.H, displayBounds.h * 3 / 4);

        // Load settings
        Preferences = new HatchStudioSettings();
        if (LoadSettings()) {
            startSize.W = Preferences->WindowWidth;
            startSize.H = Preferences->WindowHeight;
        }

        // Set up the menu
        try {
            MenuSetup();
        }
        catch (const char* err) {
            fprintf(stderr, "Couldn't setup menu: %s\n", err);
            exit(EXIT_FAILURE);
        }

        // Resize window
        Size = startSize;
        SDL_SetWindowSize(UI::Graphics::Renderer::Window, startSize.W, startSize.H);
        SDL_SetWindowPosition(UI::Graphics::Renderer::Window, SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0));
        if (Preferences->Maximized)
            SDL_MaximizeWindow(UI::Graphics::Renderer::Window);

        // Set up presence timer
        PresenceStartTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        Studio::ResourcePathPrefix = &CurrentProjectFolderPath;

        // Reopen last project if desired
        if (Preferences->ReopenLastProject && Preferences->RecentProjects.Count() > 0) {
            OpenProject(Preferences->RecentProjects[0].Filepath);
        }
        else {
            ReflectProjectNameChange();
            ReflectCurrentFileEditorChange();
        }

        SDL_ShowWindow(UI::Graphics::Renderer::Window);
    }

    void HandleSDLEvent(SDL_Event* e) {
        switch (e->type) {
        case SDL_KEYDOWN:
            CheckShortcuts(e->key.keysym.sym, (SDL_Keymod)e->key.keysym.mod);
            break;

        case SDL_WINDOWEVENT:
            switch (e->window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                Size = { e->window.data1, e->window.data2 };
                OnResized(NULL);
                break;
            }
            break;
        }

        // Handle the menu bar control first
        MenuBarControl->HandleSDLEvent(e);

        if (MenuBarControl->Dropdown == NULL) {
            // If a dropdown is up then don't handle the other controls
            for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
                if (Controls.Items[i] != MenuBarControl) {
                    Controls.Items[i]->HandleSDLEvent(e);
                }
            }
        }
    }
};

HatchStudioForm* HatchStudioForm::MainForm = NULL;

int main(int argc, char** args) {
    // Handle options here
    bool option_PackAssets = false;
    char option_AssetFolder[256] = { 0 };
    char option_AssetFilePath[256] = { 0 };
    while (true) {
        switch (UI::System::Application::ParseOptions(argc, args, "p:i:o:h")) {
        case 'p':
            option_PackAssets = true;
            continue;

        case 'i':
            strncpy(option_AssetFolder, UI::System::Application::optarg, 255);
            continue;

        case 'o':
            strncpy(option_AssetFilePath, UI::System::Application::optarg, 255);
            continue;

        case '?':
        case 'h':
        default:
            printf("Command Line Help Guide:\n");
            printf("-h                   | Display this help message.\n");
            printf("-p [version]         | Starts the packing program. Follow the option with a 'G' for Hatch Game Engine, or 'L' for HatchLite.\n");
            printf("-i [resourceFolder]  | Sets the Resources folder to pack.\n");
            printf("-o [outputPath]      | Sets the output file path for the packer.\n");
            break;

        case -1:
            break;
        }
        break;
    }

    if (option_PackAssets) {
        if (option_AssetFolder[0] == '\0') {
            fprintf(stderr, "Resources folder not specified. Use -h for help.\n");
            return 0;
        }
        if (option_AssetFilePath[0] == '\0') {
            fprintf(stderr, "Asset pack output file path not specified. Use -h for help.\n");
            return 0;
        }
        return 0;
    }

    UI::System::Application::Start(argc, args, new HatchStudioForm());
	return 0;
}

#include <Studio/Editors/ResourceEditor.hpp>

#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Strings.h>
#include <Hatch/IO/Stream.h>
#include <Hatch/IO/FileStream.h>

#include <UI/Filesystem/Paths.hpp>
#include <UI/Graphics/Font.hpp>
#include <UI/Graphics/Renderer.hpp>
#include <UI/System/SystemDialog.hpp>

namespace Studio {
    ResourceEditor::ResourceEditor() : TabPage() {
        Strings::Init(&FilePath, 1);

        SetChangesUnsaved();

        JustCreated = false;
    }

    bool ResourceEditor::Open(CString filename) {
        Strings::FromCString(&FilePath, filename, 0);

        SetChangesSaved();

        return Open();
    }
    bool ResourceEditor::SaveAs(CString filename) {
        Strings::FromCString(&FilePath, filename, 0);
        return Save();
    }
    bool ResourceEditor::CloseFile() {
        if (UnsavedChanges) {
            int ret = PromptSaveChanges();

            // Save
            if (ret == 1) {
                Save();
            }
            // Cancel
            else if (ret == 2) {
                return false;
            }
        }
        return true;
    }

    bool ResourceEditor::PromptSaveAs() {
        char filePath[512];
        Strings::ToCString(filePath, &FilePath);

        char filter[64] = { '*', '\0' };
        UI::Filesystem::Paths::GetExtension(&filter[1], filePath);

        UI::SystemDialog::SaveFileData sfd;
        sfd.Title = "Select a destination for the file...";
        sfd.InitialDirectory = filePath;
        sfd.FilterPatterns.Add(filter);

        if (UI::SystemDialog::SaveFile(&sfd)) {
            SaveAs(sfd.Filename);
            return true;
        }
        return false;
    }
    int ResourceEditor::PromptSaveChanges() {
        char filenameBuffer[256];
        if (FilePath.Length > 0)
            Strings::ToCString(filenameBuffer, &FilePath);
        else
            strcpy(filenameBuffer, "untitled");

        char titleStringBuffer[128];
        char* split = strrchr(filenameBuffer, '/');
        if (split)
            sprintf(titleStringBuffer, "'%s' has changes, do you want to save them?", split + 1);
        else
            sprintf(titleStringBuffer, "'%s' has changes, do you want to save them?", filenameBuffer);

        const SDL_MessageBoxButtonData buttons[] = {
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Save" },
            {                                       0, 0, "Don't Save" },
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "Cancel" },
        };
        const SDL_MessageBoxData messageboxdata = {
            SDL_MESSAGEBOX_INFORMATION, UI::Graphics::Renderer::Window,
            titleStringBuffer,
            "Your changes will be lost if you close this item without saving.",
            SDL_arraysize(buttons), buttons, NULL
        };

        int buttonid;
        if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) {
            SDL_Log("error displaying message box");
            return 2;
        }

        return buttonid;
    }

    void ResourceEditor::SetChangesUnsaved() {
        UnsavedChanges = true;
        UpdateTitle();
    }

    void ResourceEditor::SetChangesSaved() {
        UnsavedChanges = false;
        JustCreated = false;
        UpdateTitle();
    }

    void ResourceEditor::UpdateTitle() {
        char filenameBuffer[256];
        if (FilePath.Length > 0)
            Strings::ToCString(filenameBuffer, &FilePath);
        else
            strcpy(filenameBuffer, "untitled");

        char stringBuffer[128];
        UI::Filesystem::Paths::GetFilename(stringBuffer, filenameBuffer);
        if (UnsavedChanges) {
            strcat(stringBuffer, "*");
        }

        SetTitle(stringBuffer);
    }
}

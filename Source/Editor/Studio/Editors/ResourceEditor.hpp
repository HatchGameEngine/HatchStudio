#pragma once

#include <Studio/Enums.hpp>

#include <UI/Controls/TabControls.hpp>

namespace Studio {
    struct ResourceEditor : TabPage {
    public:
        String FilePath;
        ResourceFileType FileType;
        bool UnsavedChanges;
        bool JustCreated;

        ResourceEditor();

        virtual void New() = 0;
        virtual bool Open() = 0;
        virtual bool Save() = 0;
        virtual int GetEditorType() = 0;

        bool Open(CString filename);
        bool Open(CString filename, ResourceFileType fileType);
        bool SaveAs(CString filename);
        bool CloseFile();

        bool PromptSaveAs();
        int PromptSaveChanges();

        void SetChangesUnsaved();
        void SetChangesSaved();

    protected:
        void UpdateTitle();
    };
}

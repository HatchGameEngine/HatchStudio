#pragma once

#include <UI/Controls/TabControls.hpp>

namespace Studio {
    struct ResourceEditor : TabPage {
    public:
        String FilePath;
        bool UnsavedChanges;

        ResourceEditor();

        virtual void New() = 0;
        virtual bool Open() = 0;
        virtual bool Save() = 0;
        virtual int GetEditorType() = 0;

        bool Open(CString filename);
        bool SaveAs(CString filename);
        bool CloseFile();

        bool PromptSaveAs();
        int PromptSaveChanges();

    protected:
        void UpdateTitle();
    };
}

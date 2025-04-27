#pragma once

#include <UI/Components/Collections.hpp>

namespace UI {
    namespace SystemDialog {
        struct FileData {
            const char* Title = NULL;
            const char* InitialDirectory = NULL;
            List<const char*> FilterPatterns;
        };
        struct OpenFileData : FileData {
            bool Multiselect = false;
            List<char*> Filenames;

            ~OpenFileData() { for (int i = 0; i < Filenames.Count(); i++) { free(Filenames[i]); } }
        };
        struct SaveFileData : FileData {
            const char* Filename = NULL;
        };
        struct OpenFolderData {
            const char* Title = NULL;
            const char* InitialDirectory = NULL;
            const char* FolderPath = NULL;
        };

        struct ColorPickData {
            const char* Title = NULL;
            Uint8 DefaultRGB[3] = { 0, 0, 0 };
        };

        int ShowMessageBox();
        bool OpenFile(OpenFileData* openFileData);
        bool SaveFile(SaveFileData* saveFileData);
        bool OpenFolder(OpenFolderData* openFolderData);
        bool PickColor(ColorPickData* colorPickData);

        bool StartProcess(const char* appPath, const char* cmd, const char* startDir);
    }
}

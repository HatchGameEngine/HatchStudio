#include <string>
#include <UI/Filesystem/Paths.hpp>
#include <UI/System/SystemDialog.hpp>
#include <Libraries/tinyfiledialogs.h>

// https://sourceforge.net/projects/tinyfiledialogs/

namespace UI {
    namespace SystemDialog {
        void EnsureSettings() {
            tinyfd_verbose = 1;  /* default is 0 */
            tinyfd_silent = 0;  /* default is 1 */
            tinyfd_forceConsole = 0; /* default is 0 */
            tinyfd_assumeGraphicDisplay = 1; /* default is 0 */
            #ifdef _WIN32
                tinyfd_winUtf8 = 1; /* default is 1 */
            #endif
        }

        void SplitString(List<char*>& out, const char* text, char delimiter, int maxCount = 0) {
            int splitCount = 0;
            const char* start = text;
            const char* head = text;
            while (true) {
                if (*head == '\0' || *head == delimiter) {
                    char* substring = (char*)malloc(head - start + 1);
                    if (!substring)
                        return;

                    memcpy(substring, start, head - start);
                    substring[head - start] = '\0';

                    out.Add(substring);

                    splitCount++;
                    if (*head == '\0' || (maxCount > 0 && splitCount >= maxCount))
                        break;

                    start = head + 1;
                }
                head++;
            }
        }

        bool OpenFile(OpenFileData* openFileData) {
            if (!openFileData)
                return false;

            EnsureSettings();

            const char* result = tinyfd_openFileDialog(
                openFileData->Title, openFileData->InitialDirectory,
                openFileData->FilterPatterns.Count(), openFileData->FilterPatterns.Items,
                NULL, openFileData->Multiselect);

            if (result) {
                SplitString(openFileData->Filenames, result, '|');
                for (int i = 0; i < openFileData->Filenames.Count(); i++) {
                    openFileData->Filenames[i] = UI::Filesystem::Paths::SanitizePath(openFileData->Filenames[i]);
                }
                return true;
            }

            return false;
        }
        bool SaveFile(SaveFileData* saveFileData) {
            if (!saveFileData)
                return false;

            EnsureSettings();

            saveFileData->Filename = tinyfd_saveFileDialog(
                saveFileData->Title, saveFileData->InitialDirectory,
                saveFileData->FilterPatterns.Count(), saveFileData->FilterPatterns.Items,
                NULL);

            if (saveFileData->Filename) {
                UI::Filesystem::Paths::SanitizePath((char*)saveFileData->Filename);
                return true;
            }

            return false;
        }
        bool OpenFolder(OpenFolderData* openFolderData) {
            if (!openFolderData)
                return false;

            EnsureSettings();

            openFolderData->FolderPath = tinyfd_selectFolderDialog(openFolderData->Title, openFolderData->InitialDirectory);

            if (openFolderData->FolderPath) {
                // TODO: SanitizePath here
                return true;
            }

            return false;
        }
        bool PickColor(ColorPickData* colorPickData) {
            if (!colorPickData)
                return false;

            EnsureSettings();

            const char* result = tinyfd_colorChooser(
                colorPickData->Title, NULL, colorPickData->DefaultRGB, colorPickData->DefaultRGB);

            if (result)
                return true;

            return false;
        }
    }
}

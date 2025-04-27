#include <UI/Filesystem/Paths.hpp>

#include <string>
#include <cstring>

namespace UI::Filesystem::Paths {
    char* SanitizePath(char* path) {
        // TODO: '\' replace with '/'
        char* c = path;
        while (*c) {
            if (*c == '\\')
                *c = '/';
            c++;
        }
        return path;
    }
    char* GetEnclosingFolder(char* stringBuffer, const char* filePath) {
        strcpy(stringBuffer, filePath);

        char* lastSeparator = strrchr(stringBuffer, '/');
        if (lastSeparator != NULL)
            *lastSeparator = '\0';
        else
            *stringBuffer = '\0';
        return stringBuffer;
    }
    char* GetSiblingFilePath(char* stringBuffer, const char* filePath, const char* siblingFilename) {
        strcpy(stringBuffer, filePath);

        char* lastSeparator = strrchr(stringBuffer, '/');
        if (lastSeparator != NULL)
            strcpy(lastSeparator + 1, siblingFilename);
        else
            strcpy(stringBuffer, siblingFilename);
        return stringBuffer;
    }
    char* GetFilename(char* out, const char* filePath) {
        out[0] = 0;
        const char* nameStart = strrchr(filePath, '/');
        const char* nameEnd = filePath + strlen(filePath);

        if (nameStart == NULL) {
            nameStart = strrchr(filePath, '\\');

            if (nameStart == NULL) {
                nameStart = filePath;
            }
            else {
                nameStart = nameStart + 1;
            }
        }
        else {
            nameStart = nameStart + 1;
        }

        strncpy(out, nameStart, nameEnd - nameStart);
        out[nameEnd - nameStart] = 0;
        return out;
    }
    char* GetFilenameWithoutExtension(char* out, const char* filePath) {
        out[0] = 0;
        const char* nameStart = strrchr(filePath, '/');
        const char* nameEnd = strrchr(filePath, '.');

        if (nameStart == NULL) {
            nameStart = strrchr(filePath, '\\');

            if (nameStart == NULL) {
                nameStart = filePath;
            }
            else
                nameStart = nameStart + 1;
        }
        else
            nameStart = nameStart + 1;

        if (nameEnd == NULL)
            nameEnd = filePath + strlen(filePath);

        strncpy(out, nameStart, nameEnd - nameStart);
        out[nameEnd - nameStart] = 0;
        return out;
    }
    char* GetExtension(char* out, const char* filePath) {
        out[0] = 0;
        const char* nameEnd = strrchr(filePath, '.');

        if (nameEnd != NULL) {
            strcpy(out, nameEnd);
        }
        return out;
    }
}

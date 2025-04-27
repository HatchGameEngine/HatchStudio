#pragma once

namespace UI::Filesystem::Paths {
    char* SanitizePath(char* path);
    char* GetEnclosingFolder(char* stringBuffer, const char* filePath);
    char* GetSiblingFilePath(char* stringBuffer, const char* filePath, const char* siblingFilename);
    char* GetFilename(char* out, const char* filePath);
    char* GetFilenameWithoutExtension(char* out, const char* filePath);
    char* GetExtension(char* out, const char* filePath);
}

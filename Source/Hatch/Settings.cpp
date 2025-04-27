#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Settings.h>

#include <Hatch/IO/FileStream.h>
#include <Hatch/Memory.h>

#define INI_IMPLEMENTATION
#include <Hatch/Libraries/ini.h>

namespace Settings {
    GraphicsSettings graphics;
    AudioSettings audio;
    DevSettings dev;

    bool ReadBoolean(CString input, bool* result) {
        if (!input || !result)
            return false;

        // Match "0", "1", "y", "n"
        if (input[1] == '\0') {
            if (input[0] == '1' ||
                input[0] == 'y' ||
                input[0] == 'Y' ||
                input[0] == 't' ||
                input[0] == 'T') {
                *result = true;
                return true;
            }
            else if (input[0] == '0' ||
                input[0] == 'n' ||
                input[0] == 'N' ||
                input[0] == 'f' ||
                input[0] == 'F') {
                *result = false;
                return true;
            }
            return false;
        }

        // Match "true" / "yes"
        if (strcmp(input, "true") == 0 || strcmp(input, "yes") == 0) {
            *result = true;
            return true;
        }
        // Match "false" / "no"
        if (strcmp(input, "false") == 0 || strcmp(input, "no") == 0) {
            *result = false;
            return true;
        }

        return false;
    }
    bool ReadInteger(CString input, int* result) {
        if (!input || !result)
            return false;

        *result = atoi(input);
        return true;
    }
    char* WriteBoolean(bool value) {
        return (char*)(value ? "Y" : "N");
    }
    char* WriteInteger(int value) {
        static char writeStringBuffer[256];
        snprintf(writeStringBuffer, 256, "%d", value);
        return writeStringBuffer;
    }

    CString settingsFilename = "usersettings.ini";

#define IO_GRAPHICS { \
    ini_BOOL(vsync); \
    ini_INTEGER(frameWidth); \
    ini_INTEGER(frameHeight); \
    ini_INTEGER(windowWidth); \
    ini_INTEGER(windowHeight); \
    ini_BOOL(fullscreen); \
    ini_BOOL(borderless); \
}
#define IO_AUDIO { \
    ini_INTEGER(sfxVolume); \
    ini_INTEGER(bgmVolume); \
}

    void Load() {
        Stream* stream = FileStream::New(settingsFilename, FileStream::READ_ACCESS);
        if (!stream) {
            printf("Couldnt open %s\n", settingsFilename);
            return;
        }
        // Diagnostics::SetError("Can't find settings.ini!");

        char* sourceText = NULL;
        size_t sourceTextLength = stream->Length();
        Memory::Alloc(&sourceText, sourceTextLength, Memory::MEMPOOL_TEMP, false);
        if (!sourceText) {
            printf("Ini too big! could not allocate space for %s\n", settingsFilename);
            stream->Close();
            return;
        }

        stream->ReadBytes(sourceText, sourceTextLength);
        stream->Close();


        ini_t* ini = ini_load(sourceText, NULL);
        sourceText = NULL;

#define ini_BOOL(name) ReadBoolean(ini_property_value(ini, section_graphics, ini_find_property(ini, section_graphics, #name, 0)), &graphics.name)
#define ini_INTEGER(name) ReadInteger(ini_property_value(ini, section_graphics, ini_find_property(ini, section_graphics, #name, 0)), &graphics.name)
        int section_graphics = ini_find_section(ini, "graphics", 0);
        if (section_graphics != INI_NOT_FOUND)
            IO_GRAPHICS;
#undef ini_BOOL
#undef ini_INTEGER

#define ini_BOOL(name) ReadBoolean(ini_property_value(ini, section_audio, ini_find_property(ini, section_audio, #name, 0)), &audio.name)
#define ini_INTEGER(name) ReadInteger(ini_property_value(ini, section_audio, ini_find_property(ini, section_audio, #name, 0)), &audio.name)
        int section_audio = ini_find_section(ini, "audio", 0);
        if (section_audio != INI_NOT_FOUND)
            IO_AUDIO;
#undef ini_BOOL
#undef ini_INTEGER

        ini_destroy(ini);
    }
    void Save() {
        ini_t* ini = ini_create(NULL);

#define ini_BOOL(name) ini_property_add(ini, section_graphics, #name, 0, WriteBoolean(graphics.name), 0)
#define ini_INTEGER(name) ini_property_add(ini, section_graphics, #name, 0, WriteInteger(graphics.name), 0)
        int section_graphics = ini_section_add(ini, "graphics", 0);
        if (section_graphics != INI_NOT_FOUND)
            IO_GRAPHICS;
#undef ini_BOOL
#undef ini_INTEGER

#define ini_BOOL(name) ini_property_add(ini, section_audio, #name, 0, WriteBoolean(audio.name), 0)
#define ini_INTEGER(name) ini_property_add(ini, section_audio, #name, 0, WriteInteger(audio.name), 0)
        int section_audio = ini_section_add(ini, "audio", 0);
        if (section_audio != INI_NOT_FOUND)
            IO_AUDIO;
#undef ini_BOOL
#undef ini_INTEGER

        int size = ini_save(ini, NULL, 0); // Find the size needed
        char* data = (char*)malloc(size);
        if (data) {
            size = ini_save(ini, data, size); // Actually save the file
            ini_destroy(ini);

            FILE* f = fopen(settingsFilename, "w");
            if (f) {
                fwrite(data, 1, size, f);
                fclose(f);
            }
            free(data);
        }
    }

    void Init() {
        // Set default settings here.
        graphics.vsync = true;
        graphics.frameWidth = 424;
        graphics.frameHeight = 240;
        graphics.windowWidth = 424;
        graphics.windowHeight = 240;
        graphics.fullscreen = true;
        graphics.borderless = false;

        audio.sfxVolume = 100;
        audio.bgmVolume = 100;

        dev.frameSkip = 8;

        Load();
    }
    void Dispose() {
        Save();
    }
}

#include <UI/System/Diagnostics.hpp>

#include <stdarg.h>
#include <stdio.h>

namespace UI::System::Diagnostics {
    FILE* log = NULL;
    char ErrorString[1024];

    void SetError(const char* text, ...) {
        va_list args;
        va_start(args, text);
        {
            vsnprintf(ErrorString, 1024, text, args);
        }
        va_end(args);
    }
    void Log(const char* text, ...) {
        va_list args;

        if (!log) {
            log = fopen("HatchStudio.log", "a");
        }
        if (log) {
            va_start(args, text);
            {
                vfprintf(log, text, args);
                fprintf(log, "\n");
            }
            va_end(args);
            fclose(log);
            log = NULL;
        }
    }
}

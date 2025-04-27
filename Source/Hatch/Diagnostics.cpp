#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Diagnostics.h>

#include <stdarg.h>

namespace Diagnostics {
    char ErrorString[1024];

    FILE* log = NULL;

    void SetError(CString text, ...) {
        va_list args;
        va_start(args, text);
        {
            vsnprintf(ErrorString, 1024, text, args);
        }
        va_end(args);

        if (!log) {
            log = fopen("HatchLite.log", "a");
        }
        if (log) {
            fprintf(log, "%s", ErrorString);
            fprintf(log, "\n");
            fclose(log);
            log = NULL;
        }
    }

    // Make the remote Diagnostics work via a UDP broadcast?
}

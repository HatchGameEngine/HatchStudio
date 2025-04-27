#pragma once

namespace UI::System::Diagnostics {
    extern char ErrorString[1024];
    extern void SetError(const char* text, ...);
    extern void Log(const char* text, ...);
}

#pragma once

namespace UI {
    namespace Clipboard {
        struct Format {
            size_t format;
        };

        extern void Clear();
        extern Format RegisterFormat(const char* identifier);

        extern bool HasText();
        extern void SetText(const char* text);
        extern const char* GetText();

        extern bool HasData(Format format);
        extern bool SetData(Format format, const unsigned char* data, size_t size);
        extern bool GetData(Format format, unsigned char* data, size_t size);
    }
}

#include <string>
#include <UI/System/Clipboard.hpp>
#include <Libraries/clip.h>

// https://github.com/dacap/clip

std::string Clipboard_BufferString;

namespace UI {
    namespace Clipboard {
        void Clear() {
            clip::lock l;
            l.clear();
        }
        Format RegisterFormat(const char* identifier) {
            return Format { clip::register_format(identifier) };
        }

        bool HasText() {
            return clip::has(clip::text_format());
        }
        void SetText(const char* text) {
            clip::set_text(text);
        }
        const char* GetText() {
            if (!clip::get_text(Clipboard_BufferString))
                return NULL;

            return Clipboard_BufferString.c_str();
        }

        bool HasData(Format format) {
            return clip::has(format.format);
        }
        bool SetData(Format format, const unsigned char* data, size_t size) {
            clip::lock l;
            return l.set_data(format.format, (const char*)data, size);
        }
        bool GetData(Format format, unsigned char* data, size_t size) {
            clip::lock l;
            return l.get_data(format.format, (char*)data, size);
        }
    }
}

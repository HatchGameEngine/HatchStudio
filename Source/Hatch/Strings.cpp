#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Strings.h>

#include <Hatch/IO/ResourceStream.h>
#include <Hatch/Memory.h>
#include <Hatch/Resources.h>

namespace Strings {
    void Init(String* string, size_t capacity) {
        string->Length = 0;
        string->Capacity = (Uint16)capacity;
        string->Encoding = UTF8;
        Memory::Alloc(&string->Text, capacity * sizeof(*string->Text), Memory::MEMPOOL_STRING, false);
    }
    void FromUnicode(String* string, Uint8 unicode) {

    }
    void FromCString(String* string, CString src, size_t length) {
        string->Length = 0;
        string->Capacity = 0;
        string->Encoding = UTF8;
        if (!src)
            return;

        if (length == 0)
            string->Length = (Uint16)strlen(src);
        else
            string->Length = length;
        string->Capacity = M_MAX(1, string->Length);

        Memory::Alloc(&string->Text, string->Capacity * sizeof(*string->Text), Memory::MEMPOOL_STRING, false);
        if (string->Text == NULL)
            return;

        for (int i = 0; i < string->Length; i++) {
            string->Text[i] = src[i];
        }
    }
    void FromResource(String* string, CString filename, Uint8 encoding) {
        PREFIX_FILENAME(filename, "Strings/");

        Stream* stream = ResourceStream::New(Resources::BufferString);
        if (stream) {
            switch (encoding) {
            case UTF8:
                Strings::Init(string, stream->Length());

                for (size_t i = 0; i < string->Length; i++)
                    string->Text[i] = stream->ReadByte();

            case UTF16:
                Strings::Init(string, stream->Length() / 2);

                for (size_t i = 0; i < string->Length; i++)
                    string->Text[i] = stream->ReadInt16();
            }

            stream->Close();
        }
        else {
            fprintf(stderr, "Couldn't open stream for %s!\n", filename);
        }
    }
    void Copy(String* dst, String* src) {
        if (!dst || !src)
            return;

        if (src->Length > dst->Capacity) {
            dst->Capacity = src->Length;
            Memory::Alloc(&dst->Text, dst->Capacity * sizeof(*dst->Text), Memory::MEMPOOL_STRING, false);
        }
        dst->Length = src->Length;

        for (int i = 0; i < src->Length; i++) {
            dst->Text[i] = src->Text[i];
        }
    }
    void Concat(String* string, String* suffix) {
        int lengthA = string->Length;
        int lengthB = suffix->Length;

        Memory::Realloc(&string->Text, (lengthA + lengthB) * sizeof(*string->Text), Memory::MEMPOOL_STRING);
        string->Capacity = lengthA + lengthB;

        for (int i = 0; i < lengthB; i++) {
            string->Text[lengthA + i] = suffix->Text[i];
        }
    }
    bool Match(String* stringA, String* stringB, bool caseSensitive) {
        const Sint16 caseConversion[256] = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x40, 0x41, 0x42, 0x43, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        };

        if (stringA->Length != stringB->Length)
            return false;

        if (caseSensitive) {
            for (int i = 0; i < stringA->Length; i++) {
                if (stringA->Text[i] != stringB->Text[i])
                    return false;
            }
        }
        else {
            for (int i = 0; i < stringA->Length; i++) {
                if (caseConversion[stringA->Text[i]] != caseConversion[stringB->Text[i]])
                    return false;
            }
        }

        return true;
    }
    void ToCString(char* str, String* string) {
        for (int i = 0; i < string->Length; i++) {
            *str++ = (char)string->Text[i];
        }
        *str = 0;
    }
}

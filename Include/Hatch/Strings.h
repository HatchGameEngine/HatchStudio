#pragma once

namespace Strings {
    void Init(String* string, size_t length);
    void FromUnicode(String* string, Uint8 unicode);
    void FromCString(String* string, CString str, size_t length);
    void FromResource(String* string, CString filename, Uint8 encoding);
    void Copy(String* dst, String* src);
    void Concat(String* string, String* suffix);
    bool Match(String* stringA, String* stringB, bool caseSensitive);
    void ToCString(char* str, String* string);
}

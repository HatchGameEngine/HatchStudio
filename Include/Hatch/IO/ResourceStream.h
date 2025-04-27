#pragma once

#include <Hatch/IO/Stream.h>

#include <Hatch/Memory.h>
#include <Hatch/IO/MemoryStream.h>

struct  XORSession {
    Hash  KeyA;
    Hash  KeyB;
    bool  SwapNibbles;
    int   IndexKeyA;
    int   IndexKeyB;
    int   XORValue;
};

class ResourceStream : public Stream {
public:
    Stream* internalStream;

    bool       encrypted;
    XORSession xorSession;

    static Stream* New(const char* filename);
           void    Close();
           void    Seek(Sint64 offset);
           void    SeekEnd(Sint64 offset);
           void    Skip(Sint64 offset);
           size_t  Position();
           size_t  Length();
           size_t  ReadBytes(void* data, size_t n);
           size_t  WriteBytes(void* data, size_t n);
};

#pragma once

#include <Hatch/IO/Stream.h>

#include <Hatch/Memory.h>

class MemoryStream : public Stream {
public:
    Sint64 data_index = 0;
    Uint8* data_start = NULL;
    size_t size;
    bool   freeOnClose;

    static MemoryStream* New(Stream* other, int pool = Memory::MEMPOOL_TEMP, bool freeOnClose = true);
    static MemoryStream* New(size_t size, int pool = Memory::MEMPOOL_TEMP, bool freeOnClose = true);
           void          Close();
           void          Seek(Sint64 offset);
           void          SeekEnd(Sint64 offset);
           void          Skip(Sint64 offset);
           size_t        Position();
           size_t        Length();
           size_t        ReadBytes(void* data, size_t n);
           size_t        WriteBytes(void* data, size_t n);
};

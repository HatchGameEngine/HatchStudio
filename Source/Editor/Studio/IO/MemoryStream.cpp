#include <Hatch/Primitives.h>

#include <Hatch/Diagnostics.h>
#include <Hatch/IO/MemoryStream.h>

#include <new>

MemoryStream* MemoryStream::New(Stream* other, int pool, bool freeOnClose) {
    if (!other)
        return NULL;

    MemoryStream* stream = new (std::nothrow) MemoryStream;
    if (!stream)
        return NULL;

    stream->size = other->Length();

    Memory::Alloc(&stream->data_start, stream->size, pool, false);
    if (!stream->data_start)
        goto FREE;

    stream->data_index = 0;
    stream->freeOnClose = freeOnClose;

    other->Seek(0);
    other->ReadBytes(stream->data_start, stream->size);
    other->Seek(0);

    return stream;

FREE:
    delete stream;
    return NULL;
}
MemoryStream* MemoryStream::New(size_t size, int pool, bool freeOnClose) {
    if (!size)
        return NULL;

    MemoryStream* stream = new MemoryStream;
    if (!stream)
        return NULL;

    stream->size = size;

    Memory::Alloc(&stream->data_start, stream->size, pool, false);
    if (!stream->data_start)
        goto FREE;

    stream->data_index = 0;
    stream->freeOnClose = freeOnClose;

    return stream;

FREE:
    delete stream;
    return NULL;
}

void          MemoryStream::Close() {
    if (freeOnClose) {
        data_start = NULL;
    }
    Stream::Close();
}
void          MemoryStream::Seek(Sint64 offset) {
    data_index = offset;
}
void          MemoryStream::SeekEnd(Sint64 offset) {
    data_index = size + offset;
}
void          MemoryStream::Skip(Sint64 offset) {
    data_index = data_index + offset;
}
size_t        MemoryStream::Position() {
    return data_index;
}
size_t        MemoryStream::Length() {
    return size;
}

size_t        MemoryStream::ReadBytes(void* data, size_t n) {
    if (n > size - Position())
        n = size - Position();
    if (n == 0) return 0;

    memcpy(data, &data_start[data_index], n);
    data_index += n;
    return n;
}
size_t        MemoryStream::WriteBytes(void* data, size_t n) {
    if (Position() + n > size) {
        Memory::Alloc(&data_start, data_index + n, Memory::MEMPOOL_TEMP, false);
    }
    memcpy(&data_start[data_index], data, n);
    data_index += n;
    return n;
}

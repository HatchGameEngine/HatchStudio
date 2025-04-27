#include <Hatch/Primitives.h>

#include <Hatch/IO/FileStream.h>

FileStream* FileStream::New(const char* filename, Uint32 access) {
    FileStream* stream = new FileStream;
    if (!stream) {
        return NULL;
    }

    const char* accessString = NULL;
    switch (access & 15) {
        case FileStream::READ_ACCESS: accessString = "rb"; break;
        case FileStream::WRITE_ACCESS: accessString = "wb"; break;
        case FileStream::APPEND_ACCESS: accessString = "ab"; break;
    }

    if (!accessString)
        goto FREE;

    stream->f = fopen(filename, accessString);
    stream->size = 0;
    stream->haveLength = false;

    if (!stream->f)
        goto FREE;

    return stream;

    FREE:
        delete stream;
        return NULL;
}

void        FileStream::Close() {
    fclose(f);
    f = NULL;
    Stream::Close();
}
void        FileStream::Seek(Sint64 offset) {
    fseek(f, offset, SEEK_SET);
}
void        FileStream::SeekEnd(Sint64 offset) {
    fseek(f, offset, SEEK_END);
}
void        FileStream::Skip(Sint64 offset) {
    fseek(f, offset, SEEK_CUR);
}
size_t      FileStream::Position() {
    return ftell(f);
}
size_t      FileStream::Length() {
    if (!haveLength) {
        size_t pos = ftell(f);
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, pos, SEEK_SET);
        haveLength = true;
    }
    return size;
}

size_t      FileStream::ReadBytes(void* data, size_t n) {
    // if (!f) Log::Print(Log::LOG_ERROR, "Attempt to read from closed stream.")
    return fread(data, 1, n, f);
}
size_t      FileStream::WriteBytes(void* data, size_t n) {
    return fwrite(data, 1, n, f);
}

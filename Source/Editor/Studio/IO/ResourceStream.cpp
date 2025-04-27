#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/IO/ResourceStream.h>

#include <Hatch/IO/FileStream.h>
#include <Hatch/Hashing/MD5.h>
#include <Hatch/Diagnostics.h>
#include <Hatch/Resources.h>

#include <Studio/Project.hpp>

using namespace Resources;

char** Studio::ResourcePathPrefix = NULL;

Stream* ResourceStream::New(const char* filename) {
    ResourceStream* stream = new ResourceStream;
    if (!stream) {
        return NULL;
    }

    // Load a FileStream with the appropriate filepath into a new MemoryStream and set internalStream to it.
    char bufferString[512]; bufferString[0] = 0;
    if (Studio::ResourcePathPrefix && *Studio::ResourcePathPrefix) {
        strcat(bufferString, *Studio::ResourcePathPrefix);
        strcat(bufferString, "/");
    }
    strcat(bufferString, RESOURCE_PREFIX);
    strcat(bufferString, filename);

    Stream* streamFile = FileStream::New(bufferString, FileStream::READ_ACCESS);
    if (streamFile) {
        stream->internalStream = MemoryStream::New(streamFile);
        streamFile->Close();

        if (!stream->internalStream) {
            Diagnostics::SetError("Could not create stream for '%s'!", filename);
            goto FREE;
        }
    }
    else {
        Diagnostics::SetError("Could not open file '%s'!", bufferString);
        goto FREE;
    }

    stream->encrypted = false;

    return stream;
    FREE:
        delete stream;
        return NULL;
}

void    ResourceStream::Close() {
    internalStream->Close();
}
void    ResourceStream::Seek(Sint64 offset) {
    internalStream->Seek(offset);
}
void    ResourceStream::SeekEnd(Sint64 offset) {
    internalStream->SeekEnd(offset);
}
void    ResourceStream::Skip(Sint64 offset) {
    internalStream->Skip(offset);
}
size_t  ResourceStream::Position() {
    return internalStream->Position();
}
size_t  ResourceStream::Length() {
    return internalStream->Length();
}

size_t  ResourceStream::ReadBytes(void* data, size_t n) {
    size_t nbytesread = internalStream->ReadBytes(data, n);
    return nbytesread;
}
size_t  ResourceStream::WriteBytes(void* data, size_t n) {
    return 0;
}

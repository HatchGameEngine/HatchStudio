#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/IO/ResourceStream.h>

#include <Hatch/IO/FileStream.h>
#include <Hatch/Hashing/MD5.h>
#include <Hatch/Diagnostics.h>
#include <Hatch/Resources.h>

#include <new>

using namespace Resources;

void    XORSession_Start(XORSession* session, Hash* keyA, Hash* keyB, Uint8 keyC) {
    session->KeyA = *keyA;
    session->KeyB = *keyB;
    session->SwapNibbles = false;
    session->IndexKeyA = 0;
    session->IndexKeyB = 8;
    session->XORValue = keyC & 0x7F;
}
void    XORSession_Body(XORSession* session, void* data, size_t nbytes, bool decrypt) {
    Uint8* keyA = (Uint8*)&session->KeyA;
    Uint8* keyB = (Uint8*)&session->KeyB;

	Uint8* body = (Uint8*)data;

    Uint8 temp;
    for (Uint32 x = 0; x < nbytes; x++) {
        temp = body[x];

        if (decrypt)
            temp ^= session->XORValue ^ keyB[session->IndexKeyB++];
        else
            temp ^= keyA[session->IndexKeyA++];

        if (session->SwapNibbles)
            temp = (((temp & 0x0F) << 4) | ((temp & 0xF0) >> 4));

        if (decrypt)
            temp ^= keyA[session->IndexKeyA++];
        else
            temp ^= session->XORValue ^ keyB[session->IndexKeyB++];

		body[x] = temp;

        if (session->IndexKeyA <= 15) {
            if (session->IndexKeyB > 12) {
                session->IndexKeyB = 0;
                session->SwapNibbles ^= 1;
            }
        }
        else if (session->IndexKeyB <= 8) {
            session->IndexKeyA = 0;
            session->SwapNibbles ^= 1;
        }
        else {
            session->XORValue = (session->XORValue + 2) & 0x7F;
            if (session->SwapNibbles) {
                session->SwapNibbles = false;
                session->IndexKeyA = session->XORValue % 7;
                session->IndexKeyB = (session->XORValue % 12) + 2;
            }
            else {
                session->SwapNibbles = true;
                session->IndexKeyA = (session->XORValue % 12) + 3;
                session->IndexKeyB = session->XORValue % 7;
            }
        }
    }
}
void    XORSession_SkipForward(XORSession* session, size_t nbytes) {
    for (Uint32 x = 0; x < nbytes; x++) {
        session->IndexKeyB++;
        session->IndexKeyA++;

        if (session->IndexKeyA <= 15) {
            if (session->IndexKeyB > 12) {
                session->IndexKeyB = 0;
                session->SwapNibbles ^= 1;
            }
        }
        else if (session->IndexKeyB <= 8) {
            session->IndexKeyA = 0;
            session->SwapNibbles ^= 1;
        }
        else {
            session->XORValue = (session->XORValue + 2) & 0x7F;
            if (session->SwapNibbles) {
                session->SwapNibbles = false;
                session->IndexKeyA = session->XORValue % 7;
                session->IndexKeyB = (session->XORValue % 12) + 2;
            }
            else {
                session->SwapNibbles = true;
                session->IndexKeyA = (session->XORValue % 12) + 3;
                session->IndexKeyB = session->XORValue % 7;
            }
        }
    }
}

Stream* ResourceStream::New(const char* filename) {
    ResourceStream* stream = new (std::nothrow) ResourceStream;
    if (!stream) {
        return NULL;
    }

    if (Resources::UseResourceFolder) {
        // Load a FileStream with the appropriate filepath into a new MemoryStream and set internalStream to it.
        char bufferString[512]; bufferString[0] = 0;
        strcat(bufferString, Resources::ResourceFolderPrefix);
        strcat(bufferString, filename);

        Stream* streamFile = FileStream::New(bufferString, FileStream::READ_ACCESS);
        if (streamFile) {
            // stream->internalStream = streamFile;
            // /*
            stream->internalStream = MemoryStream::New(streamFile);
            streamFile->Close();

            if (!stream->internalStream) {
                printf("%s\n", Diagnostics::ErrorString);
                Diagnostics::SetError("Could not create stream for '%s'!", filename);
                goto FREE;
            }
            // */
        }
        else {
            Diagnostics::SetError("Could not open file '%s'!", bufferString);
            goto FREE;
        }

        stream->encrypted = false;
    }
    else {
        // TODO:
        // Find the filename in the resource packs, using the filename from the pack with the highest priority
        // Data.hatch has a priority of 0
        // Modpack .hatch's have a user defined priority of 1 to 255 (0 will error as it conflicts with base Data.hatch)

        Hash targetFilename = MD5_HashString(filename);

        stream->encrypted = false;

        ResPackFile* selectedFile = NULL;
        int          selectedFilePriority = -1;
        for (int p = 0; p < MAX_RESOURCE_PACKS; p++) {
            Resources::ResPack* resource = &Resources::ResourcePacks[p];
            if (resource->Loaded) {
                if (resource->Priority >= selectedFilePriority) {
                    for (int i = 0; i < resource->FileCount; i++) {
                        ResPackFile* file = &resource->Files[i];
                        if (file->Filename == targetFilename) {
                            selectedFile = file;
                            selectedFilePriority = resource->Priority;
                            break;
                        }
                    }
                }
            }
        }

        if (!selectedFile) {
            Diagnostics::SetError("Could not find file \"%s\" in resource pack!", filename);
            goto FREE;
        }

        // Get whether or not this file is encrypted
        stream->encrypted = (selectedFile->DataFlag == Resources::FILEFLAG_ENCRYPTED);

        // Make a new MemoryStream and set internalStream to it.
        MemoryStream* memStream = MemoryStream::New((size_t)selectedFile->Size);

        stream->internalStream = memStream;
        if (!stream->internalStream) {
			Diagnostics::SetError("Could not create stream for '%s'!", filename);
            goto FREE;
        }

        // Get the active FileStream for the resource pack, and read into internalStream.
        selectedFile->Pack->Stream->Seek(selectedFile->Offset);
        if (selectedFile->DataFlag == Resources::FILEFLAG_COMPRESSED) {
            selectedFile->Pack->Stream->ReadCompressedRaw(memStream->data_start, (size_t)selectedFile->Size, (size_t)selectedFile->CompressedSize);
        }
        else {
            selectedFile->Pack->Stream->ReadBytes(memStream->data_start, (size_t)selectedFile->Size);
        }

        if (stream->encrypted) {
            Hash keyA = targetFilename;
            Hash keyB = MD5_HashData(&selectedFile->Size, sizeof(selectedFile->Size));
            XORSession_Start(&stream->xorSession, &keyA, &keyB, (Uint8)(selectedFile->Size / 4));
        }
    }

    return stream;
    FREE:
        delete stream;
        return NULL;
}

void    ResourceStream::Close() {
    internalStream->Close();
    Stream::Close();
}
void    ResourceStream::Seek(Sint64 offset) {
    internalStream->Seek(offset);
}
void    ResourceStream::SeekEnd(Sint64 offset) {
    internalStream->SeekEnd(offset);
}
void    ResourceStream::Skip(Sint64 offset) {
    internalStream->Skip(offset);
    if (encrypted) {
        if (offset > 0)
            XORSession_SkipForward(&xorSession, (size_t)offset);
        // else if (offset < 0)
    }
}
size_t  ResourceStream::Position() {
    return internalStream->Position();
}
size_t  ResourceStream::Length() {
    return internalStream->Length();
}

size_t  ResourceStream::ReadBytes(void* data, size_t n) {
    size_t nbytesread = internalStream->ReadBytes(data, n);
    if (encrypted)
        XORSession_Body(&xorSession, data, nbytesread, true);

    return nbytesread;
}
size_t  ResourceStream::WriteBytes(void* data, size_t n) {
    return 0;
}

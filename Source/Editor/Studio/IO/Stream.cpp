#include <Hatch/Primitives.h>

#include <Hatch/IO/Stream.h>
#include <Hatch/Memory.h>

#include <Hatch/Libraries/miniz.h>

#define READ_TYPE_MACRO(type) \
    type data; \
    ReadBytes(&data, sizeof(data));

void    Stream::Close() {
    delete this;
}
void    Stream::Seek(Sint64 offset) {
}
void    Stream::SeekEnd(Sint64 offset) {
}
void    Stream::Skip(Sint64 offset) {
}
size_t  Stream::Position() {
    return 0;
}
size_t  Stream::Length() {
    return 0;
}

size_t  Stream::ReadBytes(void* data, size_t n) {
    return 0;
}
Uint8   Stream::ReadByte() {
    READ_TYPE_MACRO(Uint8);
    return data;
}
Uint16  Stream::ReadUInt16() {
    READ_TYPE_MACRO(Uint16);
    return data;
}
Uint16  Stream::ReadUInt16SE() {
    return (Uint16)(ReadByte() << 8 | ReadByte());
}
Uint32  Stream::ReadUInt32() {
    READ_TYPE_MACRO(Uint32);
    return data;
}
Uint32  Stream::ReadUInt32SE() {
    return (Uint32)(ReadByte() << 24 | ReadByte() << 16 | ReadByte() << 8 | ReadByte());
}
Uint64  Stream::ReadUInt64() {
    READ_TYPE_MACRO(Uint64);
    return data;
}
Sint16  Stream::ReadInt16() {
    READ_TYPE_MACRO(Sint16);
    return data;
}
Sint16  Stream::ReadInt16SE() {
    return (Sint16)(ReadByte() << 8 | ReadByte());
}
Sint32  Stream::ReadInt32() {
    READ_TYPE_MACRO(Sint32);
    return data;
}
Sint32  Stream::ReadInt32SE() {
    return (Sint32)(ReadByte() << 24 | ReadByte() << 16 | ReadByte() << 8 | ReadByte());
}
Sint64  Stream::ReadInt64() {
    READ_TYPE_MACRO(Sint64);
    return data;
}
float   Stream::ReadFloat() {
    READ_TYPE_MACRO(float);
    return data;
}
char*   Stream::ReadLine() {
    Uint8 byte = 0;
    size_t start = Position();
    while ((byte = ReadByte()) != '\0')
        if (byte == '\n')
            break;

    size_t size = Position() - start;

    char* data = (char*)malloc(size + 1);
    if (data) {
        Skip(-(Sint64)size);
        ReadBytes(data, size);
        data[size] = 0;
    }

    return data;
}
char*   Stream::ReadString() {
    size_t start = Position();
    while (ReadByte());

    size_t size = Position() - start;

    char* data = (char*)malloc(size + 1);
    if (data) {
        Skip(-(Sint64)size);
        ReadBytes(data, size);
        data[size] = 0;
    }

    return data;
}
Uint16* Stream::ReadUnicodeString() {
    size_t start = Position();
    while (ReadUInt16());

    size_t size = Position() - start;

    Uint16* data = (Uint16*)malloc(size);
    Skip(-(Sint64)size);
    ReadBytes(data, size);

    return data;
}
char*   Stream::ReadHeaderedString() {
    Uint8 size = ReadByte();

    char* data = (char*)malloc(size + 1);
    if (data) {
        ReadBytes(data, size);
        data[size] = 0;
    }

    return data;
}
void    Stream::ReadHeaderedString(char* data) {
    Uint8 size = ReadByte();
    ReadBytes(data, size);
    data[size] = 0;
}
Uint32  Stream::ReadCompressed(void* out) {
    Uint32 compressed_size = ReadUInt32() - 4;
    Uint32 uncompressed_size = ReadUInt32SE();

    void* buffer = NULL;
    Memory::Alloc(&buffer, compressed_size, Memory::MEMPOOL_TEMP, false);
    ReadBytes(buffer, compressed_size);

    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;

    // input data
    infstream.next_in = (Bytef*)buffer;
    infstream.avail_in = (int)compressed_size;

    // output data
    infstream.next_out = (Bytef*)out;
    infstream.avail_out = (int)uncompressed_size;

    inflateInit(&infstream);
    inflate(&infstream, Z_NO_FLUSH);
    inflateEnd(&infstream);

    return uncompressed_size;
}
Uint32  Stream::ReadCompressed(void* out, size_t outSz) {
    Uint32 compressed_size = ReadUInt32() - 4;
    Uint32 uncompressed_size = ReadUInt32SE();

    void* buffer = NULL;
    Memory::Alloc(&buffer, compressed_size, Memory::MEMPOOL_TEMP, false);
    ReadBytes(buffer, compressed_size);

    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;

    // input data
    infstream.next_in = (Bytef*)buffer;
    infstream.avail_in = (int)compressed_size;

    // output data
    infstream.next_out = (Bytef*)out;
    infstream.avail_out = (int)outSz;

    inflateInit(&infstream);
    inflate(&infstream, Z_NO_FLUSH);
    inflateEnd(&infstream);
    
    buffer = NULL; // Mark buffer for garbage collection

    return uncompressed_size;
}
Uint32  Stream::ReadCompressedRaw(void* dst, size_t dstSz, size_t srcSz) {
    void* buffer = NULL;
    Memory::Alloc(&buffer, srcSz, Memory::MEMPOOL_TEMP, false);
    ReadBytes(buffer, srcSz);

    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;

    // input data
    infstream.next_in = (Bytef*)buffer;
    infstream.avail_in = (int)srcSz;

    // output data
    infstream.next_out = (Bytef*)dst;
    infstream.avail_out = (int)dstSz;

    inflateInit(&infstream);
    inflate(&infstream, Z_NO_FLUSH);
    inflateEnd(&infstream);

    buffer = NULL; // Mark buffer for garbage collection

    return (Uint32)dstSz;
}

size_t  Stream::WriteBytes(void* data, size_t n) {
    return 0;
}
void    Stream::WriteByte(Uint8 data) {
    WriteBytes(&data, sizeof(data));
}
void    Stream::WriteUInt16(Uint16 data) {
    WriteBytes(&data, sizeof(data));
}
void    Stream::WriteUInt16SE(Uint16 data) {
    WriteByte(data >> 8 & 0xFF);
    WriteByte(data & 0xFF);
}
void    Stream::WriteUInt32(Uint32 data) {
    WriteBytes(&data, sizeof(data));
}
void    Stream::WriteUInt32SE(Uint32 data) {
    WriteByte(data >> 24 & 0xFF);
    WriteByte(data >> 16 & 0xFF);
    WriteByte(data >> 8 & 0xFF);
    WriteByte(data & 0xFF);
}
void    Stream::WriteUInt64(Uint64 data) {
    WriteBytes(&data, sizeof(data));
}
void    Stream::WriteInt16(Sint16 data) {
    WriteBytes(&data, sizeof(data));
}
void    Stream::WriteInt16SE(Sint16 data) {
    WriteUInt16SE((Uint16)data);
}
void    Stream::WriteInt32(Sint32 data) {
    WriteBytes(&data, sizeof(data));
}
void    Stream::WriteInt32SE(Sint32 data) {
    WriteUInt32SE((Sint32)data);
}
void    Stream::WriteInt64(Sint64 data) {
    WriteBytes(&data, sizeof(data));
}
void    Stream::WriteFloat(float data) {
    WriteBytes(&data, sizeof(data));
}
void    Stream::WriteString(const char* string) {
    size_t size = strlen(string) + 1;
    WriteBytes((void*)string, size);
}
void    Stream::WriteHeaderedString(const char* string) {
    size_t size = strlen(string) + 1;
    WriteByte((Uint8)size);
    WriteBytes((void*)string, size);
}
Uint32  Stream::WriteCompressed(void* data, size_t size) {
    Uint32 compressed_size;
    Uint32 uncompressed_size = (Uint32)size;

    void* buffer = NULL;
    Memory::Alloc(&buffer, uncompressed_size, Memory::MEMPOOL_TEMP, false);

    z_stream defstream;
    defstream.zalloc = Z_NULL;
    defstream.zfree = Z_NULL;
    defstream.opaque = Z_NULL;

    // input data
    defstream.next_in = (Bytef*)data;
    defstream.avail_in = uncompressed_size;

    // output data
    defstream.next_out = (Bytef*)buffer;
    defstream.avail_out = uncompressed_size;

    int res;
    deflateInit(&defstream, Z_BEST_COMPRESSION);
    res = deflate(&defstream, Z_NO_FLUSH);
    res = deflate(&defstream, Z_FINISH);
    deflateEnd(&defstream);

    compressed_size = (Uint32)defstream.total_out;

    WriteUInt32(compressed_size + 4); // compressed_size
    WriteUInt32SE(uncompressed_size); // uncompressed_size
    WriteBytes(buffer, compressed_size);
    buffer = NULL; // Mark buffer for garbage collection

    return compressed_size;
}

void    Stream::CopyTo(Stream* dest) {
    size_t length = Length() - Position();
    void*  memory = NULL;
    Memory::Alloc(&memory, length, Memory::MEMPOOL_TEMP, false);

    // Seek(0);
    ReadBytes(memory, length);
    dest->WriteBytes(memory, length);
    // dest->Seek(0);
}

        Stream::~Stream() {
}

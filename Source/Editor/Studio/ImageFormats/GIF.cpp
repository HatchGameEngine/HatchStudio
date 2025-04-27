#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/ImageFormats/GIF.h>

#include <Hatch/Diagnostics.h>
#include <Hatch/Memory.h>

struct Node {
    Uint16 Key;
    struct Node* Children[];
};
struct Entry {
    Uint8  Used;
    Uint16 Length;
	Uint16 Prefix;
	Uint8  Suffix;
};

Entry  codeTable[0x1000];
int    codeSizeTable[] = {
    0x0000,
    0x0001,
    0x0003,
    0x0007,
    0x000F,
    0x001F,
    0x003F,
    0x007F,
    0x00FF,
    0x01FF,
    0x03FF,
    0x07FF,
    0x0FFF,
};
Uint8  blockDataIndex = 0;
Uint8  blockBuffer[0x100];
inline Uint32 ReadCode(Stream* stream, int codeSize, int* blockLength, int* bitCache, int* bitCacheLength) {
    if (*blockLength == 0) {
        *blockLength = stream->ReadByte();

        stream->ReadBytes(blockBuffer, *blockLength);
        blockDataIndex = 0;
    }

    if (*blockLength > 0) {
        while (*bitCacheLength <= codeSize) {
            (*blockLength)--;
            *bitCache |= blockBuffer[blockDataIndex] << *bitCacheLength;
            *bitCacheLength += 8;

            blockDataIndex++;


            if (*blockLength == 0) {
                *blockLength = stream->ReadByte();

                stream->ReadBytes(blockBuffer, *blockLength);
                blockDataIndex = 0;
            }
        }
    }

    Uint32 result = *bitCache & codeSizeTable[codeSize];
    *bitCache >>= codeSize;
    *bitCacheLength -= codeSize;

    return result;
}


bool   GIF_Load(Stream* stream, Image* img) {
    img->Data = NULL;
    img->Palette = NULL;

    if (!stream)
        return false;

    Uint8 magicGIF[4];
    stream->ReadBytes(magicGIF, 3);
    if (memcmp(magicGIF, "GIF", 3) != 0) {
        magicGIF[3] = 0;
        Diagnostics::SetError("Invalid GIF file! Found \"%s\", expected \"GIF\"!", magicGIF);
        goto GIF_Load_FAIL;
    }

    Uint8 magic89a[4];
    stream->ReadBytes(magic89a, 3);
    if (memcmp(magic89a, "89a", 3) != 0 &&
        memcmp(magic89a, "87a", 3) != 0) {
        magic89a[3] = 0;
        Diagnostics::SetError("Invalid GIF version! Found \"%s\", expected \"89a\"!", magic89a);
        goto GIF_Load_FAIL;
    }

    Uint16 width, height, paletteTableSize;
    int bitsWidth, eighthHeight, quarterHeight, halfHeight;
    Uint8 logicalScreenDesc; // , colorBitDepth, transparentColorIndex;

    width = stream->ReadUInt16();
    height = stream->ReadUInt16();

    logicalScreenDesc = stream->ReadByte();

    stream->Skip(1); // transparentColorIndex = stream->ReadByte();
    stream->Skip(1); // pixelAspectRatio = stream->ReadByte();

    if ((logicalScreenDesc & 0x80) == 0) {
        Diagnostics::SetError("GIF missing palette table!");
        goto GIF_Load_FAIL;
    }

    img->Width = width;
    img->Height = height;

    // colorBitDepth = ((logicalScreenDesc & 0x70) >> 4) + 1; // normally 7, sometimes it is 4 (wrong)
    //sortFlag = (logicalScreenDesc & 0x8) != 0; // This is unneeded.
    paletteTableSize = 2 << (logicalScreenDesc & 0x7);

    // Prepare image data
    if (!Memory::Alloc((void**)&img->Data, width * height * sizeof(Uint8), Memory::MEMPOOL_STAGE, true)) {
        Diagnostics::SetError("Could not allocate memory for image pixel data!");
        goto GIF_Load_FAIL;
    }
    // Load Palette Table
    if (!Memory::Alloc((void**)&img->Palette, 0x100 * sizeof(Pixel), Memory::MEMPOOL_STAGE, true)) {
        Diagnostics::SetError("Could not allocate memory for image palette table!");
        goto GIF_Load_FAIL;
    }

    for (int p = 0; p < paletteTableSize; p++) {
        Color color;
        color.R = stream->ReadByte();
        color.G = stream->ReadByte();
        color.B = stream->ReadByte();
        img->Palette[p] = color;
    }

    width--;
    height--;

    eighthHeight = img->Height >> 3;
    quarterHeight = img->Height >> 2;
    halfHeight = img->Height >> 1;

    // Get frame
    Uint8 type, subtype, temp;
    type = stream->ReadByte();
    while (type) {
        bool tableFull, interlaced;
        int codeSize, initCodeSize;
        int clearCode, eoiCode, emptyCode;
        int blockLength, bitCache, bitCacheLength;
        int mark, str_len = 0, frm_off = 0;
        int currentCode;

        switch (type) {
            // Extension
            case 0x21:
                subtype = stream->ReadByte();
                switch (subtype) {
                    // Graphics Control Extension
                    case 0xF9:
                        // stream->Skip(0x06);
                        // temp = stream->ReadByte();  // Block Size [byte] (always 0x04)
                        // temp = stream->ReadByte();  // Packed Field [byte] //
                        // temp16 = stream->ReadUInt16(); // Delay Time [short] //
                        stream->Skip(4);
                        stream->Skip(1); // transparentColorIndex = stream->ReadByte();
                        stream->Skip(1);
                        // temp = stream->ReadByte();  // Block Terminator [byte] //
                        break;
                    // Plain Text Extension
                    case 0x01:
                    // Comment Extension
                    case 0xFE:
                    // Application Extension
                    case 0xFF:
                        temp = stream->ReadByte(); // Block Size
                        // Continue until we run out of blocks
                        while (temp) {
                            // Read block
                            stream->Skip(temp);
                            temp = stream->ReadByte(); // next block Size
                        }
                        break;
                    default:
                        printf("Unsupported GIF control extension '%02X'!", subtype);
                        goto GIF_Load_FAIL;
                }
                break;
            // Image descriptor
            case 0x2C:
                stream->Skip(8);
                // temp16 = stream->ReadUInt16(); // Destination X
                // temp16 = stream->ReadUInt16(); // Destination Y
                // temp16 = stream->ReadUInt16(); // Destination Width
                // temp16 = stream->ReadUInt16(); // Destination Height
                temp = stream->ReadByte();    // Packed Field [byte]

                // If a local color table exists,
                if (temp & 0x80) {
                    int size = 2 << (temp & 0x07);
                    // Load all colors
                    stream->Skip(size * 3);
                }

                interlaced = (temp & 0x40) == 0x40;
                if (interlaced) {
                    if ((width & (width - 1)) != 0) {
                        printf("Interlaced GIF width must be power of two!");
                        goto GIF_Load_FAIL;
                    }
                    if ((height & (height - 1)) != 0) {
                        printf("Interlaced GIF width must be power of two!");
                        goto GIF_Load_FAIL;
                    }

                    bitsWidth = 0;
                    while (width) {
                        width >>= 1;
                        bitsWidth++;
                    }
                    width = img->Width - 1;
                }
                else {
                    bitsWidth = 0;
                }

                codeSize = stream->ReadByte();

                clearCode = 1 << codeSize;
                eoiCode = clearCode + 1;
                emptyCode = eoiCode + 1;

                codeSize++;
                initCodeSize = codeSize;

                // Init table
                for (int i = 0; i <= eoiCode; i++) {
                    codeTable[i].Length = 1;
                    codeTable[i].Prefix = 0xFFF;
                    codeTable[i].Suffix = (Uint8)i;
                }

                blockLength = 0;
                bitCache = 0;
                bitCacheLength = 0;
                tableFull = false;

                currentCode = ReadCode(stream, codeSize, &blockLength, &bitCache, &bitCacheLength);

                codeSize = initCodeSize;
                emptyCode = eoiCode + 1;
                tableFull = false;

                Entry entry;
                entry.Suffix = 0;

                while (blockLength) {
                    mark = 0;

                    if (currentCode == clearCode) {
                        codeSize = initCodeSize;
                        emptyCode = eoiCode + 1;
                        tableFull = false;
                    }
                    else if (!tableFull) {
                        codeTable[emptyCode].Length = str_len + 1;
                        codeTable[emptyCode].Prefix = currentCode;
                        codeTable[emptyCode].Suffix = entry.Suffix;
                        emptyCode++;

                        // Once we reach highest code, increase code size
                        if ((emptyCode & (emptyCode - 1)) == 0)
                            mark = 1;
                        else
                            mark = 0;

                        if (emptyCode >= 0x1000) {
                            mark = 0;
                            tableFull = true;
                        }
                    }

                    currentCode = ReadCode(stream, codeSize, &blockLength, &bitCache, &bitCacheLength);

                    if (currentCode == clearCode) continue;
                    if (currentCode == eoiCode) goto GIF_Load_Success;
                    if (mark == 1) codeSize++;

                    entry = codeTable[currentCode];
                    str_len = entry.Length;

                    while (true) {
            			int p = frm_off + entry.Length - 1;
                        if (interlaced) {
                            int row = p >> bitsWidth;
                            if (row < eighthHeight)
                                p = (p & width) + ((((row) << 3) + 0) << bitsWidth);
                            else if (row < quarterHeight)
                                p = (p & width) + ((((row - eighthHeight) << 3) + 4) << bitsWidth);
                            else if (row < halfHeight)
                                p = (p & width) + ((((row - quarterHeight) << 2) + 2) << bitsWidth);
                            else
                                p = (p & width) + ((((row - halfHeight) << 1) + 1) << bitsWidth);
                        }

            			img->Data[p] = entry.Suffix;

            			if (entry.Prefix != 0xFFF)
            				entry = codeTable[entry.Prefix];
            			else
            				break;
            		}
            		frm_off += str_len;
            		if (currentCode < emptyCode - 1 && !tableFull)
            			codeTable[emptyCode - 1].Suffix = entry.Suffix;
                }
                break;
        }

        type = stream->ReadByte();

        if (type == 0x3B) break;
    }

    goto GIF_Load_Success;

    GIF_Load_FAIL:
        if (img->Data)
            free(img->Data);
        if (img->Palette)
            free(img->Palette);
        return false;

    GIF_Load_Success:
        return true;
}

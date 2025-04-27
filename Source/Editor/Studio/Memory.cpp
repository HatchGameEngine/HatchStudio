#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Memory.h>

#include <Hatch/Diagnostics.h>

namespace Memory {
    MemoryPool MemoryPools[MEMPOOL_COUNT];

    bool  Init() {
        ZERO_OUT(MemoryPools);

        MemoryPools[MEMPOOL_STAGE].BlocksDataSize  = 0xA00000;
        MemoryPools[MEMPOOL_MUSIC].BlocksDataSize = 0x8; // 00000;
        MemoryPools[MEMPOOL_SOUND].BlocksDataSize = 0x2; // 00000;
        MemoryPools[MEMPOOL_STRING].BlocksDataSize = 0x4000 * 0x100; // 00;
        MemoryPools[MEMPOOL_TEMP].BlocksDataSize   = 0x800000;
        MemoryPools[MEMPOOL_VIEWS].BlocksDataSize = 0x8; // 00000;

        auto memPool = &MemoryPools[0];
        for (int i = 0; i < MEMPOOL_COUNT; i++) {
            memPool->Blocks = (Uint32*)malloc(memPool->BlocksDataSize);
            if (!memPool->Blocks) {
                Diagnostics::SetError("Pool %d could not be allocated.", i);
                return false;
            }

            memPool->BlockCount = 0;
            memPool->ReferenceCount = 0;
            memPool->GC_CallCount = 0;
            memPool++;
        }
        return true;
    }

    void* Alloc(void** mem, size_t size, int pool, bool clearMem) {
        if (!mem)
            return NULL;

        size = (size & 0xFFFFFFFCU) + 4;

        Uint32 blockHeaderSize = 3 * sizeof(Uint32);

        MemoryPool* memPool = &MemoryPools[pool];
        if (memPool->BlocksDataSize == 0) {
            return NULL;
        }

        if (memPool->ReferenceCount < 0x1000) {
            // Is there enough space in the pool for this allocation?
            Uint32 blockDataCurrentSize = memPool->BlockCount * sizeof(Uint32);
            if (size + blockHeaderSize + blockDataCurrentSize < memPool->BlocksDataSize) {
                // BlockStruct: isUsed
                memPool->Blocks[memPool->BlockCount++] = true;
                // BlockStruct: dataStartIndex
                //     Set index to the block after the next one.
                memPool->Blocks[memPool->BlockCount] = memPool->BlockCount + 2;
                memPool->BlockCount++;
                // BlockStruct: dataSize
                memPool->Blocks[memPool->BlockCount++] = (Uint32)size;
                // BlockStruct: data
                *mem = (void*)&memPool->Blocks[memPool->BlockCount];
                memPool->BlockCount += size / sizeof(Uint32);

                // Add reference
                memPool->ReferenceList[memPool->ReferenceCount] = mem;
                memPool->PointerList[memPool->ReferenceCount] = *mem;
                memPool->ReferenceCount++;
            }
            else {
                // Otherwise, run the GC to attempt to clear up space.
                RunGC(pool);

                // Update the current pool size
                blockDataCurrentSize = memPool->BlockCount * sizeof(Uint32);

                // Is there enough space in the pool for this allocation?
                if (size + blockHeaderSize + blockDataCurrentSize < memPool->BlocksDataSize) {
                    // BlockStruct: isUsed
                    memPool->Blocks[memPool->BlockCount++] = true;
                    // BlockStruct: dataStartIndex
                    //     Set index to the block after the next one.
                    memPool->Blocks[memPool->BlockCount] = memPool->BlockCount + 2;
                    memPool->BlockCount++;
                    // BlockStruct: dataSize
                    memPool->Blocks[memPool->BlockCount++] = (Uint32)size;
                    // BlockStruct: data
                    *mem = (void*)&memPool->Blocks[memPool->BlockCount];
                    memPool->BlockCount += size / sizeof(Uint32);

                    // Add reference
                    memPool->ReferenceList[memPool->ReferenceCount] = mem;
                    memPool->PointerList[memPool->ReferenceCount] = *mem;
                    memPool->ReferenceCount++;
                }
                else {
                    Diagnostics::SetError("Not enough space in pool %d for 0x%X + 0x%X bytes (0x%X total, 0x%X remaining)", pool, size, blockHeaderSize, memPool->BlocksDataSize, memPool->BlocksDataSize - blockDataCurrentSize);
                    *mem = NULL;
                }
            }

            if (memPool->ReferenceCount >= 0x1000)
                CleanupReferences(pool);

            if (clearMem && *mem)
                memset(*mem, 0, size);
        }

        return *mem;
    }
    void* Realloc(void** mem, size_t size, int pool) {
        struct BlockHeader {
            Uint32 isUsed;
            Uint32 dataStartIndex;
            Uint32 dataSize;
            // Data would be here
            // Uint32 data[];
        };
        BlockHeader* header = (BlockHeader*)((Uint8*)*mem - sizeof(BlockHeader));
        Uint32 oldDataSize = header->dataSize;

        void* newData = NULL;
        Alloc(&newData, size, pool, false);
        if (!newData)
            return NULL;

        uint16_t* oldD = (uint16_t*)*mem;
        uint16_t* newD = (uint16_t*)newData;

        memcpy(newData, *mem, oldDataSize);

        PassReference(mem, &newData, pool);
        return *mem;
    }

    void  ClearPool(int pool) {
        auto memPool = &MemoryPools[pool];
        memPool->BlockCount = 0;
        memPool->ReferenceCount = 0;
        for (int v = 0; v < 0x1000; v++) {
            memPool->ReferenceList[v] = NULL;
            memPool->PointerList[v] = NULL;
        }
    }
    void  RunGC(int pool) {
        CleanupReferences(pool);

        MemoryPool* memPool = &MemoryPools[pool];
        memPool->GC_CallCount++;

        struct BlockHeader {
            Uint32 isUsed;
            Uint32 dataStartIndex;
            Uint32 dataSize;
            // Data would be here
            // Uint32 data[];
        };

        Uint32 freeBlockIndex = 0;
        Uint32 freedBlocks = 0;
        for (Uint32 i = 0; i < memPool->BlockCount;) {
            Uint32* block = &memPool->Blocks[i];
            BlockHeader* header = (BlockHeader*)block;

            int blockSizeInBytes = header->dataSize + 3 * sizeof(Uint32); // 3 = sizeof(BlockHeader) / sizeof(Uint32)
            int blockSize = blockSizeInBytes / sizeof(Uint32);
            int nextBlockIndex = i + blockSize;

            // Set block to unused until proven used
            header->isUsed = false;

            // Shortcut: If there are no references, then block logically cannot
            //   be used. Therefore, no change is needed, continue to next block
            if (!memPool->ReferenceCount) {
                freedBlocks += blockSize;
                goto NextBlock;
            }

            // Check if block is used
            for (Uint32 r = 0; r < memPool->ReferenceCount; r++) {
                if (&header[1] == memPool->PointerList[r]) {
                    header->isUsed = true;
                    break;
                }
            }

            if (!header->isUsed) {
                freedBlocks += blockSize;
                goto NextBlock;
            }
            else {
                // If the current block is ahead of the free block, move current block back to free block
                if (i > freeBlockIndex) {
                    memcpy(&memPool->Blocks[freeBlockIndex], &memPool->Blocks[i], blockSizeInBytes);
                }

                // Move free block index to next block since this one isn't free
                freeBlockIndex += blockSize;
            }

        NextBlock:
            // Next block
            i = nextBlockIndex;
        }

        if (pool == MEMPOOL_STRING)
            printf("Freed 0x%X blocks out of 0x%X possible (0x%X max)\n", freedBlocks, memPool->BlockCount, (int)(memPool->BlocksDataSize / sizeof(Uint32)));

        if (freedBlocks) {
            memPool->BlockCount -= freedBlocks;
            if (memPool->BlockCount) {
                for (Uint32 i = 0; i < memPool->BlockCount; ) {
                    Uint32* block = &memPool->Blocks[i];
                    BlockHeader* header = (BlockHeader*)block;

                    int blockSizeInBytes = header->dataSize + 3 * sizeof(Uint32); // 3 = sizeof(BlockHeader) / sizeof(Uint32)
                    int blockSize = blockSizeInBytes / sizeof(Uint32);
                    int nextBlockIndex = i + blockSize;

                    // For any pointers that point to this allocblock's data
                    void* oldAllocBlockDataPtr = (void*)&memPool->Blocks[header->dataStartIndex];
                    void* newAllocBlockDataPtr = (void*)&header[1];
                    for (Uint32 r = 0; r < memPool->ReferenceCount; r++) {
                        if (oldAllocBlockDataPtr == memPool->PointerList[r]) {
                            *memPool->ReferenceList[r] =
                            memPool->PointerList[r] = newAllocBlockDataPtr;
                        }
                    }

                    header->dataStartIndex = i + 3; // 3 = sizeof(BlockHeader) / sizeof(Uint32)

                    // Next block
                    i = nextBlockIndex;
                }
            }
        }
    }
    void  CleanupReferences(int pool) {
        MemoryPool* memPool = &MemoryPools[pool];

        // Clear stale references
        for (Uint32 r = 0; r < memPool->ReferenceCount; r++) {
            void** ref = memPool->ReferenceList[r];
            void*  ptr = memPool->PointerList[r];
            if (ref && *ref != ptr) {
                memPool->ReferenceList[r] = 0;
            }
        }

        // Move all valid references to top of list
        Uint32 v = 0;
        // v: Valid Reference Destination Index
        // r: Reference Index (could be either stale or not)
        for (Uint32 r = 0; r < memPool->ReferenceCount; r++) {
            void** ref = memPool->ReferenceList[r];
            void*  ptr = memPool->PointerList[r];
            if (ref) {
                if (r > v) {
                    memPool->ReferenceList[v] = ref;
                    memPool->PointerList[v] = ptr;
                    // Clear the old spot
                    memPool->ReferenceList[r] = 0;
                    memPool->PointerList[r] = 0;
                }
                v++;
            }
        }
        // Set new reference count
        memPool->ReferenceCount = v;

        // Cleanup references outside of bounds
        for (; v < 0x1000; v++) {
            memPool->ReferenceList[v] = NULL;
            memPool->PointerList[v] = NULL;
        }
    }
    void  Dispose() {
        auto memPool = &MemoryPools[0];
        for (int i = 0; i < MEMPOOL_COUNT; i++) {
            if (memPool->Blocks)
                free(memPool->Blocks);

            memPool->BlockCount = 0;
            memPool->ReferenceCount = 0;
            memPool->GC_CallCount = 0;
            memPool++;
        }
    }

    void  PassReference(void** dstReference, void** srcReference, int pool) {
        const int MAX_REFERENCE_COUNT = 0x1000;

        if (srcReference) {
            *dstReference = *srcReference;

            auto mPool = &MemoryPools[pool];
            if (mPool->ReferenceCount < MAX_REFERENCE_COUNT) {
                mPool->ReferenceList[mPool->ReferenceCount] = dstReference;
                mPool->PointerList[mPool->ReferenceCount] = *dstReference;
                mPool->ReferenceCount++;
                if (mPool->ReferenceCount >= MAX_REFERENCE_COUNT)
                    CleanupReferences(pool);
            }
        }
    }
}

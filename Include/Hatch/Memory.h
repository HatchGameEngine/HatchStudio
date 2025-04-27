#pragma once

namespace Memory {
    struct MemoryPool {
        Uint32* Blocks;
        Uint32  BlockCount;
        Uint32  BlocksDataSize;
        void**  ReferenceList[0x1000];
        void*   PointerList[0x1000];
        Uint32  ReferenceCount;
        Uint32  GC_CallCount;
    };
    enum {
        MEMPOOL_STAGE,
        MEMPOOL_MUSIC,
        MEMPOOL_SOUND,
        MEMPOOL_STRING,
        MEMPOOL_TEMP,
        MEMPOOL_VIEWS,

        MEMPOOL_COUNT,
    };

    extern MemoryPool MemoryPools[MEMPOOL_COUNT];

    bool  Init();

    void* Alloc(void** mem, size_t size, int pool, bool clearMem);
    void* Realloc(void** mem, size_t size, int pool);
    void  ClearPool(int pool);
    void  RunGC(int pool);
    void  CleanupReferences(int pool);
    void  Dispose();
    void  PassReference(void** newReference, void** oldReference, int pool);

    template <typename M>
    M*    Alloc(M** mem, size_t size, int pool, bool clearMem) {
        return (M*)Alloc((void**)mem, size, pool, clearMem);
    }
    template <typename M>
    M* Realloc(M** mem, size_t size, int pool) {
        return (M*)Realloc((void**)mem, size, pool);
    }
}

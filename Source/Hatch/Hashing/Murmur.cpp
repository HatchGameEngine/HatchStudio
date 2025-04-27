#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Hashing/Murmur.h>

Hash Murmur_HashData(const void* data, size_t size, Hash base) {
    const int r = 24;
    const Uint32 m = 0x5BD1E995;
	const Uint8* dataPtr = (const Uint8*)data;

	Uint32 h = base.A ^ (Uint32)size;
	while (size >= 4) {
		Uint32 k = *(Uint32*)dataPtr;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		dataPtr += 4;
		size -= 4;
	}

	// Handle the last few bytes of the input array
	switch (size) {
    	case 3: h ^= dataPtr[2] << 16;
    	case 2: h ^= dataPtr[1] << 8;
    	case 1: h ^= dataPtr[0];
    	        h *= m;
	}

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return Hash { h, 0, 0, 0 };
}
Hash Murmur_HashString(CString text, Hash base) {
    size_t length = strlen(text);
    return Murmur_HashData(text, length, base);
}

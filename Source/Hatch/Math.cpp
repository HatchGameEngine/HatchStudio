#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Math.h>

#include <math.h>
#define PI 3.1415926535f

namespace Math {
    Sint32 SinTbl_0x100[0x100];
    Sint32 CosTbl_0x100[0x100];
    Sint32 TanTbl_0x100[0x100];
    Sint32 ASinTbl_0x100[0x100];
    Sint32 ACosTbl_0x100[0x100];
    Sint32 SinTbl_0x200[0x200];
    Sint32 CosTbl_0x200[0x200];
    Sint32 TanTbl_0x200[0x200];
    Sint32 ASinTbl_0x200[0x200];
    Sint32 ACosTbl_0x200[0x200];
    Sint32 SinTbl_0x400[0x400];
    Sint32 CosTbl_0x400[0x400];
    Sint32 TanTbl_0x400[0x400];
    Sint32 ASinTbl_0x400[0x400];
    Sint32 ACosTbl_0x400[0x400];

    Uint8  ATanTbl_0x100[0x100][0x100];

    Sint32 RandomSeed = 0;

    void SetupMathTables() {
        RandomSeed = (int)rand();

        for (int i = 0; i < 0x400; i++) {
            SinTbl_0x400[i] = (int)round(sinf((float)i * PI / 512.f) * 1024.f);
            CosTbl_0x400[i] = (int)round(cosf((float)i * PI / 512.f) * 1024.f);
            TanTbl_0x400[i] = (int)round(tanf((float)i * PI / 512.f) * 1024.f);
            ASinTbl_0x400[i] = (int)((asinf((float)i / 1023.f) * 512.f) / PI);
            ACosTbl_0x400[i] = (int)((acosf((float)i / 1023.f) * 512.f) / PI);
        }
        SinTbl_0x400[0] = 0;
        CosTbl_0x400[0] = 0x400;
        SinTbl_0x400[256] = 0x400;
        CosTbl_0x400[256] = 0;
        SinTbl_0x400[512] = 0;
        CosTbl_0x400[512] = -0x400;
        SinTbl_0x400[768] = -0x400;
        CosTbl_0x400[768] = 0;

        for (int i = 0; i < 0x200; i++) {
            SinTbl_0x200[i] = SinTbl_0x400[i << 1] >> 1;
            CosTbl_0x200[i] = CosTbl_0x400[i << 1] >> 1;
            TanTbl_0x200[i] = TanTbl_0x400[i << 1] >> 1;
            ASinTbl_0x200[i] = ASinTbl_0x400[i << 1] >> 1;
            ACosTbl_0x200[i] = ACosTbl_0x400[i << 1] >> 1;
        }
        for (int i = 0; i < 0x100; i++) {
            SinTbl_0x100[i] = SinTbl_0x200[i << 1] >> 1;
            CosTbl_0x100[i] = CosTbl_0x200[i << 1] >> 1;
            TanTbl_0x100[i] = TanTbl_0x200[i << 1] >> 1;
            ASinTbl_0x100[i] = ASinTbl_0x200[i << 1] >> 1;
            ACosTbl_0x100[i] = ACosTbl_0x200[i << 1] >> 1;
        }

        for (int y = 0; y < 256; y++) {
            for (int x = 0; x < 256; x++) {
                ATanTbl_0x100[x][y] = (Uint8)(atan2f((float)y, (float)x) * 128.f / PI);
            }
        }
    }

    int ATan(int x, int y) {
        int x_abs = M_ABS(x);
        int y_abs = M_ABS(y);
        if (x_abs > y_abs) {
            while (x_abs >= 0x100) {
                x_abs >>= 4;
                y_abs >>= 4;
            }
        }
        else {
            while (y_abs >= 0x100) {
                x_abs >>= 4;
                y_abs >>= 4;
            }
        }

        int n = Math::ATanTbl_0x100[x_abs][y_abs] & 0xFF;
        if (x <= 0) {
            if (y <= 0)
                n = (n + 0x80) & 0xFF;
            else
                n = (0x100 + (0x80 - n)) & 0xFF;
        }
        else if (y <= 0) {
            n = (0x100 - n) & 0xFF;
        }
        return n;
    }
    int Sqrt(Uint32 x) {
        unsigned int v1; // esi
        unsigned int v2; // eax
        unsigned int v3; // edx
        unsigned int result; // eax

        v1 = x;
        v2 = 0x40000000;
        v3 = 0;
        if (x >= 0x40000000)
            goto LABEL_11;

        do
            v2 >>= 2;
        while (v2 > x);

        if (v2) {
            LABEL_11:
            do {
                if (v1 >= v2 + v3) {
                    v1 -= v2 + v3;
                    v3 += 2 * v2;
                }
                v2 >>= 2;
                v3 >>= 1;
            }
            while (v2);
        }

        result = v3 + 1;
        if (v1 <= v3)
            result = v3;
        return result;
    }

    int ToNextPOT(int n) {
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 16;
        n++;
        return n;
    }
    int CountEmptyBits(int n) {
        int c = 0;
        for (Uint32 b = ((Uint32)n) >> 1; b > 0; b >>= 1)
            c++;
        return c;
    }

    int RandomRangeSeeded(int min, int max, int* seed) {
        int r1 = 0x41C64E6D * *seed + 12345;
        int r2 = 0x41C64E6D * r1 + 12345;
        int r3 = 0x41C64E6D * r2 + 12345;
        
        *seed = r3;

        int r4 = ((((((r1 >> 16) & 0x7FF) << 10) ^ ((r2 >> 16) & 0x7FF)) << 10) ^ ((r3 >> 16) & 0x7FF));
        if (min >= max)
            return max + (r4 % (min - max));
        
        return min + (r4 % (max - min));
    }
    int RandomRange(int min, int max) {
        return RandomRangeSeeded(min, max, &RandomSeed);
    }
    void RandomSetSeed(int seed) {
        RandomSeed = seed;
    }
}

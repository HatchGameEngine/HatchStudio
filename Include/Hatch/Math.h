#pragma once

namespace Math {
    extern Sint32 SinTbl_0x100[0x100];
    extern Sint32 CosTbl_0x100[0x100];
    extern Sint32 TanTbl_0x100[0x100];
    extern Sint32 ASinTbl_0x100[0x100];
    extern Sint32 ACosTbl_0x100[0x100];
    extern Sint32 SinTbl_0x200[0x200];
    extern Sint32 CosTbl_0x200[0x200];
    extern Sint32 TanTbl_0x200[0x200];
    extern Sint32 ASinTbl_0x200[0x200];
    extern Sint32 ACosTbl_0x200[0x200];
    extern Sint32 SinTbl_0x400[0x400];
    extern Sint32 CosTbl_0x400[0x400];
    extern Sint32 TanTbl_0x400[0x400];
    extern Sint32 ASinTbl_0x400[0x400];
    extern Sint32 ACosTbl_0x400[0x400];

    extern Uint8  ATanTbl_0x100[0x100][0x100];

    extern Sint32 RandomSeed;

    void SetupMathTables();

    int ATan(int x, int y);
    int Sqrt(Uint32 n);
    int ToNextPOT(int n);
    int CountEmptyBits(int n);
    int RandomRangeSeeded(int min, int max, int* seed);
    int RandomRange(int min, int max);
    void RandomSetSeed(int seed);
}

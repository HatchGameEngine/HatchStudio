#pragma once

namespace GameLinker {
    extern HatchFunctionSet HatchFuncs;
    extern ServicesFunctionSet ServiceFuncs;

    extern Class ClassList[MAX_CLASSES];
    extern int   ClassCount;

    extern void* GameLogicSharedObject;

    void Init();
    void Load();
}

#include <Hatch/GameLogic/Engine.h>

namespace Engine {
    ::HatchFunctionSet HatchFuncs;
    ::ServicesFunctionSet ServiceFuncs;
    ::Entity** CurrentEntityPtr;
    ::GameState* GameState;

    Globals* gVars;
    size_t   gVarsSize;

    CString ClassNameList[MAX_CLASSES];
    bool  BufferedIsGlobalClassList[MAX_CLASSES];
    Class BufferedClassList[MAX_CLASSES];
    int   BufferedClassCount = 0;

    Globals* RegisterGlobals(size_t size) {
        gVarsSize = size;
        return NULL;
    }
}

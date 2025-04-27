#include <Hatch/GameLogic/Engine.h>
#include <Hatch/GameLogic/GameLib.h>

void* DefaultClassStaticObject;

void GameLib::LinkGameLogic(LinkData* linkData) {
    memcpy(&Engine::HatchFuncs, linkData->HatchFuncs, sizeof(Engine::HatchFuncs));
    memcpy(&Engine::ServiceFuncs, linkData->ServiceFuncs, sizeof(Engine::ServiceFuncs));
    Engine::CurrentEntityPtr = linkData->CurrentEntityPtr;
    Engine::GameState = linkData->GameStatePtr;

    HATCH_AllocateGlobals(&Engine::gVars, Engine::gVarsSize);

    // Add default class
    HATCH_ClassAdd(":DefaultClass:", &DefaultClassStaticObject, sizeof(EntitySlot), sizeof(StaticObject), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    // Add classes
    for (int i = 0; i < Engine::BufferedClassCount; i++) {
        Class* objectClass = &Engine::BufferedClassList[i];
        bool isGlobal = Engine::BufferedIsGlobalClassList[i];
        if (!isGlobal) {
            HATCH_ClassAdd(
                Engine::ClassNameList[i], objectClass->StaticObjectPtr,
                objectClass->EntitySize, objectClass->StaticObjectSize,
                objectClass->onStageLoad, objectClass->onEditorLoad,
                objectClass->onStaticUpdate,
                objectClass->onCreate, objectClass->onUpdate, objectClass->onUpdateLate,
                objectClass->onStageDraw, objectClass->onEditorDraw,
                objectClass->onSetup, objectClass->onStaticConstructor);
        }
        else {
            HATCH_ClassCreateGlobalClass(
                Engine::ClassNameList[i], objectClass->StaticObjectPtr,
                objectClass->StaticObjectSize, objectClass->onStaticConstructor);
        }
    }

    // Clear classes
    Engine::BufferedClassCount = 0;
}

#ifdef USE_DYNAMIC_GAMELOGIC_LINK
    #ifdef _USRDLL
        #if defined(_MSC_VER)
            // Microsoft
            #define EXPORT __declspec(dllexport)
            #define IMPORT __declspec(dllimport)
        #elif defined(__GNUC__)
            // GCC
            #define EXPORT __attribute__((visibility("default")))
            #define IMPORT
        #else
            #define EXPORT
            #define IMPORT
            #pragma warning Unknown dynamic link import/export semantics.
        #endif

        extern "C" {
            EXPORT void LinkGameLogic(LinkData* linkData) {
                GameLib::LinkGameLogic(linkData);
            }
        }

        #if defined(_MSC_VER)
            #define WIN32_LEAN_AND_MEAN
            #include <windows.h>

            BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
                switch (ul_reason_for_call) {
                case DLL_PROCESS_ATTACH:
                case DLL_THREAD_ATTACH:
                case DLL_THREAD_DETACH:
                case DLL_PROCESS_DETACH:
                    break;
                }
                return TRUE;
            }
        #endif
    #endif
#endif

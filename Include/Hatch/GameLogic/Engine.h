#pragma once

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>
#include <Hatch/GameLogic/LibMacros.h>
#include <new>

#define DEFINE_STATIC static StaticObject* sVars;
#define REGISTER_CLASS(objectClass) objectClass::StaticObject* objectClass::sVars = (objectClass::StaticObject*)Engine::RegisterClass<objectClass, objectClass::StaticObject>(#objectClass, &objectClass::sVars, false)
#define REGISTER_GLOBAL_CLASS(objectClass) objectClass::StaticObject* objectClass::sVars = (objectClass::StaticObject*)Engine::RegisterClass<objectClass, objectClass::StaticObject>(#objectClass, &objectClass::sVars, true)
#define $(objectClass) (objectClass::sVars)
#define SETUP_ATTRIBUTE(objectClass, type, name) Engine::HatchFuncs.Class.SetupAttribute(type, #name, offsetof(objectClass, name))
#define SETUP_ATTRIBUTE_ALIAS(objectClass, type, name, alias) Engine::HatchFuncs.Class.SetupAttribute(type, alias, offsetof(objectClass, name))

#define GLOBAL ((GameGlobals*)Engine::gVars)
#define REGISTER_GLOBALS() Globals* unused = Engine::RegisterGlobals(sizeof(GameGlobals))

#define GAMESTATE (Engine::GameState)

#define FOR_ALL(objectClass, entity) while (Engine::HatchFuncs.Class.SearchEntity($(objectClass)->StageClassID, (Entity**)&entity))
#define FOR_ALL_INTERACTABLE(objectClass, entity) while (Engine::HatchFuncs.Class.SearchInteractableEntity($(objectClass)->StageClassID, (Entity**)&entity))
#define FOR_ALL_BREAK { Engine::HatchFuncs.Class.EndSearch(); break; }
#define FOR_ALL_CONTINUE { continue; }
#define FOR_ALL_RETURN { Engine::HatchFuncs.Class.EndSearch(); return; }
#define FOR_ALL_GOTO(whereTo) { Engine::HatchFuncs.Class.EndSearch(); goto whereTo; }

#define ADD_ENUM_VAL(enumName, enumValue) { Engine::HatchFuncs.Class.AddEnumValue(enumName, enumValue); }

#define IS_EDITOR (Engine::GameState->IsEditor)

#define SET_BASIC() Interactable = true;
#define SET_DRAWGROUP(d) DrawGroup = d; CanDraw = true;
#define SET_UPDATERANGE(x, y) UpdateType = UpdateType_Ranged; UpdateRange = Vector2(x << 16, y << 16);

namespace Engine {
    extern ::HatchFunctionSet HatchFuncs;
    extern ::ServicesFunctionSet ServiceFuncs;
    extern ::Entity** CurrentEntityPtr;
    extern ::GameState* GameState;

    extern Globals* gVars;
    extern size_t   gVarsSize;

    extern CString ClassNameList[MAX_CLASSES];
    extern bool  BufferedIsGlobalClassList[MAX_CLASSES];
    extern Class BufferedClassList[MAX_CLASSES];
    extern int   BufferedClassCount;

    template <typename M>
    static void* UnionCast(M functionPtr) {
        union { void* rawPtr; M funcPtr; } caster;
        caster.funcPtr = functionPtr;
        return caster.rawPtr;
    }

    template <typename E, typename S>
    static S* RegisterClass(CString name, S** staticObjectPtr, bool isGlobal) {
        ClassNameList[BufferedClassCount] = name;
        BufferedIsGlobalClassList[BufferedClassCount] = isGlobal;
        Class* objectClass = &BufferedClassList[BufferedClassCount++];

        objectClass->StaticObjectPtr = (void**)staticObjectPtr;
        objectClass->EntitySize = sizeof(E);
        objectClass->StaticObjectSize = sizeof(S);

        if (UnionCast(&Entity::OnStageLoad) != UnionCast(&E::OnStageLoad))
            objectClass->onStageLoad = E::OnStageLoad;
        else objectClass->onStageLoad = NULL;

        if (UnionCast(&Entity::OnEditorLoad) != UnionCast(&E::OnEditorLoad))
            objectClass->onEditorLoad = E::OnEditorLoad;
        else objectClass->onEditorLoad = NULL;

        if (UnionCast(&Entity::OnStaticUpdate) != UnionCast(&E::OnStaticUpdate))
            objectClass->onStaticUpdate = E::OnStaticUpdate;
        else objectClass->onStaticUpdate = NULL;

        if (UnionCast(&Entity::OnCreate) != UnionCast(&E::OnCreate))
            objectClass->onCreate = [](CreateFlag data) -> void { E* entity = (E*)*CurrentEntityPtr; entity->OnCreate(data); };
        else objectClass->onCreate = NULL;

        if (UnionCast(&Entity::OnUpdate) != UnionCast(&E::OnUpdate))
            objectClass->onUpdate = []() -> void { E* entity = (E*)*CurrentEntityPtr; entity->OnUpdate(); };
        else objectClass->onUpdate = NULL;

        if (UnionCast(&Entity::OnUpdateLate) != UnionCast(&E::OnUpdateLate))
            objectClass->onUpdateLate = []() -> void { E* entity = (E*)*CurrentEntityPtr; entity->OnUpdateLate(); };
        else objectClass->onUpdateLate = NULL;

        if (UnionCast(&Entity::OnStageDraw) != UnionCast(&E::OnStageDraw))
            objectClass->onStageDraw = []() -> void { E* entity = (E*)*CurrentEntityPtr; entity->OnStageDraw(); };
        else objectClass->onStageDraw = NULL;

        if (UnionCast(&Entity::OnEditorDraw) != UnionCast(&E::OnEditorDraw))
            objectClass->onEditorDraw = []() -> void { E* entity = (E*)*CurrentEntityPtr; entity->OnEditorDraw(); };
        else objectClass->onEditorDraw = NULL;

        if (UnionCast(&Entity::OnSetup) != UnionCast(&E::OnSetup))
            objectClass->onSetup = E::OnSetup;
        else objectClass->onSetup = NULL;

        objectClass->onStaticConstructor = [](void* staticMemory) -> void {
            new (staticMemory) S();
        };

        return NULL;
    }

    extern Globals* RegisterGlobals(size_t size);
}

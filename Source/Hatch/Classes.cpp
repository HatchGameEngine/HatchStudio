#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Classes.h>

#include <Hatch/Hashing/MD5.h>
#include <Hatch/Diagnostics.h>
#include <Hatch/GameLinker.h>
#include <Hatch/Memory.h>

namespace Classes {
    ClassAttribute ClassAttributes[0x100];
    int            ClassAttributeCount = 0;

    void Add(CString className, void** staticObjectPtr, size_t entitySize, size_t staticObjectSize, void (*onStageLoad)(), void (*onEditorLoad)(), void (*onStaticUpdate)(), void (*onCreate)(CreateFlag flag), void (*onUpdate)(), void (*onUpdateLate)(), void (*onStageDraw)(), void (*onEditorDraw)(), void (*onSetup)(), void (*onStaticConstructor)(void* staticObject)) {
        if (GameLinker::ClassCount >= 4096)
            return;

        Class* objectClass = &GameLinker::ClassList[GameLinker::ClassCount++];

        objectClass->Name = MD5_HashString(className);

        objectClass->StaticObjectPtr = staticObjectPtr;
        objectClass->EntitySize = entitySize;
        objectClass->StaticObjectSize = staticObjectSize;

        objectClass->onStageLoad = onStageLoad;
        objectClass->onEditorLoad = onEditorLoad;
        objectClass->onStaticUpdate = onStaticUpdate;
        objectClass->onCreate = onCreate;
        objectClass->onUpdate = onUpdate;
        objectClass->onUpdateLate = onUpdateLate;
        objectClass->onStageDraw = onStageDraw;
        objectClass->onEditorDraw = onEditorDraw;
        objectClass->onSetup = onSetup;
        objectClass->onStaticConstructor = onStaticConstructor;

        if (entitySize > sizeof(EntitySlot)) {
            Diagnostics::SetError("Warning! Class \"%s\" entity size of 0x%X is larger than the allotted 0x%X for entities!", className, entitySize, sizeof(EntitySlot));
            printf("%s\n", Diagnostics::ErrorString);
        }
    }
    void CreateGlobalClass(CString className, void** staticObjectPtr, size_t staticObjectSize, void (*onStaticConstructor)(void* staticObject)) {
        Memory::Alloc(staticObjectPtr, staticObjectSize, Memory::MEMPOOL_STAGE, true);
        if (*staticObjectPtr)
            onStaticConstructor(*staticObjectPtr);
    }
    void SetupAttribute(int attributeType, CString name, size_t offset) {
        if (ClassAttributeCount >= 0x100)
            return;

        ClassAttribute* attr = &ClassAttributes[ClassAttributeCount++];
        attr->Name = MD5_HashString(name);
        attr->StructOffset = offset;
        attr->AttributeType = attributeType;
    }
}

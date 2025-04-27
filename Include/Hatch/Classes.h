#pragma once

namespace Classes {
    struct ClassAttribute {
        Hash Name;
        size_t StructOffset;
        int Using;
        int AttributeType;
    };

    extern ClassAttribute ClassAttributes[0x100];
    extern int            ClassAttributeCount;

    void Add(CString className, void** staticObjectPtr, size_t entitySize, size_t staticObjectSize, void (*onStageLoad)(), void (*onEditorLoad)(), void (*onStaticUpdate)(), void (*onCreate)(CreateFlag flag), void (*onUpdate)(), void (*onUpdateLate)(), void (*onStageDraw)(), void (*onEditorDraw)(), void (*onSetup)(), void (*onStaticConstructor)(void* staticObject));
    void CreateGlobalClass(CString className, void** staticObjectPtr, size_t staticObjectSize, void (*onStaticConstructor)(void* staticObject));
    void SetupAttribute(int attributeType, CString name, size_t offset);
}

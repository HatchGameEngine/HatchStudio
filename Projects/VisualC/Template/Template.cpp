// Main required headers
#include <Hatch/GameLogic/Engine.h>
#include "Globals.h"

// Class header
#include "$itemname$.h"

// Referenced classes' headers

// Class registration
REGISTER_CLASS($safeitemname$);

#define $$ $($safeitemname$)

void $safeitemname$::OnStageLoad() {}
void $safeitemname$::OnEditorLoad() {}

void $safeitemname$::OnCreate(CreateFlag flag) {}

void $safeitemname$::OnStaticUpdate() {}

void $safeitemname$::OnUpdate() {}
void $safeitemname$::OnUpdateLate() {}

void $safeitemname$::OnStageDraw() {}
void $safeitemname$::OnEditorDraw() {}

void $safeitemname$::OnSetup() {
    // SETUP_ATTRIBUTE($safeitemname$, VAR_ENUM, flags);
}

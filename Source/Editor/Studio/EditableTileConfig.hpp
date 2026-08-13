#pragma once

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>

struct EditableTileConfig {
    Uint8 Collision[16];
    Uint8 Orientation;
    Uint8 AngleTop;
    Uint8 AngleLeft;
    Uint8 AngleRight;
    Uint8 AngleBottom;
    Uint8 Behavior;
};

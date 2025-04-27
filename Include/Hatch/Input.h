#pragma once

namespace Input {
    extern InputPad PadInputs[4];
    extern InputTouch TouchInputs[8];

    bool Init();
    void Dispose();
    void Poll();
}

#pragma once

namespace Video {
    bool PlayStream(CString filename, double startPosition, bool (*stateFunction)());
    void DecodeFrame(bool init);
}

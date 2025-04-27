#pragma once

namespace Diagnostics {
    extern char ErrorString[1024];
    extern void (*NetworkStatus)();

    void SetError(CString text, ...);

    int  NetworkThread(void* opaque);
    bool NetworkLock();
    void NetworkUnlock();
}

#pragma once

namespace Threading {
    struct Thread { };
    typedef int(*ThreadFunction)(void*);

    Thread* CreateThread(ThreadFunction function, void* opaque);
    void    DetachThread(Thread* thread);
    int     WaitThread(Thread* thread);
}

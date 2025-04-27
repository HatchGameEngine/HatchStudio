#pragma once

namespace Clock {
    struct Counter {
        Uint8 Padding[0x20];
    };

    void CounterStart(Counter* counter);
    void CounterFinish(Counter* counter);
    double CounterGetElapsed(Counter* counter);

    void Delay(double millis);
}

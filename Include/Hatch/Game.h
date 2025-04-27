#pragma once

namespace Game {
    extern bool Running;
    extern int UpdatesPerFrame;
    extern bool StepForward;
    extern GameState State;

    extern bool GifManualRecordOn;

    extern char Title[32];
    extern char Subtitle[32];
    extern char Version[32];

    extern char OverridenStartScene[256];

    void UpdateWindowTitle();
    void SetWindowTitle(CString title);

    bool Init();
    void Run();
}

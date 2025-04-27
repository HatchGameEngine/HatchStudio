#pragma once

namespace Settings {
    struct GraphicsSettings {
        int frameWidth;
        int frameHeight;
        int windowWidth;
        int windowHeight;
        bool vsync;
        bool fullscreen;
        bool borderless;
    };
    struct AudioSettings {
        int bgmVolume;
        int sfxVolume;
    };
    struct DevSettings {
        int frameSkip;
    };
    extern GraphicsSettings graphics;
    extern AudioSettings audio;
    extern DevSettings dev;

    void Load();
    void Save();
    void Init();
    void Dispose();
}

#include "SDL.h"

#ifdef USE_DYNAMIC_GAMELOGIC_LINK
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/GameLogic/GameLib.h>
#include <Hatch/ImageFormats/GIF.h>
#include <Hatch/IO/FileStream.h>

#include <Hatch/Audio.h>
#include <Hatch/Classes.h>
#include <Hatch/Clock.h>
#include <Hatch/Diagnostics.h>
#include <Hatch/Game.h>
#include <Hatch/GameLinker.h>
#include <Hatch/Graphics.h>
#include <Hatch/Input.h>
#include <Hatch/Math.h>
#include <Hatch/Memory.h>
#include <Hatch/Renderer.h>
#include <Hatch/Resources.h>
#include <Hatch/Scene.h>
#include <Hatch/Services.h>
#include <Hatch/Settings.h>
#include <Hatch/Threading.h>
#include <Hatch/Video.h>

bool FreezeApp = false;

namespace Renderer {
    SDL_Window* Window;
    Uint32 WindowPixelFormat;

    SDL_Renderer* Renderer;
    SDL_Texture* FrameBufferTexture;
    SDL_Texture* VideoTexture;

    int ConversionBPP;
    Uint32 ConversionRMask, ConversionGMask, ConversionBMask, ConversionAMask;

    bool Init() {
        Window = NULL;
        Renderer = NULL;
        FrameBufferTexture = NULL;
        VideoTexture = NULL;

        SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
        SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
            Diagnostics::SetError("SDL_Init failed with error: %s", SDL_GetError());
            return false;
        }

        Window = SDL_CreateWindow(NULL,
            SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0),
            FRAMEBUFFER_WIDTH * 2, FRAMEBUFFER_HEIGHT * 2,
            SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!Window) {
            Diagnostics::SetError("SDL_CreateWindow failed with error: %s", SDL_GetError());
            return false;
        }

        WindowPixelFormat = SDL_GetWindowPixelFormat(Window);
        if (!SDL_PixelFormatEnumToMasks(WindowPixelFormat, &ConversionBPP, &ConversionRMask, &ConversionGMask, &ConversionBMask, &ConversionAMask)) {
            Diagnostics::SetError("SDL_PixelFormatEnumToMasks failed with error: %s", SDL_GetError());
            return false;
        }

        Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!Renderer) {
            Diagnostics::SetError("SDL_CreateRenderer failed with error: %s", SDL_GetError());
            return false;
        }

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

        FrameBufferTexture = SDL_CreateTexture(Renderer, WindowPixelFormat, SDL_TEXTUREACCESS_STREAMING, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
        if (!FrameBufferTexture) {
            Diagnostics::SetError("SDL_CreateTexture failed with error: %s", SDL_GetError());
            return false;
        }

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

        VideoTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, 1024, 1024);
        if (!VideoTexture) {
            Diagnostics::SetError("SDL_CreateTexture failed with error: %s", SDL_GetError());
            return false;
        }

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

        SDL_GetRendererOutputSize(Renderer, &RendererW, &RendererH);
        return true;
    }
    void Clear() {
        SDL_RenderClear(Renderer);
    }
    void TransferFrameBuffers() {
        int tw, th;
        SDL_Rect src;
        SDL_Rect dst;
        int frameBufferTexturePitch;
        Uint32* frameBufferTexturePixels;

        // SDL_GetRendererOutputSize(Renderer, &RendererW, &RendererH);

        if (Game::State.EngineState == ENGINESTATE_VIDEO) {
            src = { 0, 0, 1024, 512 };
            dst = { 0, 0, RendererW, RendererH };
            SDL_SetRenderDrawColor(Renderer, 0xFF, 0, 0, 0xFF);
            SDL_RenderFillRect(Renderer, NULL);

            SDL_RenderCopy(Renderer, VideoTexture, &src, NULL);
            return;
        }

        ViewOutput* viewOutput = &Graphics::ViewOutputs[0];
        if (viewOutput->Active) {
            View* view = &Graphics::Views[viewOutput->ViewIndex];

            // Update texture if it's a different size
            if (!SDL_QueryTexture(FrameBufferTexture, NULL, NULL, &tw, &th)) {
                if (tw != view->Width || th != view->Height) {
                    // Re-create texture
                    SDL_DestroyTexture(FrameBufferTexture);

                    FrameBufferTexture = SDL_CreateTexture(Renderer, WindowPixelFormat, SDL_TEXTUREACCESS_STREAMING, view->Width, view->Height);
                    if (!FrameBufferTexture) {
                        Diagnostics::SetError("SDL_CreateTexture failed with error: %s", SDL_GetError());
                        return;
                    }
                }
            }

            // Update texture pixels
            if (!SDL_LockTexture(FrameBufferTexture, NULL, (void**)&frameBufferTexturePixels, &frameBufferTexturePitch)) {
                Uint8 colorBuffer[4];
                int rowP = 0, rowC = 0;
                for (int row = view->Height; row; row--) {
                    Uint32* cRow = &frameBufferTexturePixels[rowC];
                    Pixel*  pRow = &view->Pixels[rowP];

                    for (int x = view->Width; x; x--) {
                        *cRow = ConversionAMask;

                        colorBuffer[0] = colorBuffer[1] = colorBuffer[2] = colorBuffer[3] = (pRow->R << 3);
                        *cRow |= (*(Uint32*)colorBuffer) & ConversionRMask;
                        colorBuffer[0] = colorBuffer[1] = colorBuffer[2] = colorBuffer[3] = (pRow->G << 3);
                        *cRow |= (*(Uint32*)colorBuffer) & ConversionGMask;
                        colorBuffer[0] = colorBuffer[1] = colorBuffer[2] = colorBuffer[3] = (pRow->B << 3);
                        *cRow |= (*(Uint32*)colorBuffer) & ConversionBMask;

                        cRow++;
                        pRow++;
                    }

                    rowP += view->Pitch;
                    rowC += view->Width;
                }
                SDL_UnlockTexture(FrameBufferTexture);
            }

            // Render to screen
            src = { 0, 0, view->Width, view->Height };
            dst = { viewOutput->X, viewOutput->Y, viewOutput->Width, viewOutput->Height };
            SDL_RenderCopy(Renderer, FrameBufferTexture, &src, &dst);
        }
    }
    void Present() {
        SDL_RenderPresent(Renderer);
    }
    void Dispose() {
        SDL_DestroyTexture(FrameBufferTexture);
        SDL_DestroyRenderer(Renderer);
        SDL_DestroyWindow(Window);
        SDL_Quit();
    }

    void UpdateTexture420(int width, int height, Uint8* pixelY, Uint8* pixelU, Uint8* pixelV, int strideY, int strideU, int strideV) {
        SDL_Rect src = { 0, 0, width, height };
        SDL_UpdateYUVTexture(VideoTexture, &src, pixelY, strideY, pixelU, strideU, pixelV, strideV);
    }
    void UpdateTexture422(int width, int height, Uint8* pixelY, Uint8* pixelU, Uint8* pixelV, int strideY, int strideU, int strideV) {
        SDL_Rect src = { 0, 0, width, height };
        SDL_UpdateYUVTexture(VideoTexture, &src, pixelY, strideY, pixelU, strideU, pixelV, strideV);
    }
    void UpdateTexture444(int width, int height, Uint8* pixelY, Uint8* pixelU, Uint8* pixelV, int strideY, int strideU, int strideV) {
        SDL_Rect src = { 0, 0, width, height };
        SDL_UpdateYUVTexture(VideoTexture, &src, pixelY, strideY, pixelU, strideU, pixelV, strideV);
    }
}
namespace Audio {
    SDL_AudioDeviceID Device;
    SDL_AudioSpec     DeviceFormat;

    void   AudioCallback(void* data, Uint8* stream, int len) {
        if (FreezeApp) {
            memset(stream, 0, len);
            return;
        }

        RunPlaybacks((Sample*)stream);
    }

    // Implementations
    bool   PlatformInit() {
        Device = 0;

        SDL_AudioSpec options;
        memset(&options, 0, sizeof(options));

        options.freq = SAMPLE_RATE;
        options.format = AUDIO_S16;
        options.samples = SAMPLE_READ;
        options.channels = SAMPLE_CHANNELS;
        options.callback = AudioCallback;
        if ((Device = SDL_OpenAudioDevice(NULL, false, &options, &DeviceFormat, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE)) == 0) {
            Diagnostics::SetError("SDL_OpenAudioDevice failed with error: %s", SDL_GetError());
            return false;
        }

        // BUG: iPhone slows down audio because it wants 48000 Hz instead of 44100.
        SAMPLE_RATE_MOD = (((Uint64)SAMPLE_RATE) << 8) / DeviceFormat.freq;

        SDL_PauseAudioDevice(Device, 0);
        return true;
    }
    void   PlatformDispose() {
        if (!Device)
            return;

        SDL_PauseAudioDevice(Device, 1);
        SDL_CloseAudioDevice(Device);
    }
    bool   Lock() {
        if (!Device)
            return false;

        SDL_LockAudioDevice(Device);
        return true;
    }
    void   Unlock() {
        if (!Device)
            return;

        SDL_UnlockAudioDevice(Device);
    }
}
namespace Resources {
    const char* ResourceFolderPrefix = "";

    bool PlatformInit() {
        return true;
    }
    void PlatformDispose() {

    }
}
namespace GameLinker {
    void Load() {
        LinkData linkData;
        linkData.HatchFuncs = &HatchFuncs;
        linkData.ServiceFuncs = &ServiceFuncs;
        linkData.CurrentEntityPtr = &Scene::CurrentEntity;
        linkData.GameStatePtr = &Game::State;

        GameLib::LinkGameLogic(&linkData);
    }
}
namespace Input {
    SDL_Joystick* Controllers[8] = { NULL };
    SDL_Haptic* ControllerHaptics[8] = { NULL };

    void Poll() {
        enum INPUT_FLAGS {
            INPUT_UP = 0x1,
            INPUT_DOWN = 0x2,
            INPUT_LEFT = 0x4,
            INPUT_RIGHT = 0x8,
            INPUT_A = 0x10,
            INPUT_B = 0x20,
            INPUT_C = 0x40,
            INPUT_X = 0x80,
            INPUT_Y = 0x100,
            INPUT_Z = 0x200,
            INPUT_START = 0x400,
            INPUT_SELECT = 0x800,
        };
        InputState* inputOutputs[] = {
            &PadInputs[0].Up,
            &PadInputs[0].Down,
            &PadInputs[0].Left,
            &PadInputs[0].Right,
            &PadInputs[0].A,
            &PadInputs[0].B,
            &PadInputs[0].C,
            &PadInputs[0].X,
            &PadInputs[0].Y,
            &PadInputs[0].Z,
            &PadInputs[0].Start,
            &PadInputs[0].Select,
        };
        Uint32 inputBitfield = 0;

        #if 1
        // Joystick controls
        SDL_Joystick* joy = Controllers[0];
        if (joy) {
            if (SDL_JoystickGetAttached(joy)) {
                struct inputKeyPair {
                    Uint32 bitflag;
                    int button;
                }
                inputKeyPairs[] = {
                    { INPUT_UP, 13 },
                    { INPUT_DOWN, 15 },
                    { INPUT_LEFT, 12 },
                    { INPUT_RIGHT, 14 },
                    { INPUT_A, 0 },
                    { INPUT_B, 1 },
                    { INPUT_C, 3 },
                    { INPUT_X, 2 },
                    { INPUT_Y, 8 },
                    { INPUT_Z, 9 },
                    { INPUT_START, 10 },
                    { INPUT_SELECT, 11 },
                };

                for (size_t i = 0; i < sizeof(inputKeyPairs) / sizeof(inputKeyPairs[0]); i++) {
                    if (SDL_JoystickGetButton(joy, inputKeyPairs[i].button))
                        inputBitfield |= inputKeyPairs[i].bitflag;
                }
            }
        }
        #endif

        #if 1
        // Keyboard controls
        const Uint8* state = SDL_GetKeyboardState(NULL);
        struct inputKeyPair {
            Uint32 bitflag;
            SDL_Scancode scancode;
        }
        inputKeyPairs[] = {
            { INPUT_UP, SDL_SCANCODE_W },
            { INPUT_DOWN, SDL_SCANCODE_S },
            { INPUT_LEFT, SDL_SCANCODE_A },
            { INPUT_RIGHT, SDL_SCANCODE_D },
            { INPUT_A, SDL_SCANCODE_J },
            { INPUT_B, SDL_SCANCODE_K },
            { INPUT_C, SDL_SCANCODE_L },
            { INPUT_X, SDL_SCANCODE_U },
            { INPUT_Y, SDL_SCANCODE_I },
            { INPUT_Z, SDL_SCANCODE_O },
            { INPUT_START, SDL_SCANCODE_P },
            { INPUT_SELECT, SDL_SCANCODE_M },
        };
        for (size_t i = 0; i < sizeof(inputKeyPairs) / sizeof(inputKeyPairs[0]); i++) {
            if (state[inputKeyPairs[i].scancode])
                inputBitfield |= inputKeyPairs[i].bitflag;
        }
        #endif

        #if 1
        // Touch controls
        ViewOutput* viewOutput = &Graphics::ViewOutputs[0];
        if (viewOutput->Active) {
            View* view = &Graphics::Views[viewOutput->ViewIndex];
            for (int d = SDL_GetNumTouchDevices() - 1; d >= 0; d--) {
                SDL_TouchID device = SDL_GetTouchDevice(d);
                if (!device)
                    continue;

                int fingerCount = SDL_GetNumTouchFingers(device);
                for (int f = 0; f < fingerCount; f++) {
                    int tX, tY;
                    SDL_Finger* touch = SDL_GetTouchFinger(device, f);
                    if (!touch)
                        continue;

                    int touchInViewX = (int)(((touch->x * Renderer::RendererW) - viewOutput->X) * view->Width / viewOutput->Width);
                    int touchInViewY = (int)(((touch->y * Renderer::RendererH) - viewOutput->Y) * view->Height / viewOutput->Height);

                    tX = touchInViewX - (64);
                    tY = touchInViewY - (view->Height - 64);
                    if (tX * tX + tY * tY < 96 * 96) {
                        int angle = (((Math::ATan(tX, tY) + 0x20) & 0xC0) >> 6) << 1;
                        switch (angle) {
                            case 0:
                                inputBitfield |= INPUT_RIGHT;
                                break;
                            case 1:
                                inputBitfield |= INPUT_RIGHT | INPUT_DOWN;
                                break;
                            case 2:
                                inputBitfield |= INPUT_DOWN;
                                break;
                            case 3:
                                inputBitfield |= INPUT_DOWN | INPUT_LEFT;
                                break;
                            case 4:
                                inputBitfield |= INPUT_LEFT;
                                break;
                            case 5:
                                inputBitfield |= INPUT_LEFT | INPUT_UP;
                                break;
                            case 6:
                                inputBitfield |= INPUT_UP;
                                break;
                            case 7:
                                inputBitfield |= INPUT_UP | INPUT_RIGHT;
                                break;
                        }
                    }

                    tX = touchInViewX - (view->Width - 64);
                    tY = touchInViewY - (view->Height - 64);
                    if (tX * tX + tY * tY < 48 * 48) {
                        inputBitfield |= INPUT_A;
                    }

                    tX = touchInViewX - (view->Width - 32);
                    tY = touchInViewY - (32);
                    if (tX * tX + tY * tY < 32 * 32) {
                        inputBitfield |= INPUT_START;
                    }
                }
            }
        }
        #endif

        // Finalize inputs
        for (size_t i = 0; i < sizeof(inputOutputs) / sizeof(inputOutputs[0]); i++) {
            int down = (inputBitfield >> i) & 1;
            inputOutputs[i]->Pressed  = down && !inputOutputs[i]->Down;
            inputOutputs[i]->Released = !down && inputOutputs[i]->Down;
            inputOutputs[i]->Down     = down;
        }
    }
}
namespace Game {
    void SetWindowTitle(CString title) {
        if (Renderer::Window) {
            SDL_SetWindowTitle(Renderer::Window, title);
        }
    }
}
namespace Clock {
    struct PlatformCounter {
        Uint64 reference;
        Uint64 elapsed;
    };

    void CounterStart(Counter* counter) {
        ((PlatformCounter*)counter)->reference = SDL_GetPerformanceCounter();
    }
    void CounterFinish(Counter* counter) {
        ((PlatformCounter*)counter)->elapsed = SDL_GetPerformanceCounter() - ((PlatformCounter*)counter)->reference;
    }
    double CounterGetElapsed(Counter* counter) {
        return ((PlatformCounter*)counter)->elapsed * 1000.0 / SDL_GetPerformanceFrequency();
    }

    void Delay(double millis) {
        SDL_Delay((int)millis);
    }
}
namespace Threading {
    Thread* CreateThread(ThreadFunction function, void* opaque) {
        return (Thread*)SDL_CreateThread(function, "SDL_Thread", opaque);
    }
    void    DetachThread(Thread* thread) {
        SDL_DetachThread((SDL_Thread*)thread);
    }
    int     WaitThread(Thread* thread) {
        int value = 0;
        SDL_WaitThread((SDL_Thread*)thread, &value);
        return value;
    }
}

bool Init() {
    if (!Memory::Init())
        return false;

    Settings::Init();
	Settings::audio.sfxVolume = 50;
	Settings::audio.bgmVolume = 50;
    if (!Resources::Init())
        return false;

    Services::Init();

    if (!Game::Init())
        return false;

    Graphics::Init();

    // Game Window
    if (!Renderer::Init())
        return false;

    if (!Audio::Init())
        return false;

    return true;
}
void Dispose() {
    Audio::Dispose();
    Services::Dispose();
    Renderer::Dispose();

    if (GameLinker::GameLogicSharedObject)
        SDL_UnloadObject(GameLinker::GameLogicSharedObject);

    Resources::Dispose();
    Settings::Dispose();
    Memory::Dispose();
}
bool ReloadGame() {
    Dispose();
    return Init();
}

Uint64 currentTime, nextTime, frameDuration;

void RunFrame(void*) {
    currentTime = SDL_GetPerformanceCounter();

    // Poll events
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                Game::Running = false;
                break;
            case SDL_WINDOWEVENT:
                break;
            case SDL_JOYDEVICEADDED:
                break;
            case SDL_JOYDEVICEREMOVED:
                break;
        }
    }

    Renderer::UpdateViewOutputs();

    // if (currentTime <= nextTime)
    //     goto Present;
    nextTime = currentTime + frameDuration;

    GameLinker::ServiceFuncs.Core.Run();

    if (FreezeApp)
        return;


    // Run game
    Game::Run();

    Present:
    Renderer::Clear();
    Renderer::TransferFrameBuffers();
    Renderer::Present();
}

int HandleAppEvents(void *userdata, SDL_Event *event) {
    switch (event->type) {
        case SDL_APP_TERMINATING:
            Dispose();
            return false;
        case SDL_APP_LOWMEMORY:
            return false;
        case SDL_APP_WILLENTERBACKGROUND:
            // NOTE: This here should send a signal to activate a pause menu.
            FreezeApp = true;
            return false;
        case SDL_APP_DIDENTERBACKGROUND:
            FreezeApp = true;
            return false;
        case SDL_APP_WILLENTERFOREGROUND:
            return false;
        case SDL_APP_DIDENTERFOREGROUND:
            FreezeApp = false;
            return false;
        default:
            return true;
    }
}

int main(int argc, char** args) {
    Game::Running = Init();

    frameDuration = SDL_GetPerformanceFrequency() / 60;
    currentTime = SDL_GetPerformanceCounter();
    nextTime = currentTime + frameDuration;

    SDL_SetEventFilter(HandleAppEvents, NULL);

    #if 0
        if (Game::Running) {
            SDL_iPhoneSetAnimationCallback(Renderer::Window, 1, RunFrame, NULL);
        }
    #else
        while (Game::Running) {
            RunFrame(NULL);
        }
    #endif

	return 0;
}

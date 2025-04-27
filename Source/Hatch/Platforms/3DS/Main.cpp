#include <citro2d.h>

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

// 3DS renderer implementation.
// Uses Citro3D.
namespace Renderer {
    struct VFrame {
        C2D_Image img;
        C3D_Tex buff[2];
        int w, h;
        bool curbuf;
    };

    Handle y2rEvent;
    // Pixel* FrameBufferTopScreenLeftEye;
    // Pixel* FrameBufferTopScreenRightEye;
    // Pixel* FrameBufferBottomScreen;
    C3D_RenderTarget* RenderTargetTopScreenLeftEye;
    C3D_RenderTarget* RenderTargetTopScreenRightEye;
    C3D_RenderTarget* RenderTargetBottomScreen;

    VFrame frameL, frameR;
    VFrame frameVideo;
    Sint32 FramebufferSwizzler[FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT];

    static inline size_t fmtGetBPP(GPU_TEXCOLOR fmt) {
        switch (fmt) {
            case GPU_RGBA8:
                return 4;
            case GPU_RGB8:
                return 3;
            default:
                return 0;
        }
    }

    void UpdateFramebuffer(VFrame* vframe, Pixel* pixels, int pitch) {
        bool drawbuf = !vframe->curbuf;
        C3D_Tex* wframe = &vframe->buff[drawbuf];

        Pixel* srcPx = pixels;
        Pixel* dstPx = (Pixel*)wframe->data;
        const size_t size = FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT;
        for (size_t r = 0; r < size; r++) {
            dstPx[FramebufferSwizzler[r]] = srcPx[r];
        }

        vframe->curbuf = drawbuf;
        vframe->img.tex = wframe;
    }

    void VFrame_Init(VFrame* vframe, int w, int h, GPU_TEXCOLOR fmt = GPU_RGB8, GPU_TEXTURE_FILTER_PARAM filter = GPU_LINEAR) {
        struct { int width, height; } infob;

        infob.width = w;
        infob.height = h;

        auto* info = &infob;

        vframe->w = w;
        vframe->h = h;

        for (int i = 0; i < 2; i++) {
            C3D_Tex* curtex = &vframe->buff[i];
            C3D_TexInit(curtex, Math::ToNextPOT(info->width), Math::ToNextPOT(info->height), fmt);
            C3D_TexSetFilter(curtex, filter, filter);
            memset(curtex->data, 0, curtex->size);
        }

        Tex3DS_SubTexture* subtex = (Tex3DS_SubTexture*)malloc(sizeof(Tex3DS_SubTexture));

        subtex->width = info->width;
        subtex->height = info->height;
        subtex->left = 0.0f;
        subtex->top = 1.0f;
        subtex->right = (float)info->width / Math::ToNextPOT(info->width);
        subtex->bottom = 1.0f - ((float)info->height / Math::ToNextPOT(info->height));

        vframe->curbuf = false;
        vframe->img.tex = &vframe->buff[vframe->curbuf];
        vframe->img.subtex = subtex;
    }
    void VFrame_Dispose(VFrame* vframe) {
        if (vframe->buff[0].data) {
            C3D_TexDelete(&vframe->buff[0]);
            C3D_TexDelete(&vframe->buff[1]);
            // free(image->tex);
        }

        if (vframe->img.subtex)
            free((void*)vframe->img.subtex);
    }
    void VFrame_Draw(VFrame* vframe, float x, float y, float depth) {
        float scaleX, scaleY;
        scaleX = scaleY = 240.0f / vframe->h;

        C2D_Image img = vframe->img;
        C2D_DrawParams params = {
            { x, y, scaleX * img.subtex->width, scaleY * img.subtex->height },
            { (scaleX * img.subtex->width) / 2.0f, (scaleY * img.subtex->height) / 2.0f },
            depth, 0.0f
        };
        C2D_DrawImage(img, &params, NULL);
    }

    // Implementation
    bool Init() {
        // Enable n3DS overclocking, it is sorely needed.
        osSetSpeedupEnable(true);
        /**
         * This allows for music to continue playing through the headphones whilst
         * the 3DS is closed.
         */
        aptSetSleepAllowed(false);

        // aptHandleSleep();

        // aptSetHomeAllowed (bool allowed)
         //   Configures whether the user can press the HOME button to jump back to the HOME menu while the application is active.

        // APT_CheckNew3DS(bool* out);
        // Checks whether the system is a New 3DS.

        // Initialize 3DS FrameBuffer with RGBA8 for top screen and BGR8 for bottom.
        gfxInitDefault();
        consoleInit(GFX_BOTTOM, NULL);
        y2rInit();

        // Enable stereoscopic 3D.
        #ifdef ENABLE_STEREOSCOPIC_VIEW
            gfxSet3D(ENABLE_STEREOSCOPIC_VIEW);
        #endif

        // Turn on double buffering, to prevent tearing.
        // gfxSetDoubleBuffering(GFX_TOP, true);

        C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
        C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
        C2D_Prepare();

        // Get screen render targets
        RenderTargetTopScreenLeftEye = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

        // Create framebuffer textures
        VFrame_Init(&frameL, 400, 240, GPU_RGBA5551, GPU_NEAREST);
        VFrame_Init(&frameR, 400, 240, GPU_RGBA5551, GPU_NEAREST);
        VFrame_Init(&frameVideo, 480, 240);

        // Build framebuffer swizzler
        int index = 0;
        for (int y = 0; y < FRAMEBUFFER_HEIGHT; y++) {
            for (int x = 0; x < FRAMEBUFFER_WIDTH; x++) {
                FramebufferSwizzler[index] =
                    (x & 1) |
                    ((x >> 1 & 1) << 2) |
                    ((x >> 2 & 1) << 4) |
                    ((y & 1) << 1) |
                    ((y >> 1 & 1) << 3) |
                    ((y >> 2 & 1) << 5) |

                    ((x >> 3) << 6) |
                    ((y >> 3) * 8 * 512); // 512 texture width, every 8 scanlines is a new tile line
                index++;
            }
        }
        return true;
    }
    void TransferFrameBuffers() {
        if (Game::State.EngineState == ENGINESTATE_VIDEO)
            return;

        #ifdef ENABLE_STEREOSCOPIC_VIEW
        float slider = osGet3DSliderState();
        #else
        float slider = 0.f;
        #endif
        Graphics::StereoscopicSplit = slider;

        if (slider == 0.0f) {
            // Set the target framebuffer to the left-eye top screen,
            // this needs to be done here when double-buffering.
            // FrameBufferTopScreenLeftEye = (Pixel*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

            // int l, x, pitch = Graphics::Views[0].Pitch;
            // Pixel* srcLine, *srcPx = &Graphics::Views[0].Pixels[0];
            // Pixel* dstLine, *dstPx = &FrameBufferTopScreenLeftEye[FRAMEBUFFER_HEIGHT - 1];
            // for (l = FRAMEBUFFER_HEIGHT; l; l--) {
            //     srcLine = srcPx;
            //     dstLine = dstPx;
            //     for (x = FRAMEBUFFER_WIDTH; x; x--) {
            //         dstLine->Full = srcLine->Full;
            //         dstLine += FRAMEBUFFER_HEIGHT;
            //         srcLine++;
            //     }
            //     srcPx += pitch;
            //     dstPx--;
            // }

            UpdateFramebuffer(&frameL, &Graphics::Views[0].Pixels[0], Graphics::Views[0].Pitch);
        }
        else {
            // FrameBufferTopScreenLeftEye = (Pixel*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
            // FrameBufferTopScreenRightEye = (Pixel*)gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, NULL, NULL);

            // int l, x, pitch = Graphics::Views[0].Pitch;
            // Pixel* srcLine, *srcPx = &Graphics::Views[0].Pixels[0];
            // Pixel* dstLine, *dstPx = &FrameBufferTopScreenLeftEye[FRAMEBUFFER_HEIGHT - 1];
            // for (l = FRAMEBUFFER_HEIGHT; l; l--) {
            //     srcLine = srcPx;
            //     dstLine = dstPx;
            //     for (x = FRAMEBUFFER_WIDTH; x; x--) {
            //         dstLine->Full = srcLine->Full;
            //         dstLine += FRAMEBUFFER_HEIGHT;
            //         srcLine++;
            //     }
            //     srcPx += pitch;
            //     dstPx--;
            // }
            //
            // srcPx = &Graphics::Views[1].Pixels[0];
            // dstPx = &FrameBufferTopScreenRightEye[FRAMEBUFFER_HEIGHT - 1];
            // for (l = FRAMEBUFFER_HEIGHT; l; l--) {
            //     srcLine = srcPx;
            //     dstLine = dstPx;
            //     for (x = FRAMEBUFFER_WIDTH; x; x--) {
            //         dstLine->Full = srcLine->Full;
            //         dstLine += FRAMEBUFFER_HEIGHT;
            //         srcLine++;
            //     }
            //     srcPx += pitch;
            //     dstPx--;
            // }

            UpdateFramebuffer(&frameL, &Graphics::Views[0].Pixels[0], Graphics::Views[0].Pitch);
            UpdateFramebuffer(&frameR, &Graphics::Views[1].Pixels[0], Graphics::Views[1].Pitch);
        }
    }
    void Present() {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            C2D_TargetClear(RenderTargetTopScreenLeftEye, C2D_Color32(0, 0, 0, 0xFF));
            C2D_SceneBegin(RenderTargetTopScreenLeftEye);
            if (Game::State.EngineState == ENGINESTATE_VIDEO)
                VFrame_Draw(&frameVideo, FRAMEBUFFER_WIDTH / 2, FRAMEBUFFER_HEIGHT / 2, 0.5f);
            else
                VFrame_Draw(&frameL, FRAMEBUFFER_WIDTH / 2, FRAMEBUFFER_HEIGHT / 2, 0.5f);
        C3D_FrameEnd(0);

        // gfxFlushBuffers();
        // gfxSwapBuffers();
        // gspWaitForVBlank();
    }
    void Dispose() {
        y2rExit();

        osSetSpeedupEnable(false);
        gfxExit();
    }

    void UpdateTexture420(int width, int height, Uint8* pixelY, Uint8* pixelU, Uint8* pixelV, int strideY, int strideU, int strideV) {
        VFrame* vframe = &frameVideo;

        bool is_busy = true;
        bool drawbuf = !vframe->curbuf;

        C3D_Tex* wframe = &vframe->buff[drawbuf];

        Y2RU_StopConversion();

        while (is_busy)
            Y2RU_IsBusyConversion(&is_busy);

        Y2RU_SetInputFormat(INPUT_YUV420_INDIV_8);

        Y2RU_SetOutputFormat(OUTPUT_RGB_24);
        Y2RU_SetRotation(ROTATION_NONE);
        Y2RU_SetBlockAlignment(BLOCK_8_BY_8);
        Y2RU_SetTransferEndInterrupt(true);
        Y2RU_SetInputLineWidth(width);
        Y2RU_SetInputLines(height);
        Y2RU_SetStandardCoefficient(COEFFICIENT_ITU_R_BT_601_SCALING);
        Y2RU_SetAlpha(0xFF);

        Y2RU_SetSendingY(pixelY, width * height, width, strideY - width);
        Y2RU_SetSendingU(pixelU, (width / 2) * (height / 2), width / 2, strideU - (width >> 1));
        Y2RU_SetSendingV(pixelV, (width / 2) * (height / 2), width / 2, strideV - (width >> 1));

        Y2RU_SetReceiving(wframe->data, width * height * fmtGetBPP(wframe->fmt), width * 8 * fmtGetBPP(wframe->fmt), (Math::ToNextPOT(width) - width) * 8 * fmtGetBPP(wframe->fmt));
        Y2RU_StartConversion();

        // Wait until we are ready to present the frame
        Y2RU_GetTransferEndEvent(&y2rEvent);
        if (svcWaitSynchronization(y2rEvent, 6e7))
            puts("Y2R timed out"); // DEBUG

        vframe->curbuf = drawbuf;
        vframe->img.tex = wframe;
    }
    void UpdateTexture422(int width, int height, Uint8* pixelY, Uint8* pixelU, Uint8* pixelV, int strideY, int strideU, int strideV) {
        // Y2RU_SetInputFormat(INPUT_YUV422_INDIV_8);
    }
    void UpdateTexture444(int width, int height, Uint8* pixelY, Uint8* pixelU, Uint8* pixelV, int strideY, int strideU, int strideV) {
        // Y2RU_SetInputFormat(INPUT_YUV422_INDIV_8);
    }
}

// 3DS audio implementation.
// Uses NDSP.
namespace Audio {
    Thread      threadId;
    LightEvent  s_event;
    ndspWaveBuf s_waveBufs[3];
    int16_t*    s_audioBuffer = NULL;
    static const int THREAD_AFFINITY = -1;           // Execute thread on any core
    static const int THREAD_STACK_SZ = 32 * 1024;    // 32kB stack for audio thread

    #define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

    void audioCallback(void *const nul_) {
        (void)nul_;  // Unused

        if (!Game::Running)
            return;

        LightEvent_Signal(&s_event);
    }
    void audioThread(void *const opusFile_) {
        while (Game::Running) {
            if (Game::State.EngineState > 0) {
                for (size_t i = 0; i < ARRAY_SIZE(s_waveBufs); ++i) {
                    if (s_waveBufs[i].status != NDSP_WBUF_DONE)
                        continue;

                    RunPlaybacks((Sample*)s_waveBufs[i].data_pcm16);
                    s_waveBufs[i].nsamples = SAMPLE_READ;
                    ndspChnWaveBufAdd(0, &s_waveBufs[i]);
                    DSP_FlushDataCache(s_waveBufs[i].data_pcm16, SAMPLE_READ_LENGTH);
                }
            }

            // Wait for a signal that we're needed again before continuing,
            // so that we can yield to other things that want to run
            // (Note that the 3DS uses cooperative threading)
            LightEvent_Wait(&s_event);
        }
    }

    bool PlatformInit() {
        ndspInit();
        LightEvent_Init(&s_event, RESET_ONESHOT);

        // Setup NDSP
        ndspChnReset(0);
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
        ndspChnSetInterp(0, NDSP_INTERP_POLYPHASE);
        ndspChnSetRate(0, SAMPLE_RATE);
        ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

        // Allocate audio buffer
        const size_t bufferSize = SAMPLE_READ_LENGTH * ARRAY_SIZE(s_waveBufs);
        s_audioBuffer = (int16_t*)linearAlloc(bufferSize);
        if (!s_audioBuffer) {
            Diagnostics::SetError("Failed to allocate audio buffer");
            return false;
        }

        // Setup waveBufs for NDSP
        memset(&s_waveBufs, 0, sizeof(s_waveBufs));
        int16_t* buffer = s_audioBuffer;

        for (size_t i = 0; i < ARRAY_SIZE(s_waveBufs); ++i) {
            s_waveBufs[i].data_vaddr = buffer;
            s_waveBufs[i].status     = NDSP_WBUF_DONE;

            buffer += SAMPLE_READ_LENGTH / sizeof(int16_t);
        }

        ndspSetCallback(audioCallback, NULL);

        // Set the thread priority to the main thread's priority ...
        int32_t priority = 0x30;
        svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
        // ... then subtract 1, as lower number => higher actual priority ...
        priority -= 1;
        // ... finally, clamp it between 0x18 and 0x3F to guarantee that it's valid.
        priority = priority < 0x18 ? 0x18 : priority;
        priority = priority > 0x3F ? 0x3F : priority;

        // Start the thread, passing our opusFile as an argument.
        threadId = threadCreate(audioThread, NULL, THREAD_STACK_SZ, priority, THREAD_AFFINITY, false);

        StreamVolume = 0.5f;
        SoundFXVolume = 0.5f;
        return true;
    }
    void PlatformDispose() {
        LightEvent_Signal(&s_event);
        threadJoin(threadId, UINT64_MAX);
        threadFree(threadId);
        ndspChnReset(0);
        linearFree(s_audioBuffer);
        ndspExit();
    }

    bool Lock() {
        return true;
    }
    void Unlock() {

    }
}

// 3DS resource file reading implementation.
// Uses RomFS.
namespace Resources {
    const char* ResourceFolderPrefix = "romfs:/";

    bool PlatformInit() {
        Result rc = romfsInit();
        if (rc) {
            Diagnostics::SetError("Failed to initialize RomFS with code %08lX.", rc);
            return false;
        }
        return true;
    }
    void PlatformDispose() {
        romfsExit();
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
    void Poll() {
        u32 kPressed = hidKeysDown(); // Pressed
        u32 kDown = hidKeysHeld(); // Held
        u32 kUp = hidKeysUp();
        struct inputKeyPair {
            InputState* input;
            u32 hidKey;
        }
        inputKeyPairs[] = {
            { &PadInputs[0].Up, KEY_UP },
            { &PadInputs[0].Down, KEY_DOWN },
            { &PadInputs[0].Left, KEY_LEFT },
            { &PadInputs[0].Right, KEY_RIGHT },
            { &PadInputs[0].A, KEY_A },
            { &PadInputs[0].B, KEY_B },
            { &PadInputs[0].C, KEY_Y },
            { &PadInputs[0].X, KEY_X },
            { &PadInputs[0].Y, KEY_L },
            { &PadInputs[0].Z, KEY_R },
            { &PadInputs[0].Start, KEY_START },
            { &PadInputs[0].Select, KEY_SELECT },
        };

        for (size_t i = 0; i < sizeof(inputKeyPairs) / sizeof(inputKeyPairs[0]); i++) {
            inputKeyPairs[i].input->Down = !!(kDown & inputKeyPairs[i].hidKey);
            inputKeyPairs[i].input->Pressed = !!(kPressed & inputKeyPairs[i].hidKey);
            inputKeyPairs[i].input->Released = !!(kUp & inputKeyPairs[i].hidKey);
        }
    }
}

namespace Game {
    void SetWindowTitle(CString title) { }
}

namespace Clock {
    struct PlatformCounter {
        TickCounter tc;
    };

    void CounterStart(Counter* counter) {
        osTickCounterStart(&((PlatformCounter*)counter)->tc);
    }
    void CounterFinish(Counter* counter) {
        osTickCounterUpdate(&((PlatformCounter*)counter)->tc);
    }
    double CounterGetElapsed(Counter* counter) {
        return osTickCounterRead(&((PlatformCounter*)counter)->tc);
    }
}

namespace Threading {
    Thread* CreateThread(ThreadFunction function, void* opaque) {
        // Set the thread priority to the main thread's priority ...
        int32_t priority = 0x30;
        svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
        // ... then subtract 1, as lower number => higher actual priority ...
        priority -= 1;
        // ... finally, clamp it between 0x18 and 0x3F to guarantee that it's valid.
        priority = priority < 0x18 ? 0x18 : priority;
        priority = priority > 0x3F ? 0x3F : priority;

        return (Thread*)threadCreate((ThreadFunc)function, opaque, 32 * 1024, priority, -1, false);
    }
    void    DetachThread(Thread* thread) {
        threadDetach((::Thread)thread);
    }
    int     WaitThread(Thread* thread) {
        int result = (int)threadJoin((::Thread)thread, U64_MAX);
        threadFree((::Thread)thread);
        return result;
    }
}

int VideoDecodeThread(void*) {
    while (Game::Running) {
        if (Game::State.EngineState == ENGINESTATE_VIDEO) {
            Video::DecodeFrame(false);
            svcSleepThread(1000000000LL / 60);
        }
        else {
            svcSleepThread(1000000000LL / 15); // Sleep for a lot longer if no video is playing
        }
    }
    return 0;
}

int main(int argc, char** args) {
    Game::Running = true;

    if (Memory::Init()) {
        if (!Renderer::Init()) {
            Game::Running = false;
        }

        Settings::Init();
        Resources::Init();

        Services::Init();

        Game::Init();
        Graphics::Init();

        if (!Audio::Init())
            Game::Running = false;
    }
    else {
        Game::Running = false;
    }

    int FramesCounted = 0;
    const int FramesToCount = 30;
    const double FramesToCountMillis = 1000.0 * (FramesToCount / 60.0);
    TickCounter counterWholeFrame;
    TickCounter counterEvents;
    TickCounter counterGameRun;
    TickCounter counterTransfer;
    TickCounter counterPresent;
    double elapsedWholeFrame = 0.0;
    double elapsedEvents = 0.0;
    double elapsedGameRun = 0.0;
    double elapsedTransfer = 0.0;
    double elapsedPresent = 0.0;

    auto thread = Threading::CreateThread(VideoDecodeThread, NULL);
    if (!thread) {
        fprintf(stderr, "Could not create video thread: %s\n", Diagnostics::ErrorString);
    }

    // Main loop
    while (Game::Running) {
        osTickCounterStart(&counterWholeFrame);
        {
            // Poll events
            osTickCounterStart(&counterEvents);
            {
                Game::Running = aptMainLoop();

                hidScanInput(); // Keep this here

                u32 kPressed = hidKeysDown(); // Pressed
                if (kPressed & KEY_ZL) {
                    switch (Game::State.EngineState) {
                        case ENGINESTATE_FULLUPDATE_STEP:
                            Game::State.EngineState = ENGINESTATE_FULLUPDATE; break;
                        case ENGINESTATE_UNPAUSED_STEP:
                            Game::State.EngineState = ENGINESTATE_UNPAUSED; break;
                        case ENGINESTATE_PAUSED_STEP:
                            Game::State.EngineState = ENGINESTATE_PAUSED; break;
                        case ENGINESTATE_FULLUPDATE:
                            Game::State.EngineState = ENGINESTATE_FULLUPDATE_STEP; break;
                        case ENGINESTATE_UNPAUSED:
                            Game::State.EngineState = ENGINESTATE_UNPAUSED_STEP; break;
                        case ENGINESTATE_PAUSED:
                            Game::State.EngineState = ENGINESTATE_PAUSED_STEP; break;
                    }
                }
                if (kPressed & KEY_ZR) {
                    switch (Game::State.EngineState) {
                        case ENGINESTATE_FULLUPDATE:
                            Game::State.EngineState = ENGINESTATE_FULLUPDATE_STEP; break;
                        case ENGINESTATE_UNPAUSED:
                            Game::State.EngineState = ENGINESTATE_UNPAUSED_STEP; break;
                        case ENGINESTATE_PAUSED:
                            Game::State.EngineState = ENGINESTATE_PAUSED_STEP; break;
                    }
                    Game::StepForward = true;
                }

                u32 kDown = hidKeysHeld(); // Held
                if (kDown & KEY_SELECT) {
                    Game::Running = false;
                    break;
                }
            }
            osTickCounterUpdate(&counterEvents);

            // Run game
            osTickCounterStart(&counterGameRun);
            if (Game::State.EngineState != ENGINESTATE_VIDEO) {
                Game::Run();
            }
            osTickCounterUpdate(&counterGameRun);

            osTickCounterStart(&counterTransfer);
            {
                Renderer::TransferFrameBuffers();
            }
            osTickCounterUpdate(&counterTransfer);

            osTickCounterStart(&counterPresent);
            {
                Renderer::Present();
            }
            osTickCounterUpdate(&counterPresent);
        }
        osTickCounterUpdate(&counterWholeFrame);

        elapsedWholeFrame += osTickCounterRead(&counterWholeFrame);
        elapsedEvents += osTickCounterRead(&counterEvents);
        elapsedGameRun += osTickCounterRead(&counterGameRun);
        elapsedTransfer += osTickCounterRead(&counterTransfer);
        elapsedPresent += osTickCounterRead(&counterPresent);

        FramesCounted++;
        if (FramesCounted == FramesToCount) {
            printf("\x1b[10;1H"
                "FPS: %.1f                 \n"
                "Frame Time: % 8.1fms      \n"
                "Events:     % 8.1fms      \n"
                "GameRun:    % 8.1fms      \n"
                "Transfer:   % 8.1fms      \n"
                "Present:    % 8.1fms      \n",
                60 * FramesToCountMillis / elapsedWholeFrame,
                elapsedWholeFrame / FramesCounted,
                elapsedEvents / FramesCounted,
                elapsedGameRun / FramesCounted,
                elapsedTransfer / FramesCounted,
                elapsedPresent / FramesCounted);

            FramesCounted = 0;
            elapsedWholeFrame = 0.0;
            elapsedEvents = 0.0;
            elapsedGameRun = 0.0;
            elapsedTransfer = 0.0;
            elapsedPresent = 0.0;
        }
    }

    Audio::Dispose();

    Settings::Dispose();
    Services::Dispose();
    Renderer::Dispose();
    // Memory::FreeAll();
    Resources::Dispose();
    return 0;
}

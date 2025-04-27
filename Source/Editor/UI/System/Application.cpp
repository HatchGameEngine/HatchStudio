#include <UI/System/Application.hpp>

#include <Hatch/Memory.h>
#include <Hatch/Services.h>
#include <Hatch/IO/Stream.h>
#include <Hatch/IO/FileStream.h>

#include <Studio/Impl.hpp>

#include <UI/Controls/Form.hpp>
#include <UI/Graphics/Font.hpp>
#include <UI/Graphics/Renderer.hpp>

namespace UI {
    namespace System {
        namespace Application {
            namespace Font = UI::Graphics::Font;

            List<Form*> Forms;
            Form* BaseForm = NULL;

            bool CancelShortcuts = false;

            bool Init() {
                // Game Window
                if (!UI::Graphics::Renderer::Init())
                    return false;

                if (!Memory::Init())
                    return false;

                Services::Init();

                ::GameLinker::Init();

                Game::State.ViewCount = 1;
                ::Graphics::Init();

                return true;
            }
            void Dispose() {
                UI::Graphics::Renderer::Dispose();

                Services::Dispose();

                if (GameLinker::GameLogicSharedObject)
                    SDL_UnloadObject(GameLinker::GameLogicSharedObject);
            }

            Font::Face* LoadFontFaceFromFile(const char* path, int size) {
                Stream* stream = FileStream::New(path, FileStream::READ_ACCESS);
                if (stream) {
                    auto face = Font::LoadFontFace(stream, size);
                    stream->Close();
                    return face;
                }
                return NULL;
            }
            void LoadFonts() {
                UI::Graphics::Font::Arial[12] = LoadFontFaceFromFile("Resources_Editor/OpenSans.ttf", 16);
            }

            void RunFrame(bool update) {
                // Clear renderer & get size
                SDL_SetRenderDrawColor(UI::Graphics::Renderer::Renderer, 0, 0, 0, 255);
                int ret = SDL_RenderClear(UI::Graphics::Renderer::Renderer);
                if (ret != 0) {
                    return;
                }

                // SDL_GetRendererOutputSize(UI::Graphics::Renderer::Renderer, &UI::Graphics::Renderer::RendererW, &UI::Graphics::Renderer::RendererH);
                if (update)
                    SDL_GetWindowSize(UI::Graphics::Renderer::Window, &UI::Graphics::Renderer::RendererW, &UI::Graphics::Renderer::RendererH);

                for (int i = 0; i < Forms.Count(); i++) {
                    Form* form = Forms[i];

                    // Update form layout
                    ::Size size = form->Size;
                    // if (size.W >= UI::Graphics::Renderer::RendererW || size.H >= UI::Graphics::Renderer::RendererH)
                    if (form->IsDialog)
                        form->Location = { (UI::Graphics::Renderer::RendererW - size.W) / 2, (UI::Graphics::Renderer::RendererH - size.H) / 2 };

                    // Update container
                    if (update)
                        form->Update();

                    // Render container
                    if (form->IsDialog)
                        UI::Graphics::Renderer::DrawRect(0, 0, UI::Graphics::Renderer::RendererW, UI::Graphics::Renderer::RendererH, Color(0xFFFFFF, 0x80));
                    form->Render();
                }

                SDL_RenderPresent(UI::Graphics::Renderer::Renderer);
            }

            int EventFilter(void* data, SDL_Event* e) {
                switch (e->type) {
                case SDL_WINDOWEVENT:
                    switch (e->window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                        UI::Graphics::Renderer::RendererW = e->window.data1;
                        UI::Graphics::Renderer::RendererH = e->window.data2;

                        for (int i = Forms.Count() - 1; i >= 0; i--) {
                            Forms[i]->HandleSDLEvent(e);
                        }

                        Uint32 frameTime = SDL_GetTicks();
                        RunFrame(false);
                        frameTime = SDL_GetTicks() - frameTime;

                        // printf("took %u ms\n", frameTime);
                        break;
                    }
                    break;
                }

                return 1;
            }

            void Start(int argc, char** args, Form* startForm) {
                bool GameRunning = Init();
                if (!GameRunning)
                    return;

                LoadFonts();

                Forms.Add(startForm);

                SDL_SetEventFilter(EventFilter, NULL);

                BaseForm = startForm;
                BaseForm->MainForm = true;
                BaseForm->Load();
                BaseForm->Size = BaseForm->Size.Get();

                // Uint32 currentTime = SDL_GetTicks();
                // Uint32 nextTime = currentTime;
                Uint32 frameDur = 1000 / 30;

                int displayIndexOld = -1;

                // Main loop
                SDL_Event e;
                while (GameRunning) {
                    // Poll events
                    while (SDL_PollEvent(&e)) {
                        switch (e.type) {
                        case SDL_KEYDOWN:
                            // Handle shortcuts here (non-MacOS since it handles itself)
                            break;
                        case SDL_QUIT:
                            if (Forms.Count() > 0) {
                                Forms[0]->Close();
                                GameRunning = !Forms[0]->SignalClose;
                            }
                            else {
                                GameRunning = false;
                            }
                            break;
                        }

                        // Handle events on the topmost form
                        if (Forms.Count() > 0) {
                            Forms[Forms.Count() - 1]->HandleSDLEvent(&e);
                        }

                        // Remove any forms signaled for deletion
                        for (int i = Forms.Count() - 1; i >= 0; i--) {
                            auto form = Forms[i];
                            if (form->SignalClose) {
                                delete form;
                                Forms.RemoveAt(i);
                            }
                        }
                    }

                    // Get the current display's refresh rate
                    SDL_DisplayMode current;
                    int displayIndex = SDL_GetWindowDisplayIndex(UI::Graphics::Renderer::Window);
                    if (displayIndexOld != displayIndex) {
                        displayIndexOld = displayIndex;
                        if (SDL_GetCurrentDisplayMode(displayIndex, &current) == 0) {
                            frameDur = 1000 / current.refresh_rate;
                        }
                    }

                    // Measure frame time
                    Uint32 frameTime = SDL_GetTicks();
                    {
                        GameLinker::ServiceFuncs.Core.Run();
                        RunFrame(true);
                    }
                    frameTime = SDL_GetTicks() - frameTime;

                    // Sleep thread if we took less than the needed time
                    if (frameTime < frameDur) UI::Graphics::Renderer::Sleep((frameDur - frameTime) / 1000.0);
                }

                Dispose();
            }

            void ShowDialog(Form* dialog, DialogCallback callback) {
                dialog->ShowDialog(Forms[Forms.Count() - 1], callback);
                Forms.Add(dialog);
            }
            void Show(Form* form) {
                Forms.Add(form);
            }



            // https://gist.github.com/superwills/5815344
            #define BADCH   (int)'?'
            #define BADARG  (int)':'
            #define EMSG    ""
            int   opterr = 1, optind = 1, optopt, optreset;
            char* optarg;

            const int OPTIONS_END = -1;
            int ParseOptions(int nargc, char* const nargv[], const char* ostr) {
                // Parser position
                static const char* place = EMSG;
                const char* oli;

                // If reset flag is set or the parser position is 0
                if (optreset || !*place) {
                    optreset = 0;

                    // If the argument index is past the last argument, end parsing
                    if (optind >= nargc) {
                        place = EMSG;
                        return OPTIONS_END;
                    }

                    // Set the parser position
                    place = nargv[optind];

                    // If the first parsed character isn't '-', end parsing
                    if (*place != '-') {
                        place = EMSG;
                        return OPTIONS_END;
                    }

                    // If we've found a "--" instead of just a '-', end parsing
                    if (place[1] && *(++place) == '-') {
                        optind++;
                        place = EMSG;
                        return OPTIONS_END;
                    }
                }
                /* option letter okay? */
                if ((optopt = (int)*place++) == (int)':' || !(oli = strchr(ostr, optopt))) {
                    /*
                    * if the user didn't specify '-' as an option,
                    * assume it means -1.
                    */
                    if (optopt == (int)'-')
                        return OPTIONS_END;

                    if (!*place)
                        ++optind;

                    if (opterr && *ostr != ':')
                        printf("illegal option -- %c\n", optopt);

                    return (BADCH);
                }
                if (*++oli != ':') {                    /* don't need argument */
                    optarg = NULL;

                    if (!*place)
                        ++optind;
                }
                else {                                  /* need an argument */
                    if (*place)                     /* no white space */
                        optarg = (char*)place;
                    else if (nargc <= ++optind) {   /* no arg */
                        place = EMSG;
                        if (*ostr == ':')
                            return (BADARG);
                        if (opterr)
                            printf("option requires an argument -- %c\n", optopt);
                        return (BADCH);
                    }
                    else                            /* white space */
                        optarg = nargv[optind];
                    place = EMSG;
                    optind++;
                }
                return optopt;
            }
        }
    }
}

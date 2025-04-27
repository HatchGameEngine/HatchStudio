#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Game.h>

#include <Hatch/Hashing/MD5.h>
#include <Hatch/ImageFormats/GIF.h>
#include <Hatch/IO/FileStream.h>
#include <Hatch/IO/MemoryStream.h>
#include <Hatch/IO/ResourceStream.h>
#include <Hatch/Audio.h>
#include <Hatch/Classes.h>
#include <Hatch/Diagnostics.h>
#include <Hatch/Graphics.h>
#include <Hatch/GameLinker.h>
#include <Hatch/Input.h>
#include <Hatch/Math.h>
#include <Hatch/Resources.h>
#include <Hatch/Scene.h>
#include <Hatch/Threading.h>
#include <Hatch/Video.h>

#include <time.h>

namespace GIF {
    struct Node {
        Uint16 Key;
        struct Node* Children[];
    };
    struct Entry {
        Uint8  Used;
        Uint16 Length;
        Uint16 Prefix;
        Uint8  Suffix;
    };

    static inline void WriteCode(Stream* stream, int* offset, int* partial, Uint8* buffer, uint16_t key, int key_size) {
        int byte_offset, bit_offset, bits_to_write;
        byte_offset = *offset >> 3;
        bit_offset = *offset & 0x7;
        *partial |= ((uint32_t)key) << bit_offset;
        bits_to_write = bit_offset + key_size;
        while (bits_to_write >= 8) {
            buffer[byte_offset++] = *partial & 0xFF;
            if (byte_offset == 0xFF) {
                stream->WriteByte(0xFF);
                stream->WriteBytes(buffer, 0xFF);
                byte_offset = 0;
            }
            *partial >>= 8;
            bits_to_write -= 8;
        }
        *offset = (*offset + key_size) % (0xFF * 8);
    }

    static Node* NewNode(Uint16 key, int degree) {
        Node* node = (Node*)calloc(1, sizeof(*node) + degree * sizeof(Node*));
        if (node)
            node->Key = key;
        return node;
    }
    static Node* NewTree(int degree, int* nkeys) {
        Node *root = GIF::NewNode(0, degree);
        for (*nkeys = 0; *nkeys < degree; (*nkeys)++)
            root->Children[*nkeys] = GIF::NewNode(*nkeys, degree);
        *nkeys += 2;
        return root;
    }
    static void  FreeTree(Node* root, int degree) {
        if (!root)
            return;
        for (int i = 0; i < degree; i++)
            FreeTree(root->Children[i], degree);
        free(root);
    }

    Stream* stream = NULL;
    bool    SaveStart(const char* filename, int width, int height) {
        stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
        if (!stream)
            return false;

        stream->WriteByte('G');
        stream->WriteByte('I');
        stream->WriteByte('F');
        stream->WriteByte('8');
        stream->WriteByte('9');
        stream->WriteByte('a');

        stream->WriteUInt16((Uint16)width);
        stream->WriteUInt16((Uint16)height);

        Uint8 logicalScreenDesc = 0x70; // 8-bit color = 0x7, 5-bit color = 0x4
        if (false)
            logicalScreenDesc |= 0b10000000;
        stream->WriteByte(logicalScreenDesc);
        stream->WriteByte(0x00);
        stream->WriteByte(0x00); // stream->WriteByte((Uint8)this->TransparentColorIndex);

        // for (int p = 0; p < 256; p++) {
        //     stream->WriteByte(this->Colors[p] >> 16 & 0xFF);
        //     stream->WriteByte(this->Colors[p] >> 8 & 0xFF);
        //     stream->WriteByte(this->Colors[p] & 0xFF);
        // }

        stream->WriteByte('!');
        stream->WriteByte(0xFF);
        stream->WriteByte(0x0B);

        stream->WriteBytes((Uint8*)"NETSCAPE2.0", 11);

        stream->WriteByte(0x03);
        stream->WriteByte(0x01);

        stream->WriteUInt16(0x00); // Loop forver
        stream->WriteByte(0x00);
        return true;
    }
    void    SaveFrame(Pixel* palette, int colorCount, Uint8* data, int x, int y, int width, int pitch, int height) {
        int depth = 8; // 8 = 256 colors, 7 = 128 colors, 6 = 64, 5 = ...
        // Put Image
        Node* node;
        Node* root;
        Node* child;
        int nkeys, key_size, i, j;
        int degree = 1 << depth;

        Uint8 buffer[0x100];
        int offset = 0, partial = 0;

        stream->WriteByte(0x21);
        stream->WriteByte(0xF9);
        stream->WriteByte(0x04);
        stream->WriteByte(0x00); // Transparent
        stream->WriteUInt16(100 / 50); // 50 fps
        stream->WriteByte(0x00); // (Uint8)this->TransparentColorIndex
        stream->WriteByte(0x00);

        stream->WriteByte(0x2C);
        stream->WriteUInt16((Uint16)x); // X
        stream->WriteUInt16((Uint16)y); // Y
        stream->WriteUInt16((Uint16)width); // Width
        stream->WriteUInt16((Uint16)height); // Height

        // stream->WriteByte(0x00); // Packed field
        stream->WriteByte(0x80 | ((depth - 1) & 7)); // Packed field

        // stream->WriteBytes(palette, 256 * 2); // colorCount);
        for (int i = 0; i < 256; i++) {
            stream->WriteByte((palette[i].R) << 3);
            stream->WriteByte((palette[i].G) << 3);
            stream->WriteByte((palette[i].B) << 3);
        }

        stream->WriteByte((Uint8)depth); // Key size

        root = node = (Node*)NewTree(degree, &nkeys);
        key_size = depth + 1;
        WriteCode(stream, &offset, &partial, buffer, degree, key_size); /* clear code */
        for (i = x; i < height; i++) {
            for (j = y; j < width; j++) {
                Uint8 pixel = (Uint8)(data[i * pitch + j] & (degree - 1));
                child = node->Children[pixel];
                if (child) {
                    node = child;
                }
                else {
                    WriteCode(stream, &offset, &partial, buffer, node->Key, key_size);
                    if (nkeys < 0x1000) {
                        if (nkeys == (1 << key_size))
                            key_size++;
                        node->Children[pixel] = (Node*)NewNode(nkeys++, degree);
                    }
                    else {
                        WriteCode(stream, &offset, &partial, buffer, degree, key_size); /* clear code */
                        FreeTree(root, degree);
                        root = node = (Node*)NewTree(degree, &nkeys);
                        key_size = depth + 1;
                    }
                    node = root->Children[pixel];
                }
            }
        }
        WriteCode(stream, &offset, &partial, buffer, node->Key, key_size);
        WriteCode(stream, &offset, &partial, buffer, degree + 1, key_size); /* stop code */
        // end_key(gif);
        int byte_offset;
        byte_offset = offset >> 3;
        if (offset & 7)
            buffer[byte_offset++] = partial & 0xFF;
        stream->WriteByte((Uint8)byte_offset);
        stream->WriteBytes(buffer, byte_offset);
        stream->WriteByte(0);
        offset = partial = 0;
        //
        FreeTree(root, degree);
    }
    void    SaveFinish() {
        stream->WriteByte(0x3B);
        stream->Close();
    }

    int     GetBoundingBox(Pixel* current, Pixel* prev, int fbW, int fbPitch, int fbH, Uint16* x, Uint16* y, Uint16* w, Uint16* h) {
        int i, j, k, kLine;
        int left, right, top, bottom;
        left = fbW; right = 0;
        top = fbH; bottom = 0;
        kLine = 0;
        for (i = 0; i < fbH; i++) {
            k = kLine;
            for (j = 0; j < fbW; j++, k++) {
                if (current[k] != prev[k]) {
                    if (j < left)   left = j;
                    if (j > right)  right = j;
                    if (i < top)    top = i;
                    if (i > bottom) bottom = i;
                }
            }
            kLine += fbPitch;
        }
        if (left != fbW && top != fbH) {
            *x = left;
            *y = top;
            *w = right - left + 1;
            *h = bottom - top + 1;
            return 1;
        }
        return 0;
    }
}

namespace Game {
    GameState State;
    bool Running = false;
    int  UpdatesPerFrame = 1;
    bool StepForward = false;

    char Title[32];
    char Subtitle[32];
    char Version[32];

    char OverridenStartScene[256] = { 0 };

    char WindowTitle[256];

    // GIF recording
    struct GifRawFrame {
        Pixel  FrameBuffer[FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT];
        Uint8  GifFrameBuffer[FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT];
    };
    int  GifFramesEncoded = 0;
    bool GifIsEncoding = false;
    bool GifManualRecordOn = false;
    bool GifFlashbackRecord = true;
    int  GifFlashbackRecordLength = 50 * 5;
    int  GifFramesRecorded = 0;
    int  GifWidth, GifHeight;
    GifRawFrame* GifRaws = NULL;
    Sint16* GifPixelToPaletteIndexConvertArray;

    void UpdateWindowTitle() {
        sprintf(WindowTitle, "%s", Title);

        bool parenStarted = false;
        #define START_PAREN { if (!parenStarted) { \
            parenStarted = true; \
            strcat(WindowTitle, " ("); \
        } \
        else strcat(WindowTitle, ", "); }

        if (Resources::UseResourceFolder) {
            START_PAREN;
            strcat(WindowTitle, "using Resources folder");
        }
        if (false) {
            START_PAREN;
            strcat(WindowTitle, "using Modpack");
        }

        if (UpdatesPerFrame > 1) {
            START_PAREN;
            strcat(WindowTitle, "Fast Forward ON");
        }

        switch (0) {
            case 1:
                START_PAREN;
                strcat(WindowTitle, "Viewing Path A");
                break;
            case 2:
                START_PAREN;
                strcat(WindowTitle, "Viewing Path B");
                break;
            default:
                break;
        }

        if (State.EngineState == 4 || State.EngineState == 5) {
            START_PAREN;
            strcat(WindowTitle, "Frame Stepper ON");
        }

        // GIF recording
        if (GifManualRecordOn) {
            START_PAREN;
            if (!GifIsEncoding)
                strcat(WindowTitle, "GIF Recording ON");
            else
                sprintf(WindowTitle + strlen(WindowTitle), "GIF Encoding %d / %d", GifFramesEncoded, GifFramesRecorded);
        }

        if (parenStarted)
            strcat(WindowTitle, ")");

        SetWindowTitle(WindowTitle);
    }
    int  EncodeGif(void*) {
        // Finalize and encode
        Pixel palette[0x100];
        const int pxCount = GifWidth * GifHeight;

        int GifPitch = GifWidth;

        char gifFilename[256];

        time_t rawtime;
        struct tm* timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);

        int hour = timeinfo->tm_hour, minute = timeinfo->tm_min, second = timeinfo->tm_sec;
        sprintf(gifFilename, "%s %d-%02d-%02d %02d-%02d-%02d.gif", "HatchLite", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, hour, minute, second);

        GifFramesEncoded = 0;
        UpdateWindowTitle();

        GIF::SaveStart(gifFilename, GifWidth, GifHeight);

        int lastIndex = -1;
        int lastFrame = -1;
        for (int i = 0; i < GifFramesRecorded; i++) {
            if (i * 60 / 50 != lastIndex)
                lastIndex = i * 60 / 50;
            else continue;

            Uint8 colorReduction = 0x1F; // 0x1F: None, 0x1E: A bit, ...
            Uint8 reductionStep = 0;

            while (reductionStep < 3) {
                Uint16 pixelFilter = (colorReduction << 10 | colorReduction << 5 | colorReduction) << 1;
                int colorCount = 0;
                memset(GifPixelToPaletteIndexConvertArray, 0xFF, 0x10000 * sizeof(Sint16));

                Pixel* px = &GifRaws[i].FrameBuffer[0];
                Uint8* pxOut = &GifRaws[i].GifFrameBuffer[0];
                for (int p = 0; p < pxCount; p++) {
                    Uint16 filteredPx = (*px) & pixelFilter;
                    if (GifPixelToPaletteIndexConvertArray[filteredPx] == -1) {
                        if (colorCount == 256)
                            goto ReduceColors;

                        GifPixelToPaletteIndexConvertArray[filteredPx] = colorCount;
                        palette[colorCount] = filteredPx;
                        colorCount++;
                    }
                    *pxOut = (Uint8)GifPixelToPaletteIndexConvertArray[filteredPx];
                    px++;
                    pxOut++;
                }
                break;

            ReduceColors:
                reductionStep++;
                colorReduction = (colorReduction >> reductionStep) << reductionStep;
            }

            Uint16 x = 0;
            Uint16 y = 0;
            Uint16 w = GifWidth;
            Uint16 h = GifHeight;
            if (lastFrame > -1) {
                if (!GIF::GetBoundingBox(GifRaws[i].FrameBuffer, GifRaws[lastFrame].FrameBuffer, GifWidth, GifPitch, GifHeight, &x, &y, &w, &h)) {
                    x = y = 0;
                    w = h = 1;
                }
            }

            // Add frame
            GIF::SaveFrame(palette, 0x100, &GifRaws[i].GifFrameBuffer[0], 0, 0, GifWidth, GifPitch, GifHeight);

            lastFrame = i;

            GifFramesEncoded = i;
            UpdateWindowTitle();
        }

        GIF::SaveFinish();

        free(GifRaws);
        free(GifPixelToPaletteIndexConvertArray);
        GifRaws = NULL;
        GifManualRecordOn = false;
        GifIsEncoding = false;
        UpdateWindowTitle();

        return 0;
    }

    void RunTimer() {
        if (State.TimerActive) {
            State.TimerTicks += 100;
            if (State.TimerTicks >= 6000) {
                State.TimerTicks -= 6000;
                if (++State.TimerSeconds >= 60) {
                    State.TimerSeconds = 0;
                    if (++State.TimerMinutes >= 60)
                        State.TimerMinutes = 0;
                }
            }
            State.TimerCentiseconds = State.TimerTicks / 60;
        }
    }
    void UpdateParallaxes() {
        Layer* layer = Scene::Layers;
        for (int l = 0; l < MAX_LAYERS; l++) {
            if (layer->Tiles) {
                layer->ScrollOffset += layer->ConstantScroll;
                Parallax* info = layer->ParallaxInfos;
                for (int i = 0; i < layer->ParallaxInfoCount; i++) {
                    info->ParallaxOffset.Full += info->ConstantParallax.Full;
                    info++;
                }
            }
            layer++;
        }
    }

    bool LoadConfig() {
        Stream* stream = ResourceStream::New("Game/" "GameConfig.bin");
        if (stream) {
            char streamStringBuffer[256];
            Uint32 magic = stream->ReadUInt32();
            if (magic == 0x00474643) {
                stream->ReadHeaderedString(Title);
                stream->ReadHeaderedString(Subtitle);
                stream->ReadHeaderedString(Version);

                State.CategoryStartIndex = stream->ReadByte();
                int startSceneIndex = stream->ReadUInt16();

                // Add global Classes
                Scene::GlobalClassIndexCount = 0;
                int globalClassCount = stream->ReadByte();
                for (int i = 0; i < globalClassCount; i++) {
                    stream->ReadHeaderedString(streamStringBuffer);
                    Hash  globalClassHash = MD5_HashString(streamStringBuffer);

                    auto oldClassCount = Scene::GlobalClassIndexCount;
                    for (int c = 0; c < GameLinker::ClassCount; c++) {
                        if (globalClassHash == GameLinker::ClassList[c].Name) {
                            Scene::GlobalClassIndexList[Scene::GlobalClassIndexCount++] = c;
                            break;
                        }
                    }

                    if (oldClassCount == Scene::GlobalClassIndexCount) {
                        printf("Could not find logic for class '%s'.\n", streamStringBuffer);
                    }
                }

                // Load palettes
                Color color;
                for (int i = 0; i < MAX_PALETTE_COUNT; i++) {
                    // Palette Set
                    int bitmap = Scene::UsedGameConfigPaletteLines[i] = stream->ReadUInt16();
                    for (int paletteLine = 0; paletteLine < 16; paletteLine++) {
                        if ((bitmap & (1 << paletteLine)) != 0) {
                            for (int d = 0; d < 16; d++) {
                                color.R = stream->ReadByte();
                                color.G = stream->ReadByte();
                                color.B = stream->ReadByte();

                                Scene::GameConfigPalette[i][(paletteLine << 4) | d] = color;
                            }
                        }
                    }
                }

                // Load sound effects
                int wavConfigCount = stream->ReadByte();
                for (int i = 0; i < wavConfigCount; i++) {
                    stream->ReadHeaderedString(streamStringBuffer);
                    Audio::LoadSoundFX(streamStringBuffer, stream->ReadByte(), UNLOAD_GAME_END);
                }

                // Load scene lists & categories
                int totalScenes = stream->ReadUInt16();
                Memory::Alloc(&State.Scenes, totalScenes * sizeof(SceneInfo), Memory::MEMPOOL_STAGE, false);

                State.CategoryCount = stream->ReadByte();
                Memory::Alloc(&State.Categories, State.CategoryCount * sizeof(SceneCategory), Memory::MEMPOOL_STAGE, false);

                State.CurrentSceneIndex = 0;
                for (int c = 0; c < State.CategoryCount; c++) {
                    SceneCategory* category = &State.Categories[c];

                    stream->ReadHeaderedString(category->Name);
                    category->NameHash = MD5_HashString(category->Name);

                    category->FirstSceneIndex = State.CurrentSceneIndex;
                    category->SceneCount = stream->ReadByte();

                    for (int s = 0; s < category->SceneCount; s++) {
                        SceneInfo* scene = &State.Scenes[State.CurrentSceneIndex];
                        stream->ReadHeaderedString(scene->Name);
                        scene->NameHash = MD5_HashString(scene->Name);

                        stream->ReadHeaderedString(scene->Zone);
                        stream->ReadHeaderedString(scene->SceneID);

                        scene->Filter = stream->ReadByte();
                        if (!scene->Filter)
                            scene->Filter = 0xFF;

                        State.CurrentSceneIndex++;
                    }

                    if (State.CurrentSceneIndex)
                        category->LastSceneIndex = State.CurrentSceneIndex - 1;
                    else
                        category->LastSceneIndex = 0;
                }

                State.CurrentSceneIndex = State.Categories[State.CategoryStartIndex].FirstSceneIndex + startSceneIndex;

                Scene::CurrentStage[0] = '\0';
            }
            else {
                fprintf(stderr, "Invalid magic for file '%s'!\n", "GameConfig.bin");
                stream->Close();
                return false;
            }
            stream->Close();
        }
        else {
            fprintf(stderr, "Could not open '%s'!\n", "GameConfig.bin");
            return false;
        }

        return true;
    }
    bool Init() {
        ZERO_OUT(State);

        // Clear Class arrays and count here
        // Init game state
        Math::SetupMathTables();
        GameLinker::Init();
        GameLinker::Load();

        if (!LoadConfig())
            return false;
        return true;
    }
    void Run() {
        int oldSceneIndex = State.CurrentSceneIndex;

        Scene::SearchStackTop = Scene::SearchStack;

        switch (State.EngineState) {
            case ENGINESTATE_SCENELOAD:
                UpdateWindowTitle();

                Scene::LoadStage(NULL);
                Scene::LoadScene(NULL);
                Scene::StartScene();

                Input::Poll();

                // Update All Entities
                Scene::Update();
                // Graphics::DrawAll();
                break;
            case ENGINESTATE_UNPAUSED:
            case ENGINESTATE_UNPAUSED_STEP:
                Input::Poll();
                if (State.EngineState != ENGINESTATE_UNPAUSED_STEP || StepForward) {
                    // Update All Entities
                    for (int i = 0; i < UpdatesPerFrame; i++) {
                        RunTimer();
                        Scene::Update();
                        UpdateParallaxes();
                    }
                    Graphics::DrawAll();

                    StepForward = false;
                }
                break;
            case ENGINESTATE_PAUSED:
            case ENGINESTATE_PAUSED_STEP:
                Input::Poll();
                if (State.EngineState != ENGINESTATE_PAUSED_STEP || StepForward) {
                    // Update All Entities
                    for (int i = 0; i < UpdatesPerFrame; i++) {
                        // RunTimer();
                        Scene::Update();
                        // UpdateParallaxes();
                    }
                    Graphics::DrawAll();
                }
                break;
            case ENGINESTATE_FULLUPDATE:
            case ENGINESTATE_FULLUPDATE_STEP:
                Input::Poll();
                if (State.EngineState != ENGINESTATE_FULLUPDATE_STEP || StepForward) {
                    // Update All Entities
                    for (int i = 0; i < UpdatesPerFrame; i++) {
                        // RunTimer();
                        Scene::Update();
                        UpdateParallaxes();
                    }
                    Graphics::DrawAll();

                    StepForward = false;
                }
                break;
            case ENGINESTATE_STEP:
                Input::Poll();
                Graphics::DrawAll();
                break;
            case ENGINESTATE_VIDEO:
                Input::Poll();
                Video::DecodeFrame(false);
                break;
        }

        if (Input::PadInputs[0].Start.Down && Input::PadInputs[0].A.Pressed) {
            State.CurrentSceneIndex++;
            State.EngineState = ENGINESTATE_SCENELOAD;
            if (State.CurrentSceneIndex > 65)
                State.CurrentSceneIndex = 9;
        }

        if (State.CurrentSceneIndex != oldSceneIndex) {
            Game::OverridenStartScene[0] = 0;
        }

        // Debug GIF recorder
        // NOTE: Only records the first View
        if (GifManualRecordOn) {
            View* view = &Graphics::Views[0];

            if (!GifRaws) {
                GifRaws = (GifRawFrame*)malloc(GifFlashbackRecordLength * sizeof(GifRawFrame));
                GifPixelToPaletteIndexConvertArray = (Sint16*)malloc(0x10000 * sizeof(Sint16));
                GifFramesRecorded = 0;
            }

            if (!GifRaws)
                return;

            if (GifFramesRecorded < GifFlashbackRecordLength) {
                // Copy Pixels line by line
                for (int i = 0, src = 0, dst = 0; i < view->Height; i++) {
                    memcpy(&GifRaws[GifFramesRecorded].FrameBuffer[dst], &Graphics::Views[0].Pixels[src], view->Width * sizeof(Pixel));
                    dst += view->Width;
                    src += view->Pitch;
                }
                GifFramesRecorded++;
            }
            else if (!GifIsEncoding) {
                GifWidth = view->Width;
                GifHeight = view->Height;

                GifIsEncoding = true;
                Threading::DetachThread(Threading::CreateThread(EncodeGif, NULL));
            }
        }
    }
}

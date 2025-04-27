#pragma once

#define USE_STB_VORBIS

#ifdef USE_STB_VORBIS
    #include <Hatch/Libraries/stb_vorbis.h>
#elif defined(OGG_HEADER)
    #include OGG_HEADER
#elif defined(OGG_USE_TREMOR)
    #include <tremor/ivorbisfile.h>
#else
    #include <vorbis/vorbisfile.h>
#endif

namespace Audio {
    // Common
    typedef struct { Sint16 L; Sint16 R; } Sample;
    static const int SAMPLE_RATE = 44100;
    static const int SAMPLE_CHANNELS = 2;
    static const int SAMPLE_READ = 0x800;
    static const int SAMPLE_READ_LENGTH = SAMPLE_READ * sizeof(Sample);

    extern int SAMPLE_RATE_MOD;

    enum   AudioPlaybackStates {
        PLAYBACK_NONE,
        PLAYBACK_STREAM,
        PLAYBACK_SOUNDFX,
        PLAYBACK_STREAM_PAUSED,
        PLAYBACK_SOUNDFX_PAUSED,
        PLAYBACK_STREAM_LOADING,
    };
    struct AudioStream {
        char           FileName[64];
        Uint8*         FileBuffer;
        Uint8*         FileBufferHead;
        size_t         FileSize;
        #ifdef USE_STB_VORBIS
        stb_vorbis*    VorbisSTB;
        #else
        OggVorbis_File VorbisFile;
        int            VorbisBitstream;
        #endif
        int            StartAtSampleIndex;
        Sample*        SampleBuffer;
    };
    struct AudioPlayback {
        int     State;
        int     Index; // Sound Index for SoundFX, Stream Index for Streams
        int     Speed;
        float   Panning;
        float   Volume;
        int     LoopSampleIndex;
        int     SampleIndex;
        int     SampleCount;
        Sample* SampleBuffer;
        int     SoundPlayIndex;
        int     Priority;
    };

    // WAV reading
    struct WavHeader {
        /* RIFF Chunk Descriptor */
        Uint32        MagicRIFF;        // RIFF Header Magic header
        Uint32        ChunkSize;      // RIFF Chunk Size
        Uint32        MagicWAVE;        // WAVE Header
        /* "fmt" sub-chunk */
        Uint32        MagicFMT;
        Uint32        FMTSize;        // Size of the fmt chunk
        Uint16        AudioFormat;    // Audio format 1=PCM,6=mulaw,7=alaw,     257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
        Uint16        Channels;
        Uint32        Frequency;
        Uint32        BytesPerSecond;
        Uint16        BlockAlign;
        Uint16        BitsPerSample;
        /* "data" sub-chunk */
        Uint32        MagicDATA; // "data"  string
        Uint32        DATASize;  // Sampled data length
        Uint8         OverflowBuffer[64];
    };

    extern AudioStream   Streams[MAX_AUDIO_STREAMS];
    extern AudioPlayback Playbacks[MAX_AUDIO_PLAYBACKS];
    extern float         StreamVolume;
    extern float         SoundFXVolume;

    void     ReadStreamSamples(AudioPlayback* playback);
    void     RunPlaybacks(Sample* stream);

    bool     Init();
    void     Dispose();

    // Streams
    int      LoadStream(void* playbackPtr);
    int      PlayStream(CString filename, int streamIndex, int playbackIndex, int startAtSampleIndex, int loopAtSampleIndex);
    // SoundFX
    Resource LoadSoundFX(CString filename, Uint8 maxConcurrentPlay, int unloadPolicy);
    int      PlaySoundFX(Resource sound, int loopAtSampleIndex, Uint8 priority);
    void     StopSoundFX(Resource sound);
    bool     IsSoundFXPlaying(Resource sound);
    // General playback
    void     PlaybackAlter(int playbackIndex, float volume, float panning, float speed);
    bool     PlaybackIsValid(int playbackIndex);
    int      PlaybackGetSamplePosition(int playbackIndex);
    void     PlaybackStop(int playbackIndex);
    void     PlaybackPause(int playbackIndex);
    void     PlaybackResume(int playbackIndex);
    void     PlaybackPauseAll();
    void     PlaybackResumeAll();
    void     PlaybackResumeAll();
    double   GetPlaybackClock(int playbackIndex);

    // Implementations
    bool     PlatformInit();
    void     PlatformDispose();
    bool     Lock();
    void     Unlock();
}

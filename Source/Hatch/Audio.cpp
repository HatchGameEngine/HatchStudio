#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Audio.h>

#include <Hatch/Hashing/MD5.h>
#include <Hatch/IO/ResourceStream.h>
#include <Hatch/Diagnostics.h>
#include <Hatch/Game.h>
#include <Hatch/Memory.h>
#include <Hatch/Resources.h>
#include <Hatch/Settings.h>
#include <Hatch/Threading.h>

namespace Audio {
    // Statics
    AudioStream   Streams[MAX_AUDIO_STREAMS];
    AudioPlayback Playbacks[MAX_AUDIO_PLAYBACKS];

    float         StreamVolume = 1.0f;
    float         SoundFXVolume = 1.0f;

    int SAMPLE_RATE_MOD = 0x100;

    // Functions
    void   ReadStreamSamples(AudioPlayback* playback) {
        // Samples are interleaved.
        char*  buffer = (char*)playback->SampleBuffer;
        int    bufferSize = playback->SampleCount * sizeof(Sample);

        AudioStream* stream = &Streams[playback->Index];
        while (bufferSize > 0) {
            if (!Game::Running)
                break;

        #ifdef USE_STB_VORBIS
            auto readLength = stb_vorbis_get_samples_short_interleaved(stream->VorbisSTB, 2, (short*)buffer, bufferSize / sizeof(short)) * sizeof(Sample);
            if (readLength <= 0) {
                if (playback->LoopSampleIndex >= 0) {
                    stb_vorbis_seek_frame(stream->VorbisSTB, playback->LoopSampleIndex);
                    continue;
                }
                else {
                    // Stop playback
                    playback->State = PLAYBACK_NONE;
                    playback->Index = -1;

                    // Fill rest of buffer with silence
                    memset(playback->SampleBuffer, 0, bufferSize);
                    readLength = bufferSize;
                }
            }
        #else
            auto readLength = ov_read(&stream->VorbisFile, buffer, bufferSize,
                #ifndef OGG_USE_TREMOR
                0, 2, 1,
                #endif
                &stream->VorbisBitstream);
            if (readLength <= 0) {
                switch (readLength) {
                case OV_HOLE:
                    printf("OV_HOLE\n");
                    memset(playback->SampleBuffer, 0, bufferSize);
                    readLength = bufferSize;
                    return;
                case OV_EBADLINK:
                    printf("OV_EBADLINK\n");
                    memset(playback->SampleBuffer, 0, bufferSize);
                    readLength = bufferSize;
                    return;
                case OV_EINVAL:
                    printf("OV_EINVAL\n");
                    memset(playback->SampleBuffer, 0, bufferSize);
                    readLength = bufferSize;
                    return;
                    // End of file
                case 0:
                    if (playback->LoopSampleIndex >= 0) {
                        ov_pcm_seek(&stream->VorbisFile, playback->LoopSampleIndex);
                        continue;
                    }
                    else {
                        // Stop playback
                        playback->State = PLAYBACK_NONE;
                        playback->Index = -1;

                        // Fill rest of buffer with silence
                        memset(playback->SampleBuffer, 0, bufferSize);
                        readLength = bufferSize;
                    }
                    break;
                }
                break;
            }
        #endif
            buffer += readLength;
            bufferSize -= readLength;
        }
    }
    void   RunPlaybacks(Sample* stream) {
        Lock();

        Sample* outStream;
        Sample* outStreamEnd;
        Sample* inStream;
        Sample* inStreamEnd;
        float volume, panning, volumeL, volumeR;
        int advance, advanceAccumulator, speed;

        bool firstMix = true;
        AudioPlayback* playback = &Playbacks[0];
        for (int p = 0; p < MAX_AUDIO_PLAYBACKS; p++) {
            outStream = stream;
            outStreamEnd = stream + SAMPLE_READ;

            if (playback->State) {
                inStream = &playback->SampleBuffer[playback->SampleIndex];
                inStreamEnd = &playback->SampleBuffer[playback->SampleCount];

                advance = 0;
                advanceAccumulator = 0;
                speed = (playback->Speed * SAMPLE_RATE_MOD) >> 8;
                panning = playback->Panning;

                switch (playback->State) {
                case PLAYBACK_STREAM:
                    volume = playback->Volume * StreamVolume;
					volumeL = volume; volumeR = volume;
                    if (panning < 0.f)
                        volumeR = (1.0f + panning) * volume;
                    else
                        volumeL = (1.0f - panning) * volume;

                    if (firstMix) {
                        while (outStream < outStreamEnd) {
                            advanceAccumulator += speed;
                            advance = advanceAccumulator >> 16;
                            advanceAccumulator &= 0xFFFF;

                            outStream->L = (Sint16)(inStream->L * volumeL);
                            outStream->R = (Sint16)(inStream->R * volumeR);
                            inStream = &inStream[advance];
                            outStream++;

                            // If we've run out of samples, load some more.
                            if (inStream >= inStreamEnd) {
                                inStream = &playback->SampleBuffer[inStream - inStreamEnd];
                                ReadStreamSamples(playback);
                            }
                        }
                    }
                    else {
                        while (outStream < outStreamEnd) {
                            advanceAccumulator += speed;
                            advance = advanceAccumulator >> 16;
                            advanceAccumulator &= 0xFFFF;

                            outStream->L += (Sint16)(inStream->L * volumeL);
                            outStream->R += (Sint16)(inStream->R * volumeR);
                            inStream = &inStream[advance];
                            outStream++;

                            // If we've run out of samples, load some more.
                            if (inStream >= inStreamEnd) {
                                inStream = &playback->SampleBuffer[inStream - inStreamEnd];
                                ReadStreamSamples(playback);
                            }
                        }
                    }
                    break;
                case PLAYBACK_SOUNDFX:
                    volume = playback->Volume * SoundFXVolume;
					volumeL = volume; volumeR = volume;
                    if (panning < 0.f)
                        volumeR = (1.0f + panning) * volume;
                    else
                        volumeL = (1.0f - panning) * volume;

                    if (firstMix) {
                        while (outStream < outStreamEnd) {
                            advanceAccumulator += speed;
                            advance = advanceAccumulator >> 16;
                            advanceAccumulator &= 0xFFFF;

                            outStream->L = (Sint16)(inStream->L * volumeL);
                            outStream->R = (Sint16)(inStream->R * volumeR);
                            inStream = &inStream[advance];
                            outStream++;

                            // If we've run out of samples,
                            if (inStream >= inStreamEnd) {
                                // If we have no loop sample set,
                                if (playback->LoopSampleIndex < 0) {
                                    // Stop playback
                                    playback->State = PLAYBACK_NONE;
                                    playback->Index = -1;

                                    // Fill with silence
                                    while (outStream < outStreamEnd) {
                                        outStream->L = 0;
                                        outStream->R = 0;
                                        outStream++;
                                    }
                                }
                                // Otherwise,
                                else {
                                    // Set sample buffer to loop sample + any additional overread
                                    inStream = &playback->SampleBuffer[playback->LoopSampleIndex + inStream - inStreamEnd];
                                }
                            }
                        }
                    }
                    else {
                        while (outStream < outStreamEnd) {
                            advanceAccumulator += speed;
                            advance = advanceAccumulator >> 16;
                            advanceAccumulator &= 0xFFFF;

                            outStream->L += (Sint16)(inStream->L * volumeL);
                            outStream->R += (Sint16)(inStream->R * volumeR);
                            inStream = &inStream[advance];
                            outStream++;

                            // If we've run out of samples,
                            if (inStream >= inStreamEnd) {
                                // If we have no loop sample set,
                                if (playback->LoopSampleIndex < 0) {
                                    // Stop playback
                                    playback->State = PLAYBACK_NONE;
                                    playback->Index = -1;

                                    // Since this isn't first mix, do nothing
                                    outStream = outStreamEnd;
                                }
                                // Otherwise,
                                else {
                                    // Set sample buffer to loop sample + any additional overread
                                    inStream = &playback->SampleBuffer[playback->LoopSampleIndex + inStream - inStreamEnd];
                                }
                            }
                        }
                    }
                    break;
                }

                playback->SampleIndex = (int)(inStream - playback->SampleBuffer);

                firstMix = false;
            }
            playback++;
        }

        // If first mix never happened,
        if (firstMix) {
            outStream = stream;
            outStreamEnd = stream + SAMPLE_READ;
            // Fill with silence
            while (outStream < outStreamEnd) {
                outStream->L =
                outStream->R = 0;
                outStream++;
            }
        }

        Unlock();
    }

    #ifndef USE_STB_VORBIS
    size_t ogg_read_func(void* ptr, size_t size, size_t nmemb, void* datasource) {
        AudioStream* stream = (AudioStream*)datasource;
        size_t remaining = stream->FileSize - (stream->FileBufferHead - stream->FileBuffer);
        if (!remaining)
            return 0;

        size_t readSize = M_MIN(size * nmemb, remaining);
        memcpy(ptr, stream->FileBufferHead, readSize);
        stream->FileBufferHead += readSize;
        return readSize;
    }
    int    ogg_seek_func(void* datasource, ogg_int64_t offset, int whence) {
        AudioStream* stream = (AudioStream*)datasource;
        switch (whence) {
        case SEEK_SET:
            stream->FileBufferHead = stream->FileBuffer + offset; break;
        case SEEK_CUR:
            stream->FileBufferHead = stream->FileBufferHead + offset; break;
        case SEEK_END:
            stream->FileBufferHead = stream->FileBuffer + stream->FileSize + offset; break;
        }
        return 0;
    }
    int    ogg_close_func(void* datasource) {
        return 0;
    }
    long   ogg_tell_func(void* datasource) {
        AudioStream* stream = (AudioStream*)datasource;
        return (long)(stream->FileBufferHead - stream->FileBuffer);
    }
    #endif

    // Streams
    int      LoadStream(void* playbackPtr) {
        AudioPlayback* playback = (AudioPlayback*)playbackPtr;
		if (playback->State == PLAYBACK_STREAM)
			return 0;

        AudioStream* stream = &Streams[playback->Index];

        Stream* resStream = ResourceStream::New(stream->FileName);
        if (!resStream) {
            playback->State = PLAYBACK_NONE;
            return 0;
        }
        stream->FileSize = resStream->Length();

        if (!Memory::Alloc(&stream->FileBuffer, stream->FileSize, Memory::MEMPOOL_MUSIC, false)) {
            Diagnostics::SetError("Could not allocate memory for file buffer of audio stream!");
            playback->State = PLAYBACK_NONE;
            resStream->Close();
            return 0;
        }

        resStream->ReadBytes(stream->FileBuffer, stream->FileSize);
        resStream->Close();

        stream->FileBufferHead = stream->FileBuffer;

#ifdef USE_STB_VORBIS
        if (stream->VorbisSTB) {
            stb_vorbis_close(stream->VorbisSTB);
        }

        int error;
        stream->VorbisSTB = stb_vorbis_open_memory(stream->FileBuffer, (int)stream->FileSize, &error, NULL);
        if (!stream->VorbisSTB) {
            Diagnostics::SetError("Could not open Vorbis stream for %s!", stream->FileName);

            switch (error) {
            case VORBIS_need_more_data:
                printf("%s\n", "VORBIS_need_more_data");
                break;
            case VORBIS_invalid_api_mixing:
                printf("%s\n", "VORBIS_invalid_api_mixing");
                break;
            case VORBIS_outofmem:
                printf("%s\n", "VORBIS_outofmem");
                break;
            case VORBIS_feature_not_supported:
                printf("%s\n", "VORBIS_feature_not_supported");
                break;
            case VORBIS_too_many_channels:
                printf("%s\n", "VORBIS_too_many_channels");
                break;
            case VORBIS_file_open_failure:
                printf("%s\n", "VORBIS_file_open_failure");
                break;
            case VORBIS_seek_without_length:
                printf("%s\n", "VORBIS_seek_without_length");
                break;
            case VORBIS_unexpected_eof:
                printf("%s\n", "VORBIS_unexpected_eof");
                break;
            case VORBIS_seek_invalid:
                printf("%s\n", "VORBIS_seek_invalid");
                break;
            case VORBIS_invalid_setup:
                printf("%s\n", "VORBIS_invalid_setup");
                break;
            case VORBIS_invalid_stream:
                printf("%s\n", "VORBIS_invalid_stream");
                break;
            case VORBIS_missing_capture_pattern:
                printf("%s\n", "VORBIS_missing_capture_pattern");
                break;
            case VORBIS_invalid_stream_structure_version:
                printf("%s\n", "VORBIS_invalid_stream_structure_version");
                break;
            case VORBIS_continued_packet_flag_invalid:
                printf("%s\n", "VORBIS_continued_packet_flag_invalid");
                break;
            case VORBIS_incorrect_stream_serial_number:
                printf("%s\n", "VORBIS_incorrect_stream_serial_number");
                break;
            case VORBIS_invalid_first_page:
                printf("%s\n", "VORBIS_invalid_first_page");
                break;
            case VORBIS_bad_packet_type:
                printf("%s\n", "VORBIS_bad_packet_type");
                break;
            case VORBIS_cant_find_last_page:
                printf("%s\n", "VORBIS_cant_find_last_page");
                break;
            case VORBIS_seek_failed:
                printf("%s\n", "VORBIS_seek_failed");
                break;
            case VORBIS_ogg_skeleton_not_supported:
                printf("%s\n", "VORBIS_ogg_skeleton_not_supported");
                break;
            }

            playback->State = PLAYBACK_NONE;
            return 0;
        }

        if (stream->StartAtSampleIndex)
            stb_vorbis_seek(stream->VorbisSTB, stream->StartAtSampleIndex);
#else
        int ov_result;

        ov_callbacks callbacks;
        callbacks.read_func = ogg_read_func;
        callbacks.seek_func = ogg_seek_func;
        callbacks.tell_func = ogg_tell_func;
        callbacks.close_func = ogg_close_func;

        if ((ov_result = ov_open_callbacks(stream, &stream->VorbisFile, NULL, 0, callbacks)) != 0) {
            switch (ov_result) {
            case OV_EREAD:
                Diagnostics::SetError("A read from media returned an error."); break;
            case OV_ENOTVORBIS:
                Diagnostics::SetError("Bitstream does not contain any Vorbis data."); break;
            case OV_EVERSION:
                Diagnostics::SetError("Vorbis version mismatch."); break;
            case OV_EBADHEADER:
                Diagnostics::SetError("Invalid Vorbis bitstream header."); break;
            case OV_EFAULT:
                Diagnostics::SetError("Internal logic fault; indicates a bug or heap/stack corruption."); break;
            default:
                Diagnostics::SetError("Resource is not valid Vorbis stream!"); break;
            }
            playback->State = PLAYBACK_NONE;
            return 0;
        }

        if (stream->StartAtSampleIndex)
            ov_pcm_seek(&stream->VorbisFile, stream->StartAtSampleIndex);
#endif

        if (!Memory::Alloc(&stream->SampleBuffer, SAMPLE_READ_LENGTH, Memory::MEMPOOL_MUSIC, false)) {
            Diagnostics::SetError("Could not allocate memory for sample buffer of audio stream playback!");
            playback->State = PLAYBACK_NONE;
            resStream->Close();
            return 0;
        }
        playback->SampleBuffer = stream->SampleBuffer;

        ReadStreamSamples(playback);
        playback->State = PLAYBACK_STREAM;
        return 1;
    }
    int      PlayStream(CString filename, int streamIndex, int playbackIndex, int startAtSampleIndex, int loopAtSampleIndex) {
        AudioPlayback* playback;
        if (playbackIndex < 0 ||
            streamIndex < 0 ||
            playbackIndex >= MAX_AUDIO_PLAYBACKS ||
            streamIndex >= MAX_AUDIO_STREAMS)
            return -1;

        if (playbackIndex > -1 && Playbacks[playbackIndex].State == PLAYBACK_STREAM_LOADING)
            return -1;

        // If we couldnt, find the first free playback slot with State == PLAYBACK_STREAM_LOADING
        if (playbackIndex == -1) {
            playback = Playbacks;
            for (int i = 0; i < MAX_AUDIO_PLAYBACKS; i++) {
                if (playback->State == PLAYBACK_STREAM) {
                    playbackIndex = i;
                }
                playback++;
            }
        }

        AudioStream* stream = &Streams[streamIndex];
        playback = &Playbacks[playbackIndex];

        Lock();

        playback->State = PLAYBACK_STREAM_LOADING;

        playback->Index = streamIndex;
        playback->Speed = 0x10000;
        playback->Panning = 0.0f;
        playback->Volume = 1.0f;
        playback->LoopSampleIndex = loopAtSampleIndex;
        playback->SampleIndex = 0;
        playback->SampleCount = SAMPLE_READ;
        playback->Priority = 0xFF;

        stream->StartAtSampleIndex = startAtSampleIndex;

        stream->FileName[0] = 0;
        strcat(stream->FileName, filename);

        if ((true)) {
            auto thread = Threading::CreateThread(LoadStream, playback);
            Threading::DetachThread(thread);
        }
        else {
            LoadStream(playback);
        }

        Unlock();

        return playbackIndex;
    }

    // SoundFX
    Resource LoadSoundFX(CString filename, Uint8 maxConcurrentPlay, int unloadPolicy) {
        if (unloadPolicy < 0 || unloadPolicy > 2)
            return -1;

        Hash name = MD5_HashString(filename);

        int emptyIndex = -1;
        for (int index = 0; index < MAX_SOUNDS; index++) {
            Resources::ResSound* resource = &Resources::ResourceSounds[index];
            if (emptyIndex < 0 && !resource->UnloadPolicy)
                emptyIndex = index;

            if (resource->UnloadPolicy && resource->Name == name)
                return index;
        }

        if (emptyIndex > -1) {
            Resources::ResSound* resource = &Resources::ResourceSounds[emptyIndex];
            resource->Name = name;

            PREFIX_FILENAME(filename, "SoundFX/");

            Stream* stream = ResourceStream::New(Resources::BufferString);
            if (stream) {
                WavHeader header;
                // RIFF Header
                stream->ReadBytes(&header, 0xC);
                if (header.MagicRIFF == 0x46464952) {
                    // fmt Header
                    stream->ReadBytes(&header.MagicFMT, 0x8);
                    stream->ReadBytes(&header.AudioFormat, header.FMTSize);
                    // data Header
                    stream->ReadBytes(&header.MagicDATA, 0x08); // DATA
                    if (header.MagicDATA == 0x61746164) {
                        // printf("%s\n", Resources::BufferString);
                        // printf("header.BitsPerSample: %d\n", header.BitsPerSample);
                        // printf("header.Channels: %d\n", header.Channels);
                        // printf("header.Frequency: %d\n", header.Frequency);

                        int TotalPossibleSamples = (int)(header.DATASize / (((header.BitsPerSample & 0xFF) >> 3) * header.Channels));
                        // printf("TotalPossibleSamples: %d\n", TotalPossibleSamples);

                        resource->SoundData.SampleCount = TotalPossibleSamples / header.Channels;

                        if (!Memory::Alloc(&resource->SoundData.SampleBuffer, resource->SoundData.SampleCount * sizeof(Sample), Memory::MEMPOOL_SOUND, false)) {
                            Diagnostics::SetError("Could not allocate memory for sample buffer of sound effect!");
                            stream->Close();
                            return -1;
                        }

                        Sample* sample = (Sample*)resource->SoundData.SampleBuffer;
                        if (header.BitsPerSample / 8 * header.Channels == sizeof(Sample)) {
                            stream->ReadBytes(sample, resource->SoundData.SampleCount * sizeof(Sample));
                        }
                        else {
                            switch (header.Channels) {
                            case 1:
                                switch (header.BitsPerSample) {
                                case 8:
                                    for (int i = 0; i < resource->SoundData.SampleCount; i++) {
                                        sample->L =
                                        sample->R = (stream->ReadByte() - 0x80) << 8;
                                        sample++;
                                    }
                                    break;
                                case 16:
                                    for (int i = 0; i < resource->SoundData.SampleCount; i++) {
                                        sample->L =
                                        sample->R = stream->ReadInt16();
                                        sample++;
                                    }
                                    break;
                                case 32:
                                    for (int i = 0; i < resource->SoundData.SampleCount; i++) {
                                        sample->L =
                                        sample->R = (Sint16)(stream->ReadFloat() * 0x10000);
                                        sample++;
                                    }
                                    break;
                                }
                                break;
                            case 2:
                                switch (header.BitsPerSample) {
                                case 8:
                                    for (int i = 0; i < resource->SoundData.SampleCount; i++) {
                                        sample->L = (stream->ReadByte() - 0x80) << 8;
                                        sample->R = (stream->ReadByte() - 0x80) << 8;
                                        sample++;
                                    }
                                    break;
                                case 16:
                                    for (int i = 0; i < resource->SoundData.SampleCount; i++) {
                                        sample->L = stream->ReadInt16();
                                        sample->R = stream->ReadInt16();
                                        sample++;
                                    }
                                    break;
                                case 32:
                                    for (int i = 0; i < resource->SoundData.SampleCount; i++) {
                                        sample->L = (Sint16)(stream->ReadFloat() * 0x10000);
                                        sample->R = (Sint16)(stream->ReadFloat() * 0x10000);
                                        sample++;
                                    }
                                    break;
                                }
                                break;
                            default:
                                // Unsupported.
                                break;
                            }
                        }

                        resource->SoundData.MaxConcurrentPlay = maxConcurrentPlay;
                        resource->SoundData.CurrentPlays = 0;
                    }
                }

                stream->Close();
            }
            else {
                // fprintf(stderr, "Couldn't open stream! %s\n", Diagnostics::ErrorString);
                return -1;
            }

            resource->UnloadPolicy = unloadPolicy;
        }
        return emptyIndex;
    }
    int      PlaySoundFX(Resource sound, int loopAtSampleIndex, Uint8 priority) {
        if (sound < 0 || sound >= MAX_SOUNDS)
            return -1;

        Resources::ResSound* resSound = &Resources::ResourceSounds[sound];
        if (!resSound->UnloadPolicy)
            return -1;

        int concurrentPlays;
        AudioPlayback* playback;

        int playbackIndex = -1;

        // Check for how many playbacks are playing this sound
        concurrentPlays = 0;
        playback = Playbacks;
        for (int i = 0; i < MAX_AUDIO_PLAYBACKS; i++) {
            if (playback->State == PLAYBACK_SOUNDFX && playback->Index == sound) {
                if (++concurrentPlays >= resSound->SoundData.MaxConcurrentPlay)
                    break;
            }
            playback++;
        }

        // If we hit the max playbacks, find the oldest playback with this sound index to use
        if (concurrentPlays >= resSound->SoundData.MaxConcurrentPlay) {
            playback = Playbacks;
            int oldestSoundPlayIndex = 0x7FFFFFFF;
            for (int i = 0; i < MAX_AUDIO_PLAYBACKS; i++) {
                if (oldestSoundPlayIndex > playback->SoundPlayIndex && playback->State == PLAYBACK_SOUNDFX && playback->Index == sound) {
                    oldestSoundPlayIndex = playback->SoundPlayIndex;
                    playbackIndex = i;
                }
                playback++;
            }
        }

        // If we couldnt, find the first free playback slot with Index == -1 && State != PLAYBACK_STREAM_LOADING
        if (playbackIndex == -1) {
            playback = Playbacks;
            for (int i = 0; i < MAX_AUDIO_PLAYBACKS; i++) {
                if (playback->State != PLAYBACK_STREAM && playback->State != PLAYBACK_STREAM_LOADING && playback->Index == -1) {
                    playbackIndex = i;
                }
                playback++;
            }
        }

        // If all slots are being used, find the playback with the smallest sample count that is <= the desired priority and not a loading stream
        if (playbackIndex == -1) {
            playback = Playbacks;
            int shortestSampleCountIndex = 0x7FFFFFFF;
            for (int i = 0; i < MAX_AUDIO_PLAYBACKS; i++) {
                if (shortestSampleCountIndex > playback->SampleCount &&
                    playback->Priority <= priority &&
                    playback->State != PLAYBACK_STREAM &&
                    playback->State != PLAYBACK_STREAM_LOADING) {
                    shortestSampleCountIndex = playback->SampleCount;
                    playbackIndex = i;
                }
                playback++;
            }
        }

        if (playbackIndex == -1)
            return -1;

        if (Lock()) {
            playback = &Playbacks[playbackIndex];
            playback->State = PLAYBACK_SOUNDFX;

            playback->Index = sound;
            playback->Speed = 0x10000;
            playback->Panning = 0.0f;
            playback->Volume = 1.0f;
            playback->LoopSampleIndex = loopAtSampleIndex;
            playback->SampleIndex = 0;
            playback->SampleBuffer = (Sample*)resSound->SoundData.SampleBuffer;
            playback->SampleCount = resSound->SoundData.SampleCount;
            playback->Priority = 0xFF;

            playback->SoundPlayIndex = resSound->SoundData.CurrentPlays++;

            Unlock();
        }

        return playbackIndex;
    }
    void     StopSoundFX(Resource sound) {
        AudioPlayback* playback = Playbacks;
        for (int i = 0; i < MAX_AUDIO_PLAYBACKS; i++) {
            if (playback->State == PLAYBACK_SOUNDFX && playback->Index == sound) {
                playback->State = PLAYBACK_NONE;
                playback->Index = -1;
            }
            playback++;
        }
    }
    bool     IsSoundFXPlaying(Resource sound) {
        AudioPlayback* playback = Playbacks;
        for (int i = 0; i < MAX_AUDIO_PLAYBACKS; i++) {
            if (playback->State == PLAYBACK_SOUNDFX && playback->Index == sound) {
                return true;
            }
            playback++;
        }
        return false;
    }

    // General playback
    void   PlaybackAlter(int playbackIndex, float volume, float panning, float speed) {
        if (playbackIndex < 0 || playbackIndex >= MAX_AUDIO_PLAYBACKS)
            return;

        if (Lock()) {
            AudioPlayback* playback = &Playbacks[playbackIndex];
            playback->Speed = (int)(speed * 0x10000);
            playback->Panning = M_CLAMP(panning, -1.0f, 1.0f);
            playback->Volume = M_CLAMP(volume, 0.0f, 4.0f);
            Unlock();
        }
    }
    bool   PlaybackIsValid(int playbackIndex) {
        return false;
    }
    int    PlaybackGetSamplePosition(int playbackIndex) {
        int result;
        AudioPlayback* playback = &Playbacks[playbackIndex];
        AudioStream* stream = &Streams[playback->Index];
        switch (playback->State) {
        case PLAYBACK_SOUNDFX:
            return playback->SampleIndex;
        case PLAYBACK_STREAM:
#ifdef USE_STB_VORBIS
            result = stb_vorbis_get_sample_offset(stream->VorbisSTB);
            if (result < 0)
                return 0;
#else
            result = (int)ov_pcm_tell(&Streams[playback->Index].VorbisFile);
            if (result < 0 || result == OV_EINVAL)
                return 0;
#endif
            return result;
        }
        return 0;
    }
    void   PlaybackStop(int playbackIndex) {
        if (playbackIndex < 0 || playbackIndex >= MAX_AUDIO_PLAYBACKS)
            return;

        AudioPlayback* playback = &Playbacks[playbackIndex];
        if (playback->State == PLAYBACK_NONE)
            return;

        if (Lock()) {
            playback->State = PLAYBACK_NONE;
            playback->Index = -1;
            Unlock();
        }
    }
    void   PlaybackPause(int playbackIndex) { }
    void   PlaybackResume(int playbackIndex) { }
    void   PlaybackPauseAll() { }
    void   PlaybackResumeAll() { }
    double GetPlaybackClock(int playbackIndex) {
        int result;
        AudioPlayback* playback = &Playbacks[playbackIndex];
        AudioStream* stream = &Streams[playback->Index];
        switch (playback->State) {
        case PLAYBACK_STREAM:
#ifdef USE_STB_VORBIS
            result = stb_vorbis_get_sample_offset(stream->VorbisSTB);
            if (result < 0)
                return -1.0;
#else
            result = (int)ov_pcm_tell(&Streams[playback->Index].VorbisFile);
            if (result < 0 || result == OV_EINVAL)
                return -1.0;
#endif
            return result / (double)SAMPLE_RATE;
        }
        return -1.0;
    }

    bool   Init() {
        ZERO_OUT(Streams);
        ZERO_OUT(Playbacks);

        StreamVolume = Settings::audio.bgmVolume * 0.005f;
        SoundFXVolume = Settings::audio.sfxVolume * 0.005f;

        if (!PlatformInit())
            return false;

        return true;
    }
    void   Dispose() {
        Lock();
    #ifdef USE_STB_VORBIS
        for (int s = 0; s < MAX_AUDIO_STREAMS; s++) {
            stb_vorbis_close(Streams[s].VorbisSTB);
        }
    #else
        for (int s = 0; s < MAX_AUDIO_STREAMS; s++) {
            ov_clear(&Streams[s].VorbisFile);
        }
    #endif
        Unlock();

        PlatformDispose();
    }
}

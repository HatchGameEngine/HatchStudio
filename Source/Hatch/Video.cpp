#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Video.h>

#include <Hatch/IO/ResourceStream.h>
#include <Hatch/Audio.h>
#include <Hatch/Diagnostics.h>
#include <Hatch/Game.h>
#include <Hatch/Memory.h>
#include <Hatch/Renderer.h>
#include <Hatch/Resources.h>

#ifdef THEORADEC_INCLUDE_PATH
#include THEORADEC_INCLUDE_PATH
#else
#include <Hatch/Libraries/Theora/codec.h>
#include <Hatch/Libraries/Theora/theoradec.h>
#endif

namespace Video {
    struct VideoStream {
        Stream* StreamIO;
        int     LastEngineState;

        /* Current State */
    	ogg_sync_state sync;
    	ogg_page page;
    	int eos;

    	/* Stream Data */
    	int tpackets;
    	int vpackets;
    	ogg_stream_state tstream;
    	ogg_stream_state vstream;

        ogg_packet packet;

    	/* Metadata */
    	th_info tinfo;
    	th_comment tcomment;

    	// vorbis_info vinfo;
    	// vorbis_comment vcomment;

    	/* Theora Data */
    	th_dec_ctx *tdec;
    	int pp_level_max;
    	int pp_level;
    	int pp_inc;

        int width;
    	int height;
    	double fps;
    	th_pixel_fmt fmt;
    	th_colorspace colorspace;

    	/* Vorbis Data */
    	int vdsp_init;
    	// vorbis_dsp_state vdsp;
    	int vblock_init;
    	// vorbis_block vblock;
    };
    struct VideoPlayback {
        double Position;
        double StartPosition;
        Sint64 GranulePosition;
        bool (*StateFunction)();
    };

    VideoStream Streams[1];
    VideoPlayback Playbacks[1];

    // #define VIDEO_BUFFER_SIZE 0x1000
    #define VIDEO_BUFFER_SIZE 0x8000

    bool ReadData(VideoStream* videoStream) {
        char* buffer = ogg_sync_buffer(&videoStream->sync, VIDEO_BUFFER_SIZE);

        if (videoStream->StreamIO->ReadBytes(buffer, VIDEO_BUFFER_SIZE) == 0) {
            return false;
        }

        ogg_sync_wrote(&videoStream->sync, VIDEO_BUFFER_SIZE);
        return true;
    }
    void QueuePage(VideoStream* videoStream) {
        if (videoStream->tpackets)
            ogg_stream_pagein(&videoStream->tstream, &videoStream->page);
    }
    bool GetNextPacket(VideoStream* videoStream, ogg_stream_state* stream, ogg_packet* packet) {
        // If we can't make a packet with the pages...
        while (ogg_stream_packetout(stream, packet) <= 0) {
            // and if we don't have any more data to read...
            if (!ReadData(videoStream)) {
                // we cannot get the next packet.
                return false;
            }
            // but, if we did read data...
            else {
                // flush the data from the buffer and insert into a page...
                while (ogg_sync_pageout(&videoStream->sync, &videoStream->page) > 0) {
                    QueuePage(videoStream);
                }

                // and hopefully, we have enough pages to output a packet, but...
            }
        }
        return true;
    }

    bool PlayStream(CString filename, double position, bool (*stateFunction)()) {
        if (Game::State.EngineState == ENGINESTATE_VIDEO)
            return false;

        PREFIX_FILENAME(filename, "Video/");

        Stream* stream = ResourceStream::New(Resources::BufferString);
        if (!stream)
            return false;

        VideoStream* videoStream = &Streams[0];
        memset(videoStream, 0, sizeof(VideoStream));

        videoStream->StreamIO = stream;

    	th_setup_info *tsetup = NULL;
        ogg_sync_init(&videoStream->sync);

        th_info_init(&videoStream->tinfo);
        th_comment_init(&videoStream->tcomment);

        // Theora is for Video streams (codec)
        // Vorbis is for Audio streams (codec)
        // OGG is the container

        bool readHeader = false;
        while (!readHeader) {
            ReadData(videoStream);

            while (ogg_sync_pageout(&videoStream->sync, &videoStream->page) > 0) {
                ogg_stream_state streamState;

                // If this page isn't at the beginning of the logical bitstream,
                if (!ogg_page_bos(&videoStream->page)) {
                    QueuePage(videoStream);
                    readHeader = true;
                    break;
                }

                ogg_stream_init(&streamState, ogg_page_serialno(&videoStream->page));
                ogg_stream_pagein(&streamState, &videoStream->page);
                ogg_stream_packetout(&streamState, &videoStream->packet);

                if (videoStream->tpackets == 0 && th_decode_headerin(&videoStream->tinfo, &videoStream->tcomment, &tsetup, &videoStream->packet) >= 0) {
    				memcpy(&videoStream->tstream, &streamState, sizeof(streamState));
    				videoStream->tpackets = 1;
    			}
                else {
    				ogg_stream_clear(&streamState);
    			}
            }
        }

        if (videoStream->tpackets == 0) {
            stream->Close();
            return false;
        }

        int res;
        while (videoStream->tpackets < 3) {
            while (videoStream->tpackets < 3) {
                if ((res = ogg_stream_packetout(&videoStream->tstream, &videoStream->packet)) <= 0) {
                    if (res < 0) {
                        stream->Close();
                        return false;
                    }
                    break;
                }

                if ((res = th_decode_headerin(&videoStream->tinfo, &videoStream->tcomment, &tsetup, &videoStream->packet)) <= 0) {
                    if (res < 0) {
                        switch (res) {
                        case TH_EFAULT:
                            Diagnostics::SetError("th_decode_headerin failed with error: %s", "TH_EFAULT");
                            break;
                        case TH_EVERSION:
                            Diagnostics::SetError("th_decode_headerin failed with error: %s", "TH_EVERSION");
                            break;
                        case TH_ENOTFORMAT:
                            Diagnostics::SetError("th_decode_headerin failed with error: %s", "TH_ENOTFORMAT");
                            break;
                        case TH_EBADHEADER:
                            Diagnostics::SetError("th_decode_headerin failed with error: %s", "TH_EBADHEADER");
                            break;
                        default:
                            Diagnostics::SetError("th_decode_headerin failed with error: %d", res);
                            break;
                        }
                    }

                    stream->Close();
                    return false;
                }
                videoStream->tpackets++;
            }

            if (ogg_sync_pageout(&videoStream->sync, &videoStream->page) > 0) {
                // Demux into the appropriate stream
                QueuePage(videoStream);
    		}
            else {
    			ReadData(videoStream);
    		}
        }

        if (videoStream->tpackets > 0) {
            videoStream->tdec = th_decode_alloc(&videoStream->tinfo, tsetup);
            videoStream->fmt = videoStream->tinfo.pixel_fmt;
    		videoStream->colorspace = videoStream->tinfo.colorspace;

    		th_decode_ctl(videoStream->tdec, TH_DECCTL_GET_PPLEVEL_MAX, &videoStream->pp_level_max, sizeof(videoStream->pp_level_max));
            videoStream->pp_level = videoStream->pp_level_max;
    		th_decode_ctl(videoStream->tdec, TH_DECCTL_SET_PPLEVEL, &videoStream->pp_level, sizeof(videoStream->pp_level));
        }
        else {
            th_info_clear(&videoStream->tinfo);
            th_comment_clear(&videoStream->tcomment);
            th_setup_free(tsetup);

            stream->Close();
            return false;
        }

        th_setup_free(tsetup);

        VideoPlayback* videoPlayback = &Playbacks[0];
        memset(videoPlayback, 0, sizeof(VideoPlayback));

        // NOTE: If no audio device is available, ->Position should just be 0.0
        videoPlayback->Position = 0.0;
        videoPlayback->GranulePosition = 0;
        videoPlayback->StartPosition = position;
        videoPlayback->StateFunction = stateFunction;

        // Adjust renderer shader for videoStream->fmt here.

        DecodeFrame(true);

        videoStream->LastEngineState = Game::State.EngineState;
        Game::State.EngineState = ENGINESTATE_VIDEO;

        // Set the renderer to video render mode

        return true;
    }
    void DecodeFrame(bool init) {
        VideoStream* stream = &Streams[0];
        VideoPlayback* playback = &Playbacks[0];

        if (!init) {
            double clock = Audio::GetPlaybackClock(0);
            if (clock >= 0.0)
                playback->Position = clock;
            else
                playback->Position += 1.0 / 60.0;

            double granTime = th_granule_time(stream->tdec, playback->GranulePosition) + playback->StartPosition;

            if (playback->StateFunction && playback->StateFunction())
                goto EndOfFile;

            if (playback->Position < granTime)
                return;
        }

        while (true) {
            if (GetNextPacket(stream, &stream->tstream, &stream->packet)) {
                if (th_decode_packetin(stream->tdec, &stream->packet, &playback->GranulePosition) == 0) {
                    th_ycbcr_buffer yuv;
                    th_decode_ycbcr_out(stream->tdec, yuv);

                    int offsetUV;
                    int offsetY = (stream->tinfo.pic_x & ~1) + yuv[0].stride * (stream->tinfo.pic_y & ~1);
                    switch (stream->fmt) {
                        case TH_PF_420:
                            offsetUV = (stream->tinfo.pic_x >> 1) + yuv[1].stride * (stream->tinfo.pic_y >> 1);
                            Renderer::UpdateTexture420(yuv[0].width, yuv[0].height, yuv[0].data + offsetY, yuv[1].data + offsetUV, yuv[2].data + offsetUV, yuv[0].stride, yuv[1].stride, yuv[2].stride);
                            break;
                        case TH_PF_422:
                            offsetUV = (stream->tinfo.pic_x >> 1) + yuv[1].stride * (stream->tinfo.pic_y);
                            Renderer::UpdateTexture422(yuv[0].width, yuv[0].height, yuv[0].data + offsetY, yuv[1].data + offsetUV, yuv[2].data + offsetUV, yuv[0].stride, yuv[1].stride, yuv[2].stride);
                            break;
                        case TH_PF_444:
                            Renderer::UpdateTexture444(yuv[0].width, yuv[0].height, yuv[0].data + offsetY, yuv[1].data + offsetY, yuv[2].data + offsetY, yuv[0].stride, yuv[1].stride, yuv[2].stride);
                            break;
                        default:
                            break;
                    }
                }

                break;
            }
            else {
                if (!init)
                    goto EndOfFile;
            }
        }

        return;

    EndOfFile:
        stream->StreamIO->Close();

        while (ogg_sync_pageout(&stream->sync, &stream->page) > 0) {
            QueuePage(stream);
        }

        ogg_stream_clear(&stream->tstream);

        th_decode_free(stream->tdec);
        th_info_clear(&stream->tinfo);
        th_comment_clear(&stream->tcomment);

        ogg_sync_clear(&stream->sync);

        Game::State.EngineState = stream->LastEngineState;
    }
}

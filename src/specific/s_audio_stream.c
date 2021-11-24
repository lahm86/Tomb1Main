#define S_AUDIO_IMPL
#include "src/specific/s_audio.h"

#include "memory.h"
#include "filesystem.h"
#include "log.h"

#include <assert.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#define MAX_STREAM_PLAYING_SOUNDS 10
#define AUDIO_STREAM_READ_BUFFER_SIZE                                          \
    (AUDIO_SAMPLES * AUDIO_WORKING_CHANNELS * sizeof(float))

typedef struct AUDIO_STREAM {
    bool is_used;
    bool is_playing;
    bool is_read_done;
    bool is_looped;
    float volume;

    void (*finish_callback)(int stream_id, void *user_data);
    void *finish_callback_user_data;

    struct {
        AVStream *stream;
        AVFormatContext *format_ctx;
        const AVCodec *codec;
        AVCodecContext *codec_ctx;
        AVPacket *packet;
        AVFrame *frame;
    } av;

    struct {
        SDL_AudioStream *stream;
    } sdl;
} AUDIO_STREAM;

extern SDL_AudioDeviceID g_AudioDeviceID;

static AUDIO_STREAM m_SoundStreams[MAX_STREAM_PLAYING_SOUNDS];
static float m_DecodeBuffer[AUDIO_SAMPLES * AUDIO_WORKING_CHANNELS];

static bool S_Audio_StreamDecodeFrame(AUDIO_STREAM *stream);
static bool S_Audio_StreamEnqueueFrame(AUDIO_STREAM *stream);
static bool S_Audio_StreamInitialiseFromPath(
    int stream_id, const char *file_path);

static bool S_Audio_StreamDecodeFrame(AUDIO_STREAM *stream)
{
    assert(stream);

    int error_code = av_read_frame(stream->av.format_ctx, stream->av.packet);

    if (error_code == AVERROR_EOF && stream->is_looped) {
        avio_seek(stream->av.format_ctx->pb, 0, SEEK_SET);
        avformat_seek_file(
            stream->av.format_ctx, -1, 0, 0, 0, AVSEEK_FLAG_FRAME);
        return S_Audio_StreamDecodeFrame(stream);
    }

    if (error_code < 0) {
        return false;
    }

    error_code = avcodec_send_packet(stream->av.codec_ctx, stream->av.packet);
    if (error_code < 0) {
        LOG_ERROR(
            "Got an error when decoding frame: %s", av_err2str(error_code));
        return false;
    }

    return true;
}

static bool S_Audio_StreamEnqueueFrame(AUDIO_STREAM *stream)
{
    int error_code = 0;

    assert(stream);

    while (error_code >= 0) {
        error_code =
            avcodec_receive_frame(stream->av.codec_ctx, stream->av.frame);
        if (error_code < 0 && error_code != AVERROR(EAGAIN)) {
            LOG_ERROR(
                "Got an error when decoding frame: %d, %s", error_code,
                av_err2str(error_code));
            continue;
        }

        int data_size = av_samples_get_buffer_size(
            NULL, stream->av.codec_ctx->channels, stream->av.frame->nb_samples,
            stream->av.codec_ctx->sample_fmt, 1);

        SDL_AudioStreamPut(
            stream->sdl.stream, stream->av.frame->data[0], data_size);
    }

    return true;
}

static bool S_Audio_StreamInitialiseFromPath(
    int stream_id, const char *file_path)
{
    if (!g_AudioDeviceID || stream_id < 0
        || stream_id >= MAX_STREAM_PLAYING_SOUNDS) {
        return false;
    }

    SDL_LockAudioDevice(g_AudioDeviceID);

    int error_code;
    char *full_path = NULL;
    File_GetFullPath(file_path, &full_path);

    AUDIO_STREAM *stream = &m_SoundStreams[stream_id];

    error_code =
        avformat_open_input(&stream->av.format_ctx, full_path, NULL, NULL);
    if (error_code != 0) {
        goto fail;
    }

    error_code = avformat_find_stream_info(stream->av.format_ctx, NULL);
    if (error_code < 0) {
        goto fail;
    }

    stream->av.stream = NULL;
    for (unsigned int i = 0; i < stream->av.format_ctx->nb_streams; i++) {
        AVStream *current_stream = stream->av.format_ctx->streams[i];
        if (current_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream->av.stream = current_stream;
            break;
        }
    }
    if (!stream->av.stream) {
        error_code = AVERROR_STREAM_NOT_FOUND;
        goto fail;
    }

    stream->av.codec =
        avcodec_find_decoder(stream->av.stream->codecpar->codec_id);
    if (!stream->av.codec) {
        error_code = AVERROR_DEMUXER_NOT_FOUND;
        goto fail;
    }

    stream->av.codec_ctx = avcodec_alloc_context3(stream->av.codec);
    if (!stream->av.codec_ctx) {
        error_code = AVERROR_UNKNOWN;
        goto fail;
    }

    error_code = avcodec_open2(stream->av.codec_ctx, stream->av.codec, NULL);
    if (error_code < 0) {
        goto fail;
    }

    stream->av.packet = av_packet_alloc();
    av_new_packet(stream->av.packet, 0);

    stream->av.frame = av_frame_alloc();
    if (!stream->av.frame) {
        error_code = AVERROR_UNKNOWN;
        goto fail;
    }

    S_Audio_StreamDecodeFrame(stream);

    int32_t sdl_format;
    switch (stream->av.codec_ctx->sample_fmt) {
    case AV_SAMPLE_FMT_U8:
        sdl_format = AUDIO_U8;
        break;

    case AV_SAMPLE_FMT_S16:
        sdl_format = AUDIO_S16;
        break;

    case AV_SAMPLE_FMT_S32:
        sdl_format = AUDIO_S32;
        break;

    default:
        LOG_ERROR(
            "Unknown sample format: %d", stream->av.codec_ctx->sample_fmt);
        goto fail;
    }

    int32_t sdl_sample_rate = stream->av.codec_ctx->sample_rate;
    int32_t sdl_channels = stream->av.codec_ctx->channels;

    stream->is_read_done = false;
    stream->is_used = true;
    stream->is_playing = true;
    stream->is_looped = false;
    stream->finish_callback = NULL;

    stream->sdl.stream = SDL_NewAudioStream(
        sdl_format, sdl_channels, sdl_sample_rate, AUDIO_WORKING_FORMAT,
        sdl_channels, AUDIO_WORKING_RATE);

    S_Audio_StreamEnqueueFrame(stream);
    SDL_UnlockAudioDevice(g_AudioDeviceID);

    return true;

fail:
    LOG_ERROR(
        "Error while opening audio %s: %s", file_path, av_err2str(error_code));

    S_Audio_StreamClose(stream_id);

    if (full_path) {
        Memory_Free(full_path);
        full_path = NULL;
    }

    SDL_UnlockAudioDevice(g_AudioDeviceID);
    return false;
}

void S_Audio_StreamInit()
{
    for (int stream_id = 0; stream_id < MAX_STREAM_PLAYING_SOUNDS;
         stream_id++) {
        AUDIO_STREAM *stream = &m_SoundStreams[stream_id];
        stream->is_playing = false;
        stream->is_read_done = true;
        stream->volume = 0.0;
        stream->sdl.stream = NULL;
        stream->finish_callback = NULL;
    }
}

bool S_Audio_StreamPause(int stream_id)
{
    if (!g_AudioDeviceID || stream_id < 0
        || stream_id >= MAX_STREAM_PLAYING_SOUNDS) {
        return false;
    }

    if (m_SoundStreams[stream_id].is_playing) {
        SDL_LockAudioDevice(g_AudioDeviceID);
        m_SoundStreams[stream_id].is_playing = false;
        SDL_UnlockAudioDevice(g_AudioDeviceID);
    }

    return true;
}

bool S_Audio_StreamUnpause(int stream_id)
{
    if (!g_AudioDeviceID || stream_id < 0
        || stream_id >= MAX_STREAM_PLAYING_SOUNDS) {
        return false;
    }

    if (!m_SoundStreams[stream_id].is_playing) {
        SDL_LockAudioDevice(g_AudioDeviceID);
        m_SoundStreams[stream_id].is_playing = true;
        SDL_UnlockAudioDevice(g_AudioDeviceID);
    }

    return true;
}

int S_Audio_StreamCreateFromFile(const char *file_path)
{
    for (int stream_id = 0; stream_id < MAX_STREAM_PLAYING_SOUNDS;
         stream_id++) {
        AUDIO_STREAM *stream = &m_SoundStreams[stream_id];
        if (stream->is_used) {
            continue;
        }

        if (!S_Audio_StreamInitialiseFromPath(stream_id, file_path)) {
            return -1;
        }

        return stream_id;
    }

    return -1;
}

bool S_Audio_StreamClose(int stream_id)
{
    if (!g_AudioDeviceID || stream_id < 0
        || stream_id >= MAX_STREAM_PLAYING_SOUNDS) {
        return false;
    }

    SDL_LockAudioDevice(g_AudioDeviceID);

    AUDIO_STREAM *stream = &m_SoundStreams[stream_id];

    AVPacket *packet;
    AVFrame *frame;

    if (stream->av.codec_ctx) {
        avcodec_close(stream->av.codec_ctx);
        av_free(stream->av.codec_ctx);
        stream->av.codec_ctx = NULL;
    }

    if (stream->av.format_ctx) {
        avformat_close_input(&stream->av.format_ctx);
        stream->av.format_ctx = NULL;
    }

    if (stream->av.packet) {
        av_packet_free(&stream->av.packet);
        stream->av.packet = NULL;
    }

    if (stream->av.frame) {
        av_frame_free(&stream->av.frame);
        stream->av.frame = NULL;
    }

    stream->av.stream = NULL;
    stream->av.codec = NULL;

    if (stream->sdl.stream) {
        SDL_FreeAudioStream(stream->sdl.stream);
        stream->sdl.stream = NULL;
    }

    stream->is_read_done = true;
    stream->is_used = false;
    stream->is_playing = false;
    stream->is_looped = false;
    SDL_UnlockAudioDevice(g_AudioDeviceID);

    if (stream->finish_callback) {
        stream->finish_callback(stream_id, stream->finish_callback_user_data);
    }

    return true;
}

bool S_Audio_StreamSetVolume(int stream_id, float volume)
{
    if (!g_AudioDeviceID || stream_id < 0
        || stream_id >= MAX_STREAM_PLAYING_SOUNDS) {
        return false;
    }

    m_SoundStreams[stream_id].volume = volume;

    return true;
}

bool S_Audio_StreamIsLooped(int stream_id)
{
    if (!g_AudioDeviceID || stream_id < 0
        || stream_id >= MAX_STREAM_PLAYING_SOUNDS) {
        return false;
    }

    return m_SoundStreams[stream_id].is_looped;
}

bool S_Audio_StreamSetIsLooped(int stream_id, bool is_looped)
{
    if (!g_AudioDeviceID || stream_id < 0
        || stream_id >= MAX_STREAM_PLAYING_SOUNDS) {
        return false;
    }

    m_SoundStreams[stream_id].is_looped = is_looped;

    return true;
}

bool S_Audio_StreamSetFinishCallback(
    int stream_id, void (*callback)(int stream_id, void *user_data),
    void *user_data)
{
    if (!g_AudioDeviceID || stream_id < 0
        || stream_id >= MAX_STREAM_PLAYING_SOUNDS) {
        return false;
    }

    m_SoundStreams[stream_id].finish_callback = callback;
    m_SoundStreams[stream_id].finish_callback_user_data = user_data;

    return true;
}

void S_Audio_StreamProcessSamples(float *target_buffer, size_t len)
{
    for (int stream_id = 0; stream_id < MAX_STREAM_PLAYING_SOUNDS;
         stream_id++) {
        AUDIO_STREAM *stream = &m_SoundStreams[stream_id];
        if (!stream->is_playing) {
            continue;
        }

        while ((SDL_AudioStreamAvailable(stream->sdl.stream) < (int)len)
               && !stream->is_read_done) {
            if (S_Audio_StreamDecodeFrame(stream)) {
                S_Audio_StreamEnqueueFrame(stream);
            } else {
                stream->is_read_done = true;
            }
        }

        memset(m_DecodeBuffer, 0, AUDIO_STREAM_READ_BUFFER_SIZE);
        int gotten = SDL_AudioStreamGet(
            stream->sdl.stream, m_DecodeBuffer, AUDIO_STREAM_READ_BUFFER_SIZE);
        if (gotten < 0) {
            LOG_ERROR("Error reading from sdl.stream: %s", SDL_GetError());
            stream->is_playing = false;
            stream->is_used = false;
            stream->is_read_done = true;
        } else if (gotten == 0) {
            // legit end of stream. looping is handled in
            // S_Audio_StreamDecodeFrame
            stream->is_playing = false;
            stream->is_used = false;
            stream->is_read_done = true;
        } else {
            int samples_gotten = gotten
                / (stream->av.codec_ctx->channels * sizeof(m_DecodeBuffer[0]));

            for (int s = 0; s < samples_gotten; s++) {
                int stream_idx = s * AUDIO_WORKING_CHANNELS;
                int decode_idx = s * stream->av.codec_ctx->channels;

                float pan = 0.0f;
                if (stream->av.codec_ctx->channels == 1) {
                    float data = m_DecodeBuffer[decode_idx] * stream->volume;
                    target_buffer[stream_idx] +=
                        data * S_Audio_InverseLerp(1.0f, 0.0f, pan);
                    target_buffer[stream_idx + 1] +=
                        data * S_Audio_InverseLerp(-1.0f, 0.0f, pan);
                } else {
                    target_buffer[stream_idx] +=
                        m_DecodeBuffer[decode_idx] * stream->volume;
                    target_buffer[stream_idx + 1] +=
                        m_DecodeBuffer[decode_idx + 1] * stream->volume;
                }
            }
        }

        if (!stream->is_used) {
            S_Audio_StreamClose(stream_id);
        }
    }
}

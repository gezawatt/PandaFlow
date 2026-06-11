#include "decoder.h"
#include "dr_mp3.h"
#include "dr_flac.h"
#include "dr_wav.h"
#include "aac.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char* extOf(const char* path) {
    const char* dot = strrchr(path, '.');
    return dot ? dot + 1 : "";
}

AudioFormat decoderDetect(const char* path) {
    const char* e = extOf(path);
    if (!strcasecmp(e, "mp3"))  return FMT_MP3;
    if (!strcasecmp(e, "flac")) return FMT_FLAC;
    if (!strcasecmp(e, "wav"))  return FMT_WAV;
    if (!strcasecmp(e, "m4a") || !strcasecmp(e, "mp4") ||
        !strcasecmp(e, "m4b") || !strcasecmp(e, "aac")) return FMT_AAC;
    return FMT_NONE;
}

bool decoderOpen(Decoder* d, const char* path) {
    memset(d, 0, sizeof(*d));
    d->format = decoderDetect(path);

    switch (d->format) {
        case FMT_MP3: {
            drmp3* m = (drmp3*)calloc(1, sizeof(drmp3));
            if (!m) return false;
            if (!drmp3_init_file(m, path, NULL)) { free(m); return false; }
            d->handle      = m;
            d->sampleRate  = m->sampleRate;
            d->channels    = m->channels;
            d->totalFrames = drmp3_get_pcm_frame_count(m);
            break;
        }
        case FMT_FLAC: {
            drflac* f = drflac_open_file(path, NULL);
            if (!f) return false;
            d->handle      = f;
            d->sampleRate  = f->sampleRate;
            d->channels    = f->channels;
            d->totalFrames = f->totalPCMFrameCount;
            break;
        }
        case FMT_WAV: {
            drwav* w = (drwav*)calloc(1, sizeof(drwav));
            if (!w) return false;
            if (!drwav_init_file(w, path, NULL)) { free(w); return false; }
            d->handle      = w;
            d->sampleRate  = w->sampleRate;
            d->channels    = w->channels;
            d->totalFrames = w->totalPCMFrameCount;
            break;
        }
        case FMT_AAC: {
            AacDec* a = aacOpen(path, &d->sampleRate, &d->totalFrames);
            if (!a) return false;
            d->handle   = a;
            d->channels = 2;   // aacRead siempre entrega estéreo
            break;
        }
        default:
            return false;
    }
    if (d->channels == 0) { decoderClose(d); return false; }
    return true;
}

// Lee 'frames' en el numero nativo de canales hacia 'tmp'.
static size_t readNative(Decoder* d, int16_t* tmp, size_t frames) {
    switch (d->format) {
        case FMT_MP3:  return (size_t)drmp3_read_pcm_frames_s16((drmp3*)d->handle, frames, tmp);
        case FMT_FLAC: return (size_t)drflac_read_pcm_frames_s16((drflac*)d->handle, frames, tmp);
        case FMT_WAV:  return (size_t)drwav_read_pcm_frames_s16((drwav*)d->handle, frames, tmp);
        default:       return 0;
    }
}

size_t decoderRead(Decoder* d, int16_t* out, size_t frameCount) {
    if (d->format == FMT_NONE || frameCount == 0) return 0;

    // AAC ya entrega estéreo desde su propio modulo.
    if (d->format == FMT_AAC) return aacRead((AacDec*)d->handle, out, frameCount);

    // Caso comun: stereo nativo -> lectura directa.
    if (d->channels == 2) {
        return readNative(d, out, frameCount);
    }

    // Necesitamos buffer intermedio en canales nativos.
    if (d->scratchFrames < frameCount) {
        int16_t* ns = (int16_t*)realloc(d->scratch, frameCount * d->channels * sizeof(int16_t));
        if (!ns) return 0;
        d->scratch = ns;
        d->scratchFrames = frameCount;
    }

    size_t got = readNative(d, d->scratch, frameCount);
    const uint32_t ch = d->channels;

    if (ch == 1) {
        // Mono -> duplicar a L/R.
        for (size_t i = 0; i < got; i++) {
            int16_t s = d->scratch[i];
            out[2*i + 0] = s;
            out[2*i + 1] = s;
        }
    } else {
        // Multicanal (>2) -> tomar los dos primeros canales como L/R.
        for (size_t i = 0; i < got; i++) {
            out[2*i + 0] = d->scratch[i*ch + 0];
            out[2*i + 1] = d->scratch[i*ch + 1];
        }
    }
    return got;
}

bool decoderSeek(Decoder* d, uint64_t frameIndex) {
    switch (d->format) {
        case FMT_MP3:  return drmp3_seek_to_pcm_frame((drmp3*)d->handle, frameIndex);
        case FMT_FLAC: return drflac_seek_to_pcm_frame((drflac*)d->handle, frameIndex);
        case FMT_WAV:  return drwav_seek_to_pcm_frame((drwav*)d->handle, frameIndex);
        case FMT_AAC:  return aacSeek((AacDec*)d->handle, frameIndex);
        default:       return false;
    }
}

uint64_t decoderTell(Decoder* d) {
    switch (d->format) {
        case FMT_MP3:  return ((drmp3*)d->handle)->currentPCMFrame;
        case FMT_FLAC: return ((drflac*)d->handle)->currentPCMFrame;
        case FMT_WAV:  return ((drwav*)d->handle)->readCursorInPCMFrames;
        case FMT_AAC:  return aacTell((AacDec*)d->handle);
        default:       return 0;
    }
}

void decoderClose(Decoder* d) {
    if (!d) return;
    switch (d->format) {
        case FMT_MP3:  if (d->handle) { drmp3_uninit((drmp3*)d->handle); free(d->handle); } break;
        case FMT_FLAC: if (d->handle) { drflac_close((drflac*)d->handle); } break;
        case FMT_WAV:  if (d->handle) { drwav_uninit((drwav*)d->handle); free(d->handle); } break;
        case FMT_AAC:  if (d->handle) { aacClose((AacDec*)d->handle); } break;
        default: break;
    }
    if (d->scratch) free(d->scratch);
    memset(d, 0, sizeof(*d));
}

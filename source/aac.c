#include "aac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MP4D_INFO_SUPPORTED 1
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
#include "neaacdec.h"

#define AAC_FRAME_SAMPLES 1024   // muestras por canal en AAC-LC (aprox.)

struct AacDec {
    bool      isMp4;
    uint8_t*  file;
    int64_t   fileSize;

    NeAACDecHandle dec;
    uint32_t  sampleRate;
    uint8_t   channels;
    uint64_t  totalFrames;
    uint64_t  curFrame;       // frames (por canal) ya entregados

    // PCM estéreo decodificado pendiente de entregar
    int16_t*  left;           // buffer
    size_t    leftCap;        // capacidad en frames
    size_t    leftLen;        // frames válidos
    size_t    leftPos;        // frames ya consumidos

    // m4a/mp4
    MP4D_demux_t mp4;
    unsigned  track;
    unsigned  nsample;

    // aac adts
    size_t    aacPos;
};

typedef struct { const uint8_t* data; int64_t size; } MemCtx;

static int memRead(int64_t offset, void* buffer, size_t size, void* token) {
    MemCtx* c = (MemCtx*)token;
    if (offset < 0 || offset > c->size) return 1;
    int64_t avail = c->size - offset;
    size_t n = (size <= (size_t)avail) ? size : (size_t)avail;
    memcpy(buffer, c->data + offset, n);
    return n != size;   // 0 = ok
}

static uint8_t* readWholeFile(const char* path, int64_t* outSize) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t* buf = (uint8_t*)malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(buf); return NULL; }
    *outSize = sz;
    return buf;
}

static bool initDecoderFromDSI(AacDec* a, unsigned char* dsi, unsigned dsiBytes) {
    a->dec = NeAACDecOpen();
    if (!a->dec) return false;
    NeAACDecConfigurationPtr cfg = NeAACDecGetCurrentConfiguration(a->dec);
    cfg->outputFormat = FAAD_FMT_16BIT;
    cfg->downMatrix   = 1;     // baja multicanal a estéreo cuando puede
    NeAACDecSetConfiguration(a->dec, cfg);

    unsigned long sr = 0; unsigned char ch = 0;
    if (NeAACDecInit2(a->dec, dsi, dsiBytes, &sr, &ch) < 0) return false;
    a->sampleRate = (uint32_t)sr;
    a->channels   = ch;
    return true;
}

AacDec* aacOpen(const char* path, uint32_t* sampleRate, uint64_t* totalFrames) {
    AacDec* a = (AacDec*)calloc(1, sizeof(AacDec));
    if (!a) return NULL;

    a->file = readWholeFile(path, &a->fileSize);
    if (!a->file) { free(a); return NULL; }

    const char* dot = strrchr(path, '.');
    a->isMp4 = dot && (!strcasecmp(dot, ".m4a") || !strcasecmp(dot, ".mp4") || !strcasecmp(dot, ".m4b"));

    if (a->isMp4) {
        MemCtx ctx = { a->file, a->fileSize };
        if (MP4D_open(&a->mp4, memRead, &ctx, a->fileSize) == 0) { aacClose(a); return NULL; }
        // re-apuntar el token a uno persistente (MP4D guarda el puntero)
        // -> guardamos el ctx dentro y reasignamos
        // (MP4D_open ya leyó todo el índice; las lecturas de frames usan offset directo)

        // elegir primera pista de audio ('soun' = 0x736F756E)
        int tr = -1;
        for (unsigned i = 0; i < a->mp4.track_count; i++) {
            if (a->mp4.track[i].handler_type == 0x736F756Eu) { tr = (int)i; break; }
        }
        if (tr < 0) tr = 0;
        a->track = (unsigned)tr;

        MP4D_track_t* t = &a->mp4.track[a->track];
        if (!initDecoderFromDSI(a, t->dsi, t->dsi_bytes)) { aacClose(a); return NULL; }
        if (a->sampleRate == 0) a->sampleRate = t->SampleDescription.audio.samplerate_hz;
        a->totalFrames = (uint64_t)t->sample_count * AAC_FRAME_SAMPLES;
        a->nsample = 0;
    } else {
        // ADTS crudo: faad encuentra la sincronía
        a->dec = NeAACDecOpen();
        if (!a->dec) { aacClose(a); return NULL; }
        NeAACDecConfigurationPtr cfg = NeAACDecGetCurrentConfiguration(a->dec);
        cfg->outputFormat = FAAD_FMT_16BIT;
        cfg->downMatrix = 1;
        NeAACDecSetConfiguration(a->dec, cfg);
        unsigned long sr = 0; unsigned char ch = 0;
        long consumed = NeAACDecInit(a->dec, a->file, a->fileSize, &sr, &ch);
        if (consumed < 0) { aacClose(a); return NULL; }
        a->sampleRate = (uint32_t)sr;
        a->channels   = ch;
        a->aacPos     = (size_t)consumed;
        a->totalFrames = 0;  // desconocido en ADTS
    }

    if (sampleRate)  *sampleRate  = a->sampleRate;
    if (totalFrames) *totalFrames = a->totalFrames;
    return a;
}

// Convierte 'n' frames de PCM nativo (s16, ch canales) a estéreo en dst.
static void toStereo(int16_t* dst, const int16_t* src, size_t n, unsigned ch) {
    if (ch == 2) { memcpy(dst, src, n * 2 * sizeof(int16_t)); return; }
    if (ch == 1) { for (size_t i = 0; i < n; i++) { dst[2*i]=src[i]; dst[2*i+1]=src[i]; } return; }
    for (size_t i = 0; i < n; i++) { dst[2*i]=src[i*ch]; dst[2*i+1]=src[i*ch+1]; }
}

// Decodifica un frame AAC al buffer 'left'. Devuelve false en EOF/error fatal.
static bool decodeFrame(AacDec* a) {
    NeAACDecFrameInfo info;
    void* pcm = NULL;

    if (a->isMp4) {
        if (a->nsample >= a->mp4.track[a->track].sample_count) return false;
        unsigned fb = 0, ts = 0, dur = 0;
        MP4D_file_offset_t off = MP4D_frame_offset(&a->mp4, a->track, a->nsample, &fb, &ts, &dur);
        a->nsample++;
        if (off + fb > (MP4D_file_offset_t)a->fileSize || fb == 0) return false;
        pcm = NeAACDecDecode(a->dec, &info, a->file + off, fb);
    } else {
        if (a->aacPos >= (size_t)a->fileSize) return false;
        pcm = NeAACDecDecode(a->dec, &info, a->file + a->aacPos, a->fileSize - a->aacPos);
        if (info.bytesconsumed == 0) return false;
        a->aacPos += info.bytesconsumed;
    }

    if (info.error != 0 || pcm == NULL || info.samples == 0) {
        // un frame con error: lo saltamos sin abortar todo
        a->leftLen = a->leftPos = 0;
        return true;
    }

    unsigned ch = info.channels ? info.channels : a->channels;
    size_t frames = info.samples / (ch ? ch : 1);

    if (a->leftCap < frames) {
        int16_t* nb = (int16_t*)realloc(a->left, frames * 2 * sizeof(int16_t));
        if (!nb) return false;
        a->left = nb; a->leftCap = frames;
    }
    toStereo(a->left, (const int16_t*)pcm, frames, ch);
    a->leftLen = frames;
    a->leftPos = 0;
    return true;
}

size_t aacRead(AacDec* a, int16_t* out, size_t frameCount) {
    size_t done = 0;
    while (done < frameCount) {
        if (a->leftPos >= a->leftLen) {
            if (!decodeFrame(a)) break;       // EOF
            if (a->leftLen == 0) continue;    // frame con error: siguiente
        }
        size_t avail = a->leftLen - a->leftPos;
        size_t need  = frameCount - done;
        size_t n = (avail < need) ? avail : need;
        memcpy(out + done*2, a->left + a->leftPos*2, n * 2 * sizeof(int16_t));
        a->leftPos += n;
        done += n;
    }
    a->curFrame += done;
    return done;
}

bool aacSeek(AacDec* a, uint64_t frameIndex) {
    if (!a->isMp4) return false;   // ADTS: sin índice
    unsigned target = (unsigned)(frameIndex / AAC_FRAME_SAMPLES);
    if (target >= a->mp4.track[a->track].sample_count)
        target = a->mp4.track[a->track].sample_count ? a->mp4.track[a->track].sample_count - 1 : 0;
    a->nsample = target;
    a->leftLen = a->leftPos = 0;
    a->curFrame = (uint64_t)target * AAC_FRAME_SAMPLES;
    NeAACDecPostSeekReset(a->dec, -1);   // limpia estado del decoder
    return true;
}

uint64_t aacTell(AacDec* a) { return a->curFrame; }

void aacClose(AacDec* a) {
    if (!a) return;
    if (a->dec) NeAACDecClose(a->dec);
    if (a->isMp4) MP4D_close(&a->mp4);
    if (a->left) free(a->left);
    if (a->file) free(a->file);
    free(a);
}

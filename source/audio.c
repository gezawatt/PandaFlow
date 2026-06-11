#include "audio.h"
#include "decoder.h"
#include <3ds.h>
#include <string.h>
#include <stdlib.h>

// --- Parametros de streaming ---
#define NUM_BUFFERS      4              // mas buffers = mas cushion contra underruns
#define FRAMES_PER_BUF   4096           // por buffer (stereo s16)
#define CHANNELS_OUT     2
#define NDSP_CHN         0
#define THREAD_STACK     (32 * 1024)

static Decoder       s_dec;
static ndspWaveBuf   s_wb[NUM_BUFFERS];
static int16_t*      s_buf = NULL;       // memoria lineal: NUM_BUFFERS * FRAMES_PER_BUF * 2
static Thread        s_thread;
static LightLock     s_lock;
static volatile bool s_running   = false;   // hilo activo
static volatile bool s_quit      = false;   // pedir fin del hilo
static volatile AudioState s_state = AUDIO_STOPPED;
static volatile bool s_loaded    = false;   // hay pista cargada
static volatile bool s_ended     = false;   // termino sola
static volatile bool s_awake     = false;   // estamos impidiendo suspension

static LightEvent    s_wake;                 // despierta al hilo cuando hay trabajo

static inline int16_t* bufPtr(int i) {
    return s_buf + (size_t)i * FRAMES_PER_BUF * CHANNELS_OUT;
}

// Activa/desactiva el bloqueo de suspension. Cuando esta en false la consola
// NO entra en sleep al cerrar la tapa: el audio sigue sonando.
static void setKeepAwake(bool keep) {
    if (keep == s_awake) return;
    aptSetSleepAllowed(!keep);   // keep=true  -> sleep NO permitido
    s_awake = keep;
}

// Rellena un waveBuf decodificando del archivo. Devuelve false si EOF.
static bool fillBuffer(int i) {
    int16_t* p = bufPtr(i);
    size_t got = decoderRead(&s_dec, p, FRAMES_PER_BUF);
    if (got == 0) return false;

    // Si quedo corto (ultimo bloque), rellenar el resto con silencio.
    if (got < FRAMES_PER_BUF) {
        memset(p + got * CHANNELS_OUT, 0,
               (FRAMES_PER_BUF - got) * CHANNELS_OUT * sizeof(int16_t));
    }
    s_wb[i].nsamples = (u32)FRAMES_PER_BUF;
    DSP_FlushDataCache(p, FRAMES_PER_BUF * CHANNELS_OUT * sizeof(int16_t));
    ndspChnWaveBufAdd(NDSP_CHN, &s_wb[i]);
    return true;
}

static void audioThreadFunc(void* arg) {
    (void)arg;
    while (!s_quit) {
        LightLock_Lock(&s_lock);
        bool didWork = false;

        if (s_state == AUDIO_PLAYING && s_loaded) {
            for (int i = 0; i < NUM_BUFFERS; i++) {
                if (s_wb[i].status == NDSP_WBUF_DONE ||
                    s_wb[i].status == NDSP_WBUF_FREE) {
                    if (!fillBuffer(i)) {
                        // Fin natural de la pista.
                        s_state  = AUDIO_STOPPED;
                        s_ended  = true;
                        setKeepAwake(false);
                        break;
                    }
                    didWork = true;
                }
            }
        }
        LightLock_Unlock(&s_lock);

        if (!didWork) {
            // Esperar a que haya trabajo o un pequeno timeout para repollear
            // el estado de los waveBuf mientras suena.
            LightEvent_WaitTimeout(&s_wake, 3 * 1000 * 1000LL); // 3 ms
        }
    }
}

bool audioInit(void) {
    if (ndspInit() != 0) return false;

    s_buf = (int16_t*)linearAlloc(NUM_BUFFERS * FRAMES_PER_BUF * CHANNELS_OUT * sizeof(int16_t));
    if (!s_buf) { ndspExit(); return false; }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(NDSP_CHN, NDSP_INTERP_POLYPHASE);
    ndspChnSetFormat(NDSP_CHN, NDSP_FORMAT_STEREO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0f;  // front left
    mix[1] = 1.0f;  // front right
    ndspChnSetMix(NDSP_CHN, mix);

    memset(s_wb, 0, sizeof(s_wb));
    for (int i = 0; i < NUM_BUFFERS; i++) {
        s_wb[i].data_vaddr = bufPtr(i);
        s_wb[i].nsamples   = 0;
    }

    LightLock_Init(&s_lock);
    LightEvent_Init(&s_wake, RESET_ONESHOT);

    s_quit = false;
    s_running = true;

    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    // Prioridad un poco mas alta que el hilo principal (numeros menores = mas prioridad).
    s_thread = threadCreate(audioThreadFunc, NULL, THREAD_STACK, prio - 1, -1, false);
    if (!s_thread) {
        s_running = false;
        linearFree(s_buf); s_buf = NULL;
        ndspExit();
        return false;
    }
    return true;
}

void audioExit(void) {
    audioStop();
    s_quit = true;
    LightEvent_Signal(&s_wake);
    if (s_thread) { threadJoin(s_thread, U64_MAX); threadFree(s_thread); s_thread = NULL; }
    setKeepAwake(false);
    if (s_buf) { linearFree(s_buf); s_buf = NULL; }
    ndspExit();
}

bool audioPlayFile(const char* path) {
    LightLock_Lock(&s_lock);

    ndspChnWaveBufClear(NDSP_CHN);
    if (s_loaded) { decoderClose(&s_dec); s_loaded = false; }

    if (!decoderOpen(&s_dec, path)) {
        LightLock_Unlock(&s_lock);
        return false;
    }

    ndspChnSetRate(NDSP_CHN, (float)s_dec.sampleRate);
    ndspChnSetPaused(NDSP_CHN, false);

    for (int i = 0; i < NUM_BUFFERS; i++) {
        s_wb[i].status = NDSP_WBUF_DONE; // forzar recarga por el hilo
        s_wb[i].nsamples = 0;
    }

    s_loaded = true;
    s_ended  = false;
    s_state  = AUDIO_PLAYING;
    setKeepAwake(true);

    LightLock_Unlock(&s_lock);
    LightEvent_Signal(&s_wake);
    return true;
}

void audioTogglePause(void) {
    LightLock_Lock(&s_lock);
    if (s_state == AUDIO_PLAYING) {
        ndspChnSetPaused(NDSP_CHN, true);
        s_state = AUDIO_PAUSED;
        setKeepAwake(false);          // en pausa, dejar dormir para ahorrar bateria
    } else if (s_state == AUDIO_PAUSED) {
        ndspChnSetPaused(NDSP_CHN, false);
        s_state = AUDIO_PLAYING;
        setKeepAwake(true);
    }
    LightLock_Unlock(&s_lock);
    LightEvent_Signal(&s_wake);
}

void audioStop(void) {
    LightLock_Lock(&s_lock);
    ndspChnWaveBufClear(NDSP_CHN);
    if (s_loaded) { decoderClose(&s_dec); s_loaded = false; }
    s_state = AUDIO_STOPPED;
    setKeepAwake(false);
    LightLock_Unlock(&s_lock);
}

AudioState audioGetState(void) { return s_state; }

bool audioConsumeTrackEnded(void) {
    if (s_ended) { s_ended = false; return true; }
    return false;
}

void audioGetProgress(uint64_t* curFrame, uint64_t* totalFrames, uint32_t* sampleRate) {
    LightLock_Lock(&s_lock);
    if (curFrame)    *curFrame    = s_loaded ? decoderTell(&s_dec) : 0;
    if (totalFrames) *totalFrames = s_loaded ? s_dec.totalFrames   : 0;
    if (sampleRate)  *sampleRate  = s_loaded ? s_dec.sampleRate    : 0;
    LightLock_Unlock(&s_lock);
}

void audioSeekSeconds(int seconds) {
    LightLock_Lock(&s_lock);
    if (s_loaded && s_dec.sampleRate > 0) {
        int64_t cur = (int64_t)decoderTell(&s_dec);
        int64_t delta = (int64_t)seconds * (int64_t)s_dec.sampleRate;
        int64_t tgt = cur + delta;
        if (tgt < 0) tgt = 0;
        if (s_dec.totalFrames && (uint64_t)tgt >= s_dec.totalFrames)
            tgt = (int64_t)s_dec.totalFrames - 1;
        decoderSeek(&s_dec, (uint64_t)tgt);
        // Forzar recarga de buffers tras el salto.
        ndspChnWaveBufClear(NDSP_CHN);
        for (int i = 0; i < NUM_BUFFERS; i++) {
            s_wb[i].status = NDSP_WBUF_DONE;
            s_wb[i].nsamples = 0;
        }
    }
    LightLock_Unlock(&s_lock);
    LightEvent_Signal(&s_wake);
}

bool audioKeepingAwake(void) { return s_awake; }

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    AUDIO_STOPPED = 0,
    AUDIO_PLAYING,
    AUDIO_PAUSED
} AudioState;

// Inicializa NDSP y lanza el hilo de audio. Llamar una vez al inicio.
bool audioInit(void);

// Detiene todo, libera y cierra NDSP. Llamar una vez al salir.
void audioExit(void);

// Carga y empieza a reproducir el archivo dado (sdmc:/...). Reemplaza la pista
// actual. Devuelve true si pudo abrirlo.
bool audioPlayFile(const char* path);

// Pausa <-> reanuda. (No tiene efecto si esta detenido.)
void audioTogglePause(void);

// Detiene la reproduccion y descarga la pista.
void audioStop(void);

AudioState audioGetState(void);

// true (y se autolimpia) si la pista termino sola desde la ultima consulta.
bool audioConsumeTrackEnded(void);

// Progreso actual. Cualquiera de los punteros puede ser NULL.
void audioGetProgress(uint64_t* curFrame, uint64_t* totalFrames, uint32_t* sampleRate);

// Salta relativo en segundos (positivo o negativo).
void audioSeekSeconds(int seconds);

// Indica si en este momento se esta impidiendo la suspension (tapa cerrada
// seguiria sonando). Util para mostrarlo en la UI.
bool audioKeepingAwake(void);

// Volumen de salida como porcentaje. 100 = sin ganancia (sonoridad nativa del
// archivo). Por encima de 100 amplifica por software con limite duro (clamp)
// para que la senal no se rompa. Se recorta a [0, AUDIO_VOL_MAX].
#define AUDIO_VOL_MAX 200
void audioSetVolume(int pct);
int  audioGetVolume(void);

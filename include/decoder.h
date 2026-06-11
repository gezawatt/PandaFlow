#pragma once
// Capa de decodificacion: abstrae MP3/FLAC/WAV (via dr_libs) y SIEMPRE
// entrega audio en stereo PCM16 intercalado (2 canales), sin importar el
// numero de canales nativo del archivo.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    FMT_NONE = 0,
    FMT_MP3,
    FMT_FLAC,
    FMT_WAV,
    FMT_AAC
} AudioFormat;

typedef struct {
    AudioFormat format;
    void*    handle;       // drmp3*/drflac*/drwav* segun el formato
    uint32_t sampleRate;   // Hz del archivo
    uint32_t channels;     // canales nativos (1, 2, ...)
    uint64_t totalFrames;  // frames PCM totales por canal (0 si desconocido)

    // scratch para conversion a stereo cuando channels != 2
    int16_t* scratch;
    size_t   scratchFrames;
} Decoder;

// Detecta el formato por la extension del archivo.
AudioFormat decoderDetect(const char* path);

// Abre el archivo. Devuelve true si pudo. Rellena los campos del Decoder.
bool decoderOpen(Decoder* d, const char* path);

// Lee hasta frameCount frames y los escribe SIEMPRE como stereo intercalado
// (2 * frameCount muestras s16) en out. Devuelve los frames realmente leidos
// (0 = fin del archivo).
size_t decoderRead(Decoder* d, int16_t* out, size_t frameCount);

// Salta al frame indicado (por canal). Devuelve true si tuvo exito.
bool decoderSeek(Decoder* d, uint64_t frameIndex);

// Posicion actual en frames (por canal).
uint64_t decoderTell(Decoder* d);

// Cierra y libera. Seguro de llamar aunque no este abierto.
void decoderClose(Decoder* d);

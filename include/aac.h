#pragma once
// Decodificador AAC (.m4a/.mp4 via minimp4 + .aac ADTS) sobre faad2.
// Entrega SIEMPRE estéreo PCM16 intercalado, igual que el resto del pipeline.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct AacDec AacDec;

// Abre el archivo. NULL si falla. Rellena rate/channels(=2 efectivo)/totalFrames.
AacDec*  aacOpen(const char* path, uint32_t* sampleRate, uint64_t* totalFrames);

// Lee hasta frameCount frames como estéreo (2*frameCount s16). Devuelve frames leídos.
size_t   aacRead(AacDec* a, int16_t* out, size_t frameCount);

// Salta al frame indicado (solo .m4a/.mp4; en ADTS crudo devuelve false).
bool     aacSeek(AacDec* a, uint64_t frameIndex);

uint64_t aacTell(AacDec* a);
void     aacClose(AacDec* a);

#pragma once
#include <stdint.h>
#include <stdbool.h>

// Carga letras para una canción. Busca, junto al archivo de audio:
//   <mismo nombre>.lrc  (sincronizado)  -> prioridad
//   <mismo nombre>.txt  (texto plano)
// Devuelve true si encontró alguna.
bool        lyricsLoad(const char* songPath);
void        lyricsClear(void);

bool        lyricsAvailable(void);   // hay letras cargadas
bool        lyricsSynced(void);      // son .lrc con tiempos
int         lyricsCount(void);
const char* lyricsLine(int i);

// Para letras sincronizadas: índice de la línea activa según el tiempo (ms).
// Devuelve -1 si no hay/no aplica.
int         lyricsActiveIndex(uint32_t currentMs);

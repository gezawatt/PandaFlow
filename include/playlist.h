#pragma once
#include <stddef.h>
#include <stdbool.h>

#define PL_MAX_PATH   768
#define PL_MAX_NAME   256
#define PL_MAX_TRACKS 2048
#define PL_MAX_ALBUMS 512

typedef struct {
    char path[PL_MAX_PATH];   // ruta completa: sdmc:/music/Album/NN - art - tit.flac
    char file[PL_MAX_NAME];   // nombre de archivo
    char title[PL_MAX_NAME];  // título parseado (sin Nº ni artista)
    char artist[160];         // artista parseado ("" si no se pudo)
    int  album;               // índice de álbum (en la tabla de álbumes)
} Track;

typedef struct {
    char name[PL_MAX_NAME];        // nombre de la carpeta (álbum)
    char coverPath[PL_MAX_PATH];   // ruta a cover.jpg/folder.jpg/cover.png ("" si no hay)
    int  firstTrack;               // índice del primer track del álbum
    int  count;                    // nº de tracks del álbum
} Album;

// Escanea 'root' de forma RECURSIVA (subcarpetas = álbumes). Devuelve nº de tracks.
int  playlistScan(const char* root);

int          playlistCount(void);
const Track* playlistGet(int index);

int          albumCount(void);
const Album* albumGet(int index);

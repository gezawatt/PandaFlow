#include "lyrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LY_MAX_LINES 1024
#define LY_MAX_TEXT  192

typedef struct {
    int64_t timeMs;             // -1 si no tiene tiempo (texto plano)
    char    text[LY_MAX_TEXT];
} LyLine;

static LyLine s_lines[LY_MAX_LINES];
static int    s_count  = 0;
static bool   s_synced = false;

void lyricsClear(void) { s_count = 0; s_synced = false; }

bool lyricsAvailable(void) { return s_count > 0; }
bool lyricsSynced(void)    { return s_synced; }
int  lyricsCount(void)     { return s_count; }

const char* lyricsLine(int i) {
    if (i < 0 || i >= s_count) return "";
    return s_lines[i].text;
}

static void addLine(int64_t ms, const char* text) {
    if (s_count >= LY_MAX_LINES) return;
    s_lines[s_count].timeMs = ms;
    snprintf(s_lines[s_count].text, LY_MAX_TEXT, "%s", text);
    s_count++;
}

static int cmpLine(const void* a, const void* b) {
    int64_t ta = ((const LyLine*)a)->timeMs, tb = ((const LyLine*)b)->timeMs;
    return (ta < tb) ? -1 : (ta > tb) ? 1 : 0;
}

// Quita espacios y \r al final.
static void rstrip(char* s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t'))
        s[--n] = 0;
}

// Parsea una línea .lrc: puede tener varios [mm:ss.xx] y al final el texto.
static void parseLrcLine(char* line) {
    // recolectar tiempos al inicio
    int64_t times[16]; int nt = 0;
    char* p = line;
    while (*p == '[') {
        char* close = strchr(p, ']');
        if (!close) break;
        *close = 0;
        // contenido entre [ y ]
        char* inside = p + 1;
        int mm = 0, ss = 0, cs = 0;
        // [ar:..]/[ti:..] etc -> no numérico => metadato, ignorar línea entera
        if (isdigit((unsigned char)inside[0])) {
            // formatos mm:ss, mm:ss.xx, mm:ss.xxx
            if (sscanf(inside, "%d:%d.%d", &mm, &ss, &cs) >= 2) {
                int64_t ms = (int64_t)mm * 60000 + (int64_t)ss * 1000;
                // cs puede ser centésimas (2 díg) o milésimas (3 díg)
                if (cs > 0) ms += (cs < 100) ? cs * 10 : cs;
                if (nt < 16) times[nt++] = ms;
            }
        }
        p = close + 1;
    }
    rstrip(p);
    if (nt == 0) return;          // sin tiempos: era metadato o vacío
    if (p[0] == 0) return;        // sin texto
    for (int i = 0; i < nt; i++) addLine(times[i], p);
}

static bool loadFile(const char* path, bool isLrc) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    char buf[512];
    while (fgets(buf, sizeof(buf), f)) {
        if (isLrc) {
            parseLrcLine(buf);
        } else {
            rstrip(buf);
            addLine(-1, buf);     // texto plano (incluye líneas vacías como separador)
        }
    }
    fclose(f);

    if (s_count == 0) return false;
    if (isLrc) {
        qsort(s_lines, s_count, sizeof(LyLine), cmpLine);
        s_synced = true;
    }
    return true;
}

// Reemplaza la extensión de songPath por newExt en dst.
static void swapExt(char* dst, size_t n, const char* songPath, const char* newExt) {
    snprintf(dst, n, "%s", songPath);
    char* dot = strrchr(dst, '.');
    if (dot) *dot = 0;
    strncat(dst, newExt, n - strlen(dst) - 1);
}

bool lyricsLoad(const char* songPath) {
    lyricsClear();
    char path[600];

    swapExt(path, sizeof(path), songPath, ".lrc");
    if (loadFile(path, true)) return true;

    lyricsClear();
    swapExt(path, sizeof(path), songPath, ".txt");
    if (loadFile(path, false)) return true;

    lyricsClear();
    return false;
}

int lyricsActiveIndex(uint32_t currentMs) {
    if (!s_synced || s_count == 0) return -1;
    int idx = -1;
    for (int i = 0; i < s_count; i++) {
        if (s_lines[i].timeMs <= (int64_t)currentMs) idx = i;
        else break;
    }
    return idx;
}

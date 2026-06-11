#include "playlist.h"
#include "decoder.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

static Track s_tracks[PL_MAX_TRACKS];
static int   s_count = 0;
static Album s_albums[PL_MAX_ALBUMS];
static int   s_albumCount = 0;

// --- lista temporal de carpetas-álbum (las que contienen audio) ---
static char  s_dirs[PL_MAX_ALBUMS][PL_MAX_PATH];
static int   s_dirCount = 0;

int playlistCount(void) { return s_count; }
const Track* playlistGet(int i) { return (i<0||i>=s_count)?NULL:&s_tracks[i]; }
int albumCount(void) { return s_albumCount; }
const Album* albumGet(int i) { return (i<0||i>=s_albumCount)?NULL:&s_albums[i]; }

static bool isDir(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
static bool fileExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool dirHasAudio(const char* dir) {
    DIR* dp = opendir(dir);
    if (!dp) return false;
    struct dirent* e;
    bool found = false;
    while ((e = readdir(dp))) {
        if (e->d_name[0] == '.') continue;
        if (decoderDetect(e->d_name) != FMT_NONE) { found = true; break; }
    }
    closedir(dp);
    return found;
}

// DFS: registra carpetas que contienen audio (incluida root si aplica).
static void collectDirs(const char* dir, int depth) {
    if (depth > 6 || s_dirCount >= PL_MAX_ALBUMS) return;
    if (dirHasAudio(dir)) {
        snprintf(s_dirs[s_dirCount++], PL_MAX_PATH, "%s", dir);
        if (s_dirCount >= PL_MAX_ALBUMS) return;
    }
    DIR* dp = opendir(dir);
    if (!dp) return;
    struct dirent* e;
    char sub[PL_MAX_PATH];
    while ((e = readdir(dp))) {
        if (e->d_name[0] == '.') continue;
        snprintf(sub, sizeof(sub), "%s/%s", dir, e->d_name);
        if (isDir(sub)) collectDirs(sub, depth + 1);
    }
    closedir(dp);
}

static int cmpStr(const void* a, const void* b) {
    return strcasecmp((const char*)a, (const char*)b);
}
static int cmpTrackFile(const void* a, const void* b) {
    return strcasecmp(((const Track*)a)->file, ((const Track*)b)->file);
}

// Quita la extensión.
static void stripExt(char* s) { char* d = strrchr(s, '.'); if (d) *d = 0; }

// Nombre base de una ruta (después del último '/').
static const char* baseName(const char* path) {
    const char* s = strrchr(path, '/');
    return s ? s + 1 : path;
}

// Parsea "NN - Artista - Título" / "NN - Título" / "Título".
static void parseName(const char* file, char* title, size_t tn, char* artist, size_t an) {
    char buf[PL_MAX_NAME];
    snprintf(buf, sizeof(buf), "%s", file);
    stripExt(buf);
    artist[0] = 0;

    char* p = buf;
    // saltar número de pista inicial: "01", "01 -", "01.", "01 "
    char* q = p;
    while (*q >= '0' && *q <= '9') q++;
    if (q > p) {
        char* r = q;
        while (*r == ' ' || *r == '-' || *r == '.' || *r == '_') r++;
        if (r > q) p = r;   // hubo separador tras el número
    }

    // ¿queda "Artista - Título"?
    char* sep = strstr(p, " - ");
    if (sep) {
        size_t alen = (size_t)(sep - p);
        if (alen >= an) alen = an - 1;
        memcpy(artist, p, alen); artist[alen] = 0;
        snprintf(title, tn, "%s", sep + 3);
    } else {
        snprintf(title, tn, "%s", p);
    }
}

// Busca carátula en la carpeta del álbum.
static void findCover(const char* dir, char* out, size_t n) {
    const char* names[] = { "cover.jpg", "cover.png", "folder.jpg", "folder.png",
                            "Cover.jpg", "Folder.jpg", "front.jpg", "AlbumArt.jpg" };
    char p[PL_MAX_PATH];
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); i++) {
        snprintf(p, sizeof(p), "%s/%s", dir, names[i]);
        if (fileExists(p)) { snprintf(out, n, "%s", p); return; }
    }
    out[0] = 0;
}

int playlistScan(const char* root) {
    s_count = 0; s_albumCount = 0; s_dirCount = 0;

    collectDirs(root, 0);
    if (s_dirCount == 0) return 0;

    // ordenar carpetas-álbum por nombre completo (agrupa artista/álbum)
    qsort(s_dirs, s_dirCount, PL_MAX_PATH, cmpStr);

    for (int d = 0; d < s_dirCount && s_albumCount < PL_MAX_ALBUMS; d++) {
        const char* dir = s_dirs[d];

        // recolectar tracks de esta carpeta
        int startIdx = s_count;
        DIR* dp = opendir(dir);
        if (!dp) continue;
        struct dirent* e;
        while ((e = readdir(dp)) && s_count < PL_MAX_TRACKS) {
            if (e->d_name[0] == '.') continue;
            if (decoderDetect(e->d_name) == FMT_NONE) continue;
            Track* t = &s_tracks[s_count];
            snprintf(t->file, PL_MAX_NAME, "%s", e->d_name);
            snprintf(t->path, PL_MAX_PATH, "%s/%s", dir, e->d_name);
            parseName(t->file, t->title, PL_MAX_NAME, t->artist, sizeof(t->artist));
            t->album = s_albumCount;
            s_count++;
        }
        closedir(dp);

        int n = s_count - startIdx;
        if (n <= 0) continue;

        // ordenar los tracks del álbum por nombre de archivo
        qsort(&s_tracks[startIdx], n, sizeof(Track), cmpTrackFile);
        for (int i = startIdx; i < s_count; i++) s_tracks[i].album = s_albumCount;

        Album* al = &s_albums[s_albumCount];
        const char* nm = baseName(dir);
        if (nm[0] == 0) nm = "(raiz)";
        snprintf(al->name, PL_MAX_NAME, "%s", nm);
        findCover(dir, al->coverPath, PL_MAX_PATH);
        al->firstTrack = startIdx;
        al->count = n;
        s_albumCount++;
    }
    return s_count;
}

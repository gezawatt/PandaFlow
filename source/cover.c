#include "cover.h"
#include <3ds.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"

#define COVER_SZ 128   // textura cuadrada potencia de 2

static C3D_Tex            s_tex;
static Tex3DS_SubTexture  s_sub;
static C2D_Image          s_img;
static bool               s_has = false;

bool coverHas(void) { return s_has; }

void coverClear(void) {
    if (s_has) { C3D_TexDelete(&s_tex); s_has = false; }
}

bool coverLoad(const char* path) {
    coverClear();
    if (!path || !path[0]) return false;

    int w = 0, h = 0, c = 0;
    unsigned char* img = stbi_load(path, &w, &h, &c, 4);
    if (!img || w <= 0 || h <= 0) { if (img) stbi_image_free(img); return false; }

    const int S = COVER_SZ;
    u32* lin = (u32*)linearAlloc(S * S * 4);
    if (!lin) { stbi_image_free(img); return false; }

    // reescalado nearest a SxS + reordenado a 0xRRGGBBAA (lo que espera el transfer)
    for (int y = 0; y < S; y++) {
        int sy = y * h / S;
        for (int x = 0; x < S; x++) {
            int sx = x * w / S;
            unsigned char* p = img + ((size_t)sy * w + sx) * 4;
            lin[y * S + x] = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
        }
    }
    stbi_image_free(img);

    GSPGPU_FlushDataCache(lin, S * S * 4);
    if (!C3D_TexInit(&s_tex, S, S, GPU_RGBA8)) { linearFree(lin); return false; }
    C3D_TexSetFilter(&s_tex, GPU_LINEAR, GPU_LINEAR);

    // linear -> tiled. La orientacion correcta ya la da la subtextura (top=1/bottom=0);
    // NO aplicar FLIP_VERT aqui o la caratula sale dada vuelta (doble flip).
    C3D_SyncDisplayTransfer(
        lin, GX_BUFFER_DIM(S, S),
        (u32*)s_tex.data, GX_BUFFER_DIM(S, S),
        (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
         GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
         GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
         GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)));
    linearFree(lin);

    s_sub.width  = S; s_sub.height = S;
    s_sub.left = 0.0f; s_sub.top = 1.0f; s_sub.right = 1.0f; s_sub.bottom = 0.0f;
    s_img.tex = &s_tex; s_img.subtex = &s_sub;
    s_has = true;
    return true;
}

void coverDraw(float x, float y, float depth, float size) {
    if (!s_has) return;
    float sc = size / (float)COVER_SZ;
    C2D_DrawImageAt(s_img, x, y, depth, NULL, sc, sc);
}

// Unidad de compilacion unica para las implementaciones de dr_libs.
// (Las demas TUs incluyen los headers sin el macro IMPLEMENTATION = solo declaraciones.)

// No necesitamos la salida en float ni las APIs de escritura; recortamos para
// reducir tamano y tiempo de compilacion.
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION

#define DR_FLAC_NO_OGG          // FLAC dentro de OGG: no lo usamos
#define DRWAV_ONLY_PCM_OFF      // (placeholder, dr_wav ignora desconocidos)

#include "dr_mp3.h"
#include "dr_flac.h"
#include "dr_wav.h"

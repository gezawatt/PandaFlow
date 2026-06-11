# PandaFlow — Reproductor de música para 3DS

[![build](https://github.com/PandaAkiraNakai/PandaFlow/actions/workflows/build.yml/badge.svg)](https://github.com/PandaAkiraNakai/PandaFlow/actions/workflows/build.yml)
![platform](https://img.shields.io/badge/platform-Nintendo%203DS-D12228)
![license](https://img.shields.io/badge/license-GPLv2-blue)

Reproductor de música homebrew para Nintendo 3DS/2DS que **sigue sonando con la
tapa cerrada** (igual que la app oficial "Nintendo 3DS Sound").

<!-- profile-excerpt -->
**Reproductor de música homebrew para Nintendo 3DS** que **sigue sonando con la tapa cerrada** (como la app oficial de Sonido). Escrito en **C** con **devkitPro / libctru + citro2d**: audio por streaming **NDSP** desde un hilo dedicado, **MP3/FLAC/WAV** (dr_libs) y **AAC** (faad2 + minimp4), biblioteca recursiva por **álbumes** con **carátulas** (stb_image → textura GPU), **letras** `.lrc` sincronizadas / `.txt`, **shuffle/repeat** y controles táctiles. UI estilo *Mixtape* con disco de vinilo. Compila a `.3dsx` y `.cia`, con CI en GitHub Actions.
<!-- /profile-excerpt -->

## Característica clave: tapa cerrada

Al iniciar la reproducción se llama a `aptSetSleepAllowed(false)`, lo que impide
que la consola entre en suspensión al cerrar la tapa: la pantalla se apaga pero
la CPU y el audio siguen corriendo. Al pausar/detener/salir se restaura con
`aptSetSleepAllowed(true)` para no gastar batería. El audio se alimenta desde un
**hilo dedicado** (`source/audio.c`), independiente del render, así que no se
corta aunque la pantalla esté apagada.

## Formatos soportados

- **MP3, FLAC, WAV** vía [dr_libs](https://github.com/mackron/dr_libs)
  (single-header, dominio público) — ver `include/dr_*.h`.
- **AAC** (`.m4a` / `.mp4` / `.aac`) vía **faad2** + **minimp4** (demux MP4) —
  ver `source/aac.c`. `lib/libfaad.a` es faad2 cross-compilado para 3DS.
- Cualquier número de canales se convierte a estéreo en `source/decoder.c`.

## Letras (lyrics)

Pon junto al audio un archivo con el **mismo nombre**:
- `cancion.lrc` → letras **sincronizadas** (auto-scroll, línea actual resaltada).
- `cancion.txt` → texto plano (desplazable con la cruceta).

Pulsa **SELECT** para mostrar/ocultar las letras (pantalla inferior).
Ver `source/lyrics.c`.

## Biblioteca (subcarpetas / álbumes)

Escanea `sdmc:/music` de forma **recursiva**: cada subcarpeta con audio se trata
como un **álbum**. Estructura típica soportada:

```
sdmc:/music/
  Circles (Deluxe) - Mac Miller/
    01 - Mac Miller - Circles.flac
    01 - Mac Miller - Circles.lrc      (letras)
    cover.jpg                          (carátula del álbum)
    ...
```

- **Carátula**: `cover.jpg` / `cover.png` / `folder.jpg` en la carpeta del álbum
  (se muestra en "ahora suena"). Decodificada con stb_image → `source/cover.c`.
- **Metadatos**: el Nº de pista, artista y título se derivan del nombre de archivo
  (`NN - Artista - Título`). La lista agrupa por álbum con encabezados.

## Navegación (pantalla inferior)

- **Vista de álbumes** (carpetas) → entra a un álbum con **A** o tocándolo.
- **Vista de tracks** del álbum → **A**/toque reproduce; **B** o ◀ (toque) vuelve.
- **Barra de transporte táctil** abajo: ⏮ · ▶/II · ⏭ · ■ · **SHUF** · **RPT**.
- **Shuffle** y **Repeat** (off / todo `*` / una `1`).

> [!note] Audio fluido
> En New 3DS se llama `osSetSpeedupEnable(true)` (804 MHz + L2): sin eso, FLAC/AAC
> se escucha **cortado / con pops** por falta de CPU.

## Uso

1. Crea `sdmc:/music` y mete carpetas de álbumes (o sueltos).
2. Abre PandaFlow. Tracklist abajo (con álbumes), "ahora suena" + carátula arriba.

### Controles

| Botón | Acción |
|---|---|
| D-Pad ↑/↓ | Mover cursor en la lista |
| D-Pad ←/→ | Página anterior / siguiente |
| A | Reproducir la canción seleccionada |
| Y | Pausa / reanudar |
| X | Detener |
| L / R | Retroceder / adelantar 10 s |
| ZL / ZR | Pista anterior / siguiente (New 3DS) |
| A | Entrar al álbum / reproducir track |
| B | Volver a la lista de álbumes |
| SELECT | Mostrar / ocultar letras |
| Barra táctil | ⏮ ▶/II ⏭ ■ SHUF RPT |
| START | Salir |

## Compilar

Requiere [devkitPro](https://devkitpro.org) con el grupo `3ds-dev`
(devkitARM + libctru + citro2d).

```sh
source /etc/profile.d/devkit-env.sh   # define DEVKITPRO / DEVKITARM

# Dependencias que no se versionan (se reconstruyen/descargan):
./scripts/build-faad2.sh   # cross-compila faad2 -> lib/libfaad.a + include/neaacdec.h
./scripts/fetch-tools.sh   # descarga makerom y bannertool -> tools/  (solo para 'make cia')

make          # genera pandaflow.3dsx (Homebrew Launcher)
make cia      # genera pandaflow.cia (instalable en el menú HOME)
make clean
```

El `.cia` usa `tools/bannertool` y `tools/makerom` (incluidos) + los assets de
`meta/` (icono, banner, audio, `app.rsf`).

## Estructura

```
source/
  main.c       UI (citro2d), casete/carátula, shuffle/repeat, control
  audio.c      motor NDSP, hilo de audio, lógica de "tapa cerrada"
  decoder.c    abstracción de formatos -> estéreo PCM16
  aac.c        AAC (.m4a/.aac) con faad2 + minimp4
  lyrics.c     letras .lrc (sincronizadas) / .txt
  cover.c      carátulas: stb_image -> textura GPU (C3D_Tex)
  playlist.c   escaneo RECURSIVO de sdmc:/music (álbumes)
  dr_impl.c    implementaciones de dr_libs (TU única)
include/       headers propios + dr_*.h, minimp4.h, neaacdec.h, stb_image.h
lib/           libfaad.a (se genera con scripts/build-faad2.sh)
meta/          icon.png, banner.png, banner.wav, app.rsf
scripts/       build-faad2.sh, fetch-tools.sh
tools/         bannertool, makerom (se bajan con scripts/fetch-tools.sh)
```

## Créditos y licencia

Hecho con [devkitPro](https://devkitpro.org) (libctru, citro2d). Componentes de
terceros incluidos:

| Componente | Uso | Licencia |
|---|---|---|
| [dr_libs](https://github.com/mackron/dr_libs) | MP3/FLAC/WAV | dominio público / MIT-0 |
| [minimp4](https://github.com/lieff/minimp4) | demux MP4/M4A | CC0 / dominio público |
| [faad2](https://github.com/knik0/faad2) | decodificación AAC | **GPLv2** |
| [stb_image](https://github.com/nothings/stb) | carátulas JPG/PNG | dominio público / MIT |

Como el binario enlaza **faad2 (GPLv2)**, **PandaFlow se distribuye bajo
GPLv2** (ver [`LICENSE`](LICENSE)).

> Inspiración visual: estética del juego *Mixtape* (Beethoven & Dinosaur /
> Annapurna). Proyecto sin relación oficial con ellos.

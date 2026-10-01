# PandaFlow — Music player for 3DS

*This app is an English translation of the music player by PandaAkiraNakai*

[![build](https://github.com/PandaAkiraNakai/PandaFlow/actions/workflows/build.yml/badge.svg)](https://github.com/PandaAkiraNakai/PandaFlow/actions/workflows/build.yml)
![platform](https://img.shields.io/badge/platform-Nintendo%203DS-D12228)
![license](https://img.shields.io/badge/license-GPLv2-blue)

Homebrew music player for Nintendo 3DS/2DS that **keeps playing with the lid closed**
(just like the official "Nintendo 3DS Sound" app).

<!-- profile-excerpt -->
**Homebrew music player for Nintendo 3DS** that **keeps playing with the lid closed**
(like the official Sound app). Written in **C** with **devkitPro / libctru + citro2d**:
streaming audio with **NDSP** on a dedicated thread, **MP3/FLAC/WAV** support via
**dr_libs**, **AAC** via **faad2 + minimp4**, recursive album library with covers
(**stb_image** to GPU texture), synced `.lrc` / `.txt` lyrics, **shuffle/repeat** and
multi-touch controls. UI styled like *Mixtape* with a vinyl record. Builds to a `.3dsx`
(Homebrew Launcher) file, with CI via GitHub Actions and distribution through
**Universal Updater**.
<!-- /profile-excerpt -->

## Key feature: lid-closed playback

When playback starts, `aptSetSleepAllowed(false)` is called to prevent the console
from entering sleep mode when the lid is closed: the screen turns off, but the CPU and
audio continue running. When paused/stopped/exiting, it restores `aptSetSleepAllowed(true)`
to save battery. Audio is fed from a **dedicated thread** (`source/audio.c`),
independent from rendering, so playback does not cut out even when the screen is off.

## Supported formats

- **MP3, FLAC, WAV** via [dr_libs](https://github.com/mackron/dr_libs)
  (single-header, public domain) — see `include/dr_*.h`.
- **AAC** (`.m4a` / `.mp4` / `.aac`) via **faad2** + **minimp4** (MP4 demux) —
  see `source/aac.c`. `lib/libfaad.a` is the cross-compiled faad2 for 3DS.
- Any number of channels is converted to stereo in `source/decoder.c`.

## Lyrics

Place a file next to the audio with the **same name**:
- `song.lrc` → **synced lyrics** (auto-scroll, current line highlighted).
- `song.txt` → plain text (scrollable with the D-pad).

Press **SELECT** to show/hide the lyrics (lower screen).
See `source/lyrics.c`.

## Library (subfolders / albums)

It scans `sdmc:/music` **recursively**: each subfolder containing audio is treated as
an **album**. A typical supported structure looks like:

```
sdmc:/music/
  Circles (Deluxe) - Mac Miller/
    01 - Mac Miller - Circles.flac
    01 - Mac Miller - Circles.lrc      (lyrics)
    cover.jpg                          (album art)
    ...
```

- **Cover art**: `cover.jpg` / `cover.png` / `folder.jpg` in the album folder
  (shown in "now playing"). Decoded with stb_image → `source/cover.c`.
- **Metadata**: track number, artist, and title are derived from the filename
  (`NN - Artist - Title`). The list groups by album with headers.

## Navigation (lower screen)

- **Album view** (folders) → enter an album with **A** or by tapping it.
- **Track list view** → **A**/tap plays; **B** or ◀ (tap) goes back.
- **Touch transport bar** at the bottom: ⏮ · ▶/II · ⏭ · ■ · **SHUF** · **RPT**.
- **Shuffle** and **Repeat** (off / all `*` / one `1`).

> [!note] Smooth audio
> On New 3DS, `osSetSpeedupEnable(true)` is called (804 MHz + L2): without it,
> FLAC/AAC can sound **choppy / poppy** due to CPU limits.

## Usage

1. Create `sdmc:/music` and place album folders (or loose files).
2. Open PandaFlow. The tracklist is shown below (with albums), while the current song and cover appear above.

### Controls

| Button | Action |
|---|---|
| D-Pad ↑/↓ | Move cursor in the list |
| D-Pad ←/→ | Previous / next page |
| A | Play the selected song |
| Y | Pause / resume |
| X | Stop |
| A | Open album / play track |
| B | Return to the album list |
| SELECT | Show / hide lyrics |
| **R / L** | **Raise / lower volume** (5% per press; all 3DS models) |
| **C-stick ↑/↓** | **Raise / lower volume** (optional, New 3DS) |
| Touch bar | ⏮ ▶/II ⏭ ■ SHUF RPT |
| START | Exit |

## Installation

PandaFlow is distributed as a **`.3dsx`** file (homebrew for the Homebrew Launcher), not as a `.cia`.
You need a console with CFW (Luma3DS) and the Homebrew Launcher.

### Universal Updater (recommended)

1. Open **Universal Updater** on your 3DS.
2. Search for **PandaFlow** and install it.

Universal Updater downloads the `.3dsx` from the
[Releases](https://github.com/gezawatt/PandaFlow/releases) and places it in
`sdmc:/3ds/PandaFlow/`. Then open it from the **Homebrew Launcher**.

### Manual

1. Download `pandaflow.3dsx` from the latest
   [release](https://github.com/gezawatt/PandaFlow/releases/latest).
2. Copy it to `sdmc:/3ds/` on the microSD.
3. Open it from the **Homebrew Launcher**.

> **Note:** FBI does not help here — it installs `.cia` packages (HOME menu titles), not
> `.3dsx`. For homebrew `.3dsx`, use Universal Updater or manual copying.

## Build

Requires [devkitPro](https://devkitpro.org) with the `3ds-dev` group
(devkitARM + libctru + citro2d).

```sh
source /etc/profile.d/devkit-env.sh   # defines DEVKITPRO / DEVKITARM

# Unversioned dependency that must be rebuilt:
./scripts/build-faad2.sh   # cross-compiles faad2 -> lib/libfaad.a + include/neaacdec.h

make          # produces pandaflow.3dsx (Homebrew Launcher)
make clean
```

The `.3dsx` embeds the icon from `meta/icon.png` (Homebrew Launcher).

## Structure

```
source/
  main.c       UI (citro2d), cassette / cover, shuffle/repeat, controls
  audio.c      NDSP engine, audio thread, "lid closed" logic
  decoder.c    format abstraction -> stereo PCM16
  aac.c        AAC (.m4a/.aac) via faad2 + minimp4
  lyrics.c     .lrc (synced) / .txt lyrics
  cover.c      covers: stb_image -> GPU texture (C3D_Tex)
  playlist.c   recursive scan of sdmc:/music (albums)
  dr_impl.c    dr_libs implementations (single file)
include/       project headers + dr_*.h, minimp4.h, neaacdec.h, stb_image.h
lib/           libfaad.a (built by scripts/build-faad2.sh)
meta/          icon.png (icon for .3dsx / Universal-DB)
scripts/       build-faad2.sh
```

## Credits and license

Built with [devkitPro](https://devkitpro.org) (libctru, citro2d). Third-party components included:

| Component | Use | License |
|---|---|---|
| [dr_libs](https://github.com/mackron/dr_libs) | MP3/FLAC/WAV | public domain / MIT-0 |
| [minimp4](https://github.com/lieff/minimp4) | MP4/M4A demux | CC0 / public domain |
| [faad2](https://github.com/knik0/faad2) | AAC decoding | **GPLv2** |
| [stb_image](https://github.com/nothings/stb) | JPG/PNG cover art | public domain / MIT |

Because the binary links **faad2 (GPLv2)**, **PandaFlow is distributed under GPLv2**
(see [`LICENSE`](LICENSE)).

> Visual inspiration: the aesthetic of *Mixtape* (Beethoven & Dinosaur / Annapurna).
> This project is not officially affiliated with them.

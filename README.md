<div align="center">

<h1>VIBE-FI</h1>

<p><strong>High-Performance Terminal Music Player</strong><br>
<em>Engineered in modern C++17 with <code>libmpv</code> and <code>ncurses</code> for Linux and macOS.</em></p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue.svg?style=flat-square" alt="MIT License" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square&logo=c%2B%2B" alt="C++17" />
  <img src="https://img.shields.io/badge/Audio-libmpv-orange.svg?style=flat-square" alt="libmpv" />
  <img src="https://img.shields.io/badge/UI-ncurses-yellow.svg?style=flat-square" alt="ncurses" />
  <img src="https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey.svg?style=flat-square" alt="Platform" />
</p>

</div>

---

## 📻 Overview

```
╭─────────────────────────────────────────────────────────────────────────────╮
│ VISUALIZER: NEON FLAME [♫ ● ○ ○ ○ ] [84% PEAK]                              │
│                                                                             │
│                 ✦              ✦                                            │
│               ▂ █ ▅          ▅ █ ▂                                          │
│           ▂ ▄ █ █ █ █      █ █ █ █ ▄ ▂                                      │
│         ▃ █ █ █ █ █ █ █  █ █ █ █ █ █ █ ▃                                    │
│ ─────────────────────────────────────────────────────────────────────────── │
╰─────────────────────────────────────────────────────────────────────────────╯
╭─────────────────────────────────────────────────────────────────────────────╮
│ LYRICS: Bohemian Rhapsody — Queen                                           │
│                                                                             │
│     Is this the real life?                                                  │
│     Is this just fantasy?                                                   │
│   ▸ Caught in a landslide, no escape from reality ◂                         │
│     Open your eyes, look up to the skies and see...                         │
│                                                                             │
╰─────────────────────────────────────────────────────────────────────────────╯
╭─────────────────────────────────────────────────────────────────────────────╮
│ [SPACE] Pause  [←/→] Seek  [S] Search  [L] Library  [P] Playlists  [V] Mode │
╰─────────────────────────────────────────────────────────────────────────────╯
```

---

## ⚡ Features

- 📡 **Direct YouTube Streaming**: Stream audio directly through `yt-dlp` without downloading or rendering video streams.
- 🗂️ **Local Audio Library**: Non-blocking browser supporting FLAC, MP3, WAV, M4A, OGG, Opus, AAC, ALAC, and AIFF with in-memory metadata caching.
- 🎙️ **Synced & Plain Lyrics**: Fetches timestamped `.lrc` and plain lyrics from `lrclib.net` with live auto-scrolling and local caching.
- 🌊 **Audio-Reactive Visualizers**: 3 dynamic modes (`Neon Flame`, `Stereo Bars`, and `Pulse`) rendered using UTF-8 fractional sub-blocks and gravity-accelerated peak caps.
- 🎚️ **D-Bus MPRIS Support**: Native media key and system remote control integration (`playerctl`) through a decoupled, thread-safe message queue.
- 👾 **Discord Rich Presence**: Real-time track status on your Discord profile via native Unix IPC sockets without external SDK bloat.
- 📑 **Smart Playlist Management**: Create, reorder, delete, and move songs across playlists, with one-click export to standard `.m3u` files.
- ⏱️ **Session Persistence**: Saves volume, playback position, and track history. Press <kbd>R</kbd> on startup to restore playback and lyrics immediately.

---

## 📐 Architecture

Vibe-Fi follows a modular, domain-driven architecture designed for high throughput and zero UI stutter:

```
                             +-------------------+
                             |     main.cpp      |
                             |  (CLI Bootstrap)  |
                             +---------+---------+
                                       |
                   +-------------------+-------------------+
                   |                                       |
         +---------v---------+                   +---------v---------+
         |     src/ui/       |                   |    src/core/      |
         |   UI (ncurses)    |                   |  Player (libmpv)  |
         +---------+---------+                   +---------+---------+
                   |                                       |
       +-----------+-----------+                           |
       |                       |                           |
+------v------+       +--------v--------+         +--------v--------+
|   Views     |       |   Visualizer    |<--------+ Audio Analysis  |
| (Playback,  |       | (Physics Engine)|         | (@astats filter)|
| Library,    |       +-----------------+         +-----------------+
| Queue, etc) |
+------+------+
       |
+------v-------------------------------------------------------+
|                       src/services/                          |
|  - Library: Local directory indexing & duration cache        |
|  - Lyrics: lrclib.net synced/plain parser & disk cache       |
|  - PlaylistManager: Custom lists (.txt) & M3U export         |
|  - Search: Parameterized yt-dlp search query pipeline        |
+--------------------------------------------------------------+
       |
+------v-------------------------------------------------------+
|                     src/integrations/                        |
|  - MPRIS: Thread-safe D-Bus interface for Linux media keys   |
|  - Discord RPC: Native Unix domain socket IPC                |
+--------------------------------------------------------------+
```

### Subsystems

| Module | Location | Responsibilities |
| :--- | :--- | :--- |
| 🎛️ **Audio Core** | `src/core/` | `libmpv` initialization, playback state, seeking, and real-time `@astats` audio filtering (RMS, peak, stereo split, and zero-crossing detection). |
| 🌌 **Visualizer Engine** | `src/ui/visualizer.*` | Decoupled physics renderer. Calculates dynamic frequency spectrums, gravity peak caps, flutter harmonics, and the rhythmic header metronome. |
| 🖥️ **Terminal UI** | `src/ui/ui.*` | Window hierarchy, theme color pairs, keyboard event loop, and viewport management. |
| 📡 **Data Services** | `src/services/` | External integrations: `library` indexing, `lyrics` fetching, `playlist_manager` CRUD, and `search` query formatting. |
| 🔌 **Desktop Integrations**| `src/integrations/`| System hooks: Linux `mpris` D-Bus media key listener and `discord_rpc` Unix IPC daemon. |
| 🛠️ **Utilities** | `src/utils/` | String sanitation, shell argument escaping, dynamic executable discovery, and safe string-to-float conversions. |

> [!TIP]
> For in-depth technical documentation and subsystem lifecycles, see [ARCHITECTURE.md](ARCHITECTURE.md). For the AI agent mindmap, state transitions, and developer playbooks, see [AGENTS.md](AGENTS.md).

---

## 🧰 Requirements

Ensure the following build tools and libraries are installed:

- **C++ Compiler**: GCC (>= 8) or Clang (>= 7) supporting C++17
- **Build System**: CMake (>= 3.16) and `pkg-config`
- **Libraries**: `libmpv` and `ncurses` (with UTF-8 support)
- **Runtime Tools**: `yt-dlp` (for YouTube extraction) and `ffmpeg` / `ffprobe`
- **Optional**: `libdbus-1-dev` (Linux only, for media key controls)

### Distribution Commands

```bash
# Arch Linux / Manjaro
sudo pacman -S base-devel cmake mpv ncurses yt-dlp ffmpeg dbus pkgconf

# Ubuntu / Debian / Mint
sudo apt update && sudo apt install build-essential cmake libmpv-dev libncurses-dev libdbus-1-dev mpv ffmpeg yt-dlp curl pkg-config

# Fedora / RHEL
sudo dnf install gcc-c++ cmake mpv-devel ncurses-devel dbus-devel mpv ffmpeg yt-dlp curl pkgconf-pkg-config

# openSUSE
sudo zypper install gcc-c++ cmake mpv-devel ncurses-devel dbus-1-devel mpv ffmpeg yt-dlp curl pkg-config

# macOS (Homebrew)
brew install cmake mpv ncurses yt-dlp ffmpeg pkg-config
```

> [!NOTE]
> On macOS, D-Bus MPRIS is automatically disabled at compile time. Audio playback, visualizers, library browsing, YouTube search, and Discord RPC function natively.

---

## 💿 Installation

### Method 1: Automated Script (Recommended)

```bash
git clone https://github.com/Swadesh-c0de/vibe-fi.git
cd vibe-fi
chmod +x install.sh
./install.sh
```

### Method 2: Manual CMake Build

```bash
git clone https://github.com/Swadesh-c0de/vibe-fi.git
cd vibe-fi

# Configure release build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build using all available cores
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
```

### Installing the Binary

```bash
# Option A: Install for current user (no root required)
mkdir -p ~/.local/bin
install -m 755 ./build/vibe_fi ~/.local/bin/vibe

# Option B: Install system-wide
sudo cmake --install build --prefix /usr/local
```

### Uninstalling

```bash
# Current user:
rm -f ~/.local/bin/vibe

# System-wide:
sudo rm -f /usr/local/bin/vibe /usr/local/bin/vibe_fi
```

---

## 🕹️ Usage

```bash
# Launch interactive dashboard
vibe

# Search YouTube and play top result immediately
vibe "miles davis kind of blue"

# Stream YouTube URL directly
vibe "https://www.youtube.com/watch?v=5qap5aO4i9A"

# Play local audio file
vibe ~/Music/album/track01.flac
```

> [!TIP]
> Passing search keywords directly from your terminal (`vibe "song name"`) automatically begins playback of the top hit and loads the remaining search results into your upcoming queue.

---

## ⌨️ Keybindings

### 🌐 Global Controls

| Key | Action |
| :---: | :--- |
| <kbd>ESC</kbd> | Return to previous screen / Cancel input / Exit view |
| <kbd>↑</kbd> / <kbd>k</kbd> | Navigate up |
| <kbd>↓</kbd> / <kbd>j</kbd> | Navigate down |
| <kbd>ENTER</kbd> | Select item / Open directory / Play track |

### 🎚️ Playback Screen

| Key | Action |
| :---: | :--- |
| <kbd>SPACE</kbd> | Toggle Play / Pause |
| <kbd>←</kbd> / <kbd>→</kbd> | Seek backward / forward 5 seconds |
| <kbd>+</kbd> / <kbd>-</kbd> | Adjust volume |
| <kbd>S</kbd> | Search YouTube |
| <kbd>L</kbd> | Open local music library |
| <kbd>P</kbd> | Open playlist manager |
| <kbd>C</kbd> | View upcoming play queue |
| <kbd>U</kbd> | Play from YouTube URL |
| <kbd>V</kbd> | Cycle visualizer mode (`Neon Flame` ➔ `Stereo Bars` ➔ `Pulse`) |
| <kbd>T</kbd> | Cycle color theme (`Midnight` ➔ `Matrix` ➔ `Nord`) |
| <kbd>O</kbd> | Toggle autoplay (`ON` / `OFF`) |
| <kbd>R</kbd> | Replay track from beginning (or resume last session from intro) |
| <kbd>Q</kbd> | Jump to active playlist or search results list |
| <kbd>↑</kbd> / <kbd>↓</kbd> | Manually scroll lyrics view |

### 📚 Library & Playlists

| Context | Key | Action |
| :--- | :---: | :--- |
| **Library Browser** | <kbd>ENTER</kbd> | Play audio file / Enter directory |
| | <kbd>BKSP</kbd> / <kbd>h</kbd> | Navigate to parent directory |
| | <kbd>A</kbd> | Add highlighted file to a playlist |
| **Playlist List** | <kbd>ENTER</kbd> | Open playlist to inspect tracks |
| | <kbd>N</kbd> | Create a new playlist |
| | <kbd>R</kbd> | Rename selected playlist |
| | <kbd>D</kbd> | Delete selected playlist |
| | <kbd>E</kbd> | Export playlist to `.m3u` format |
| **Inside Playlist** | <kbd>ENTER</kbd> | Play selected track |
| | <kbd>D</kbd> | Remove track from playlist |
| | <kbd>M</kbd> | Move track to another playlist |
| **Queue Screen** | <kbd>ENTER</kbd> | Jump to and play queued track |
| | <kbd>D</kbd> | Remove item from upcoming queue |

---

## 🌌 Visualizer Modes

Press <kbd>V</kbd> to cycle between visualizer engines:

1. 🔥 **Neon Flame** *(Default)*:
   - Symmetrical center-outward frequency distribution.
   - Bass transients and kicks erupt upward in the center columns.
   - Vocal midranges undulate on flanking columns.
   - Hi-hats and percussion crackle at the outer wings.
   - Real-time 4-beat header metronome indicator (`[♫ ● ○ ○ ○ ]`).
   - Floating peak caps (`✦`, `▲`, `▔`) with gravity physics.

2. 📊 **Stereo Bars**:
   - Classic linear 8x sub-block graphic equalizer.
   - Left channel drives the left half, right channel drives the right half.
   - Floating peak hold bars.

3. 💫 **Pulse**:
   - Radial subwoofer ripple expanding dynamically on bass drops.

---

## 🗄️ Storage & Configuration

Vibe-Fi stores user data, playlist files, and caches under `~/.vibe-fi/`:

```
~/.vibe-fi/
├── state.ini                   # Saved session: track URL/path, position, volume, active playlist
├── playlists/                  # Plaintext playlist files (Title|URL|Duration)
│   ├── Chill.txt
│   └── Favorites.txt
└── cache/
    └── lyrics/                 # Cached LRC and JSON files from lrclib.net
        └── Queen_Bohemian+Rhapsody.json
```

- **Resuming Sessions**: Press <kbd>R</kbd> on the welcome screen to reload the exact track, volume, seek position, and synchronized lyrics from `state.ini`.
- **Portable Playlists**: Playlists are simple line-delimited text files. Copy the `~/.vibe-fi/playlists/` directory to back up or migrate playlists across systems.

---

## 🩺 Diagnostics & Troubleshooting

### 📡 `yt-dlp` returns extraction errors or 403 Forbidden
YouTube frequently updates video stream ciphers. Update `yt-dlp` to the latest release:
```bash
# Package manager:
sudo pacman -Syu yt-dlp         # Arch Linux
sudo apt install --only-upgrade yt-dlp  # Ubuntu/Debian

# Direct binary:
sudo yt-dlp -U
```

### ⏯️ Media keys (`playerctl`) not responding on Linux
Verify the D-Bus service is registered:
```bash
playerctl -l
# Should list: vibe_fi

# Test remote control:
playerctl --player=vibe_fi play-pause
playerctl --player=vibe_fi next
```

### 🎮 Discord Rich Presence not displaying
1. Verify the official Discord desktop client is open and logged in.
2. In Discord Settings: **Activity Privacy** ➔ Enable **Display current activity as a status message**.
3. On Linux, ensure `XDG_RUNTIME_DIR` is set (`echo $XDG_RUNTIME_DIR`).

### 🔲 Terminal borders or blocks look distorted
Set your terminal locale to UTF-8 with 256-color support:
```bash
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export TERM=xterm-256color
```

---

## 📜 License

Distributed under the [MIT License](LICENSE).

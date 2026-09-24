<div align="center">

<h4>Free, open-source terminal music player for Linux & macOS.</h4>

<hr>

<picture>
  <img alt="vibe-fi logo" src="assets/logo.svg" width="220" />
</picture>
<br><br>

<p align="center">
  <strong>A fast, lightweight music player for your terminal.</strong><br>
  <em>Stream YouTube audio, play local music files, watch real-time visualizers, and read synced lyrics — without opening a browser.</em>
</p>

<p align="center">
  <a href="https://github.com/Swadesh-c0de/vibe-fi/releases/tag/v1.1.2"><img src="https://img.shields.io/github/v/release/Swadesh-c0de/vibe-fi.svg?style=flat-square&color=6366f1" alt="Release" /></a> <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-38bdf8.svg?style=flat-square" alt="License" /></a> <img src="https://img.shields.io/badge/c%2B%2B-17-ec4899.svg?style=flat-square&logo=c%2B%2B&logoColor=white" alt="C++17" /> <img src="https://img.shields.io/badge/platform-linux%20%7C%20macos-10b981.svg?style=flat-square" alt="Platform" /> <a href="https://github.com/Swadesh-c0de/vibe-fi/stargazers"><img src="https://img.shields.io/github/stars/Swadesh-c0de/vibe-fi?style=flat-square&color=eab308" alt="GitHub Stars" /></a> <a href="https://github.com/Swadesh-c0de/vibe-fi/pulls"><img src="https://img.shields.io/badge/PRs-welcome-a855f7.svg?style=flat-square" alt="PRs Welcome" /></a>
</p>

<br><br>

<img src="assets/showcase.png" alt="Vibe-Fi Interface Screenshot" width="100%" />

</div>

---

## Demo

| Direct YouTube audio streaming, lossless local playback, reactive visualizers & synced lyrics |
| :-------------------------------------------------------------------------------------------: |
| <img src="assets/demo.gif" alt="Vibe-Fi Terminal Demo" width="100%" /> |

---

## Content

- [Demo](#demo)
- [Content](#content)
- [Why Vibe-Fi?](#why-vibe-fi)
- [Features](#features)
- [Comparison](#comparison)
- [Installation](#installation)
  - [Automated Script (Recommended)](#automated-script-recommended)
  - [GitHub Releases (v1.1.2)](#github-releases-v112)
  - [Building from Source](#building-from-source)
- [Usage & Workflow](#usage--workflow)
  - [CLI Commands & Search](#cli-commands--search)
  - [Window Manager Setup](#window-manager-setup)
- [Supported Systems](#supported-systems)
- [Hotkeys](#hotkeys)
  - [Global Navigation](#global-navigation)
  - [Playback Screen](#playback-screen)
  - [Library & Playlists](#library--playlists)
- [Themes & Visualizers](#themes--visualizers)
- [Configuration & Dotfiles](#configuration--dotfiles)
- [Architecture](#architecture)
- [Notes & Invariants](#notes--invariants)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [Thanks](#thanks)
- [Keep Vibing](#d-_-b-turn-up-the-volume--keep-vibing)

---

## Why Vibe-Fi?

Most desktop music players come with trade-offs:
- **Browser & Electron apps (Spotify, YouTube web)** consume hundreds of megabytes of RAM and use noticeable CPU in the background.
- **Traditional command-line players (`cmus`, `mpd`)** are lightweight, but require complex configuration files and external scripts just to stream a song from YouTube or show synced lyrics.

**Vibe-Fi brings everything together in one simple tool:**
- **Zero-Configuration YouTube Streaming**: Search and stream audio instantly via `yt-dlp`.
- **Fast and Lightweight**: Built with C++17, `libmpv`, and `ncurses`. Starts in milliseconds and runs smoothly using under 35 MB of RAM.
- **Audio Visualizers & Live Lyrics**: Built-in visualizers that respond directly to the audio, plus automatically synced lyrics from `lrclib.net`.
- **Clean System Footprint**: Installs missing standalone tools into an isolated user folder (`~/.vibe-fi/bottle/`) and uninstalls cleanly without leaving junk behind.

---

## Features

### Direct YouTube Streaming
Search for any song with `vibe "song title"` or press <kbd>S</kbd> inside the player. You can also paste direct YouTube links with <kbd>U</kbd>. Vibe-Fi streams audio directly without decoding video, saving CPU and bandwidth.

### Local Audio Library
Press <kbd>L</kbd> to browse and play audio files from your computer. Supports `.flac`, `.mp3`, `.wav`, `.m4a`, `.ogg`, `.opus`, `.aac`, `.alac`, `.aiff`, and `.webm`. Track durations are cached for fast navigation, and you can add any song to a playlist with <kbd>A</kbd>.

### Real-Time Audio Visualizers
Visualizers driven by real-time audio statistics (volume, peak levels, and pitch dynamics):
- **Cava Wave** *(Default)*: Smooth, fluid wave spectrum with continuous frequency balancing.
- **Neon Flame**: Mirrored equalizer bars with floating peak caps (`▔`) and beat metronome.
- **Stereo Bars**: Classic left and right channel equalizer spectrum.

Press <kbd>V</kbd> to cycle between visualizer modes at any time.

### Synchronized Lyrics
Automatically searches `lrclib.net` for lyrics matching the active song and scrolls them line-by-line with the music. Lyrics are cached locally in `~/.vibe-fi/cache/lyrics/` so they work offline. You can also scroll manually with <kbd>↑</kbd> and <kbd>↓</kbd>.

### Desktop Media Keys (Linux MPRIS)
Control playback using your keyboard media keys, desktop bar widgets (Waybar, Polybar), or `playerctl` through native Linux D-Bus integration.

### Discord Rich Presence
Displays your current song and artist on your Discord profile using a direct, lightweight Unix socket connection.

### Playlists & Session Memory
Create, edit, and organize playlists (<kbd>P</kbd>), or export them to `.m3u` files (<kbd>E</kbd>). If you close the terminal, press <kbd>R</kbd> on launch to restore your last track, seek position, and volume. Your active theme and visualizer mode are remembered automatically.

### Clean Color Themes
Press <kbd>T</kbd> to cycle through four curated terminal themes:
- **Midnight**: Deep indigo borders, electric cyan spectrum, magenta highlights.
- **Matrix**: Clean green hacker aesthetic.
- **Nord**: Cool blue and frost-white palette.
- **HyDE**: Velvet magenta, violet wave, and cyan highlights.

### Isolated Dependency Bottle & Clean Uninstaller
Vibe-Fi is built to keep your operating system tidy:
- **Bottle Isolation (`~/.vibe-fi/bottle/`)**: Missing standalone tools (like `yt-dlp`) are downloaded into an isolated user folder without needing root or `sudo`.
- **Dependency Guard**: When you run `vibe --uninstall` or `./uninstall.sh`, Vibe-Fi cleans its bottle directory and checks whether other apps need installed libraries before removing them.
- **Status Check**: Run `vibe --bottle` anytime to inspect active bottle dependencies and system tracking.

---

## Comparison

| Capability | Vibe-Fi | cmus | spotify-tui | ncmpcpp + mpd |
| :--- | :---: | :---: | :---: | :---: |
| **YouTube Streaming** | **Built-in (`yt-dlp`)** | ❌ No | ❌ No | ⚠️ Complex scripts |
| **Lossless Local Audio** | **Native C++17** | ✅ Yes | ❌ No | ✅ Yes |
| **Synchronized LRC Lyrics** | **Real-time (`lrclib`)** | ❌ No | ❌ No | ⚠️ External daemon |
| **Built-in DSP Visualizer** | **3 Modes (Physics & Wave)** | ❌ No | ❌ No | ⚠️ Requires Cava |
| **Zero-Config Setup** | **Single binary** | ✅ Yes | ❌ Spotify API / Premium | ❌ MPD config & server |
| **Linux MPRIS (`playerctl`)** | **Native D-Bus** | ⚠️ Plugin | ⚠️ Partial | ⚠️ Plugin |
| **Discord Rich Presence** | **Native Unix IPC** | ❌ No | ⚠️ Third-party | ⚠️ Third-party |
| **Memory Footprint** | **~25–35 MB** | ~15 MB | ~40 MB (+ Spotify client) | ~30 MB (+ MPD daemon) |

---

## Installation

### Automated Script (Recommended)

The automated script detects your operating system, installs missing dependencies, compiles the project, and installs `vibe`:

```bash
git clone https://github.com/Swadesh-c0de/vibe-fi.git
cd vibe-fi
chmod +x install.sh
./install.sh
```

> Supported systems: **Arch Linux**, **Ubuntu / Debian**, **Fedora / RHEL**, **openSUSE**, and **macOS** (Homebrew).

---

### GitHub Releases (v1.1.2)

You can also download and run a release archive directly from the [v1.1.2 Release Page](https://github.com/Swadesh-c0de/vibe-fi/releases/tag/v1.1.2):

```bash
# Download and extract the v1.1.2 release archive
curl -LO https://github.com/Swadesh-c0de/vibe-fi/archive/refs/tags/v1.1.2.tar.gz
tar -xzf v1.1.2.tar.gz
cd vibe-fi-1.1.2
chmod +x install.sh
./install.sh
```

---

### Building from Source

If you prefer to install dependencies and compile manually using CMake:

#### 1. System Dependencies

<details>
<summary><b>Arch Linux / Manjaro</b></summary>

```bash
sudo pacman -S --needed base-devel cmake mpv ncurses yt-dlp ffmpeg dbus pkgconf
```
</details>

<details>
<summary><b>Ubuntu / Debian / Pop!_OS / Linux Mint</b></summary>

```bash
sudo apt update
sudo apt install -y build-essential cmake libmpv-dev libncurses-dev libdbus-1-dev mpv ffmpeg yt-dlp curl pkg-config
```
</details>

<details>
<summary><b>Fedora / RHEL</b></summary>

```bash
sudo dnf install -y gcc-c++ cmake mpv-devel ncurses-devel dbus-devel mpv ffmpeg yt-dlp curl pkgconf-pkg-config
```
</details>

<details>
<summary><b>openSUSE</b></summary>

```bash
sudo zypper install -y gcc-c++ cmake mpv-devel ncurses-devel dbus-1-devel mpv ffmpeg yt-dlp curl pkg-config
```
</details>

<details>
<summary><b>macOS (Homebrew)</b></summary>

```bash
brew install cmake mpv ncurses yt-dlp ffmpeg pkg-config
```
</details>

#### 2. Compile & Install

```bash
git clone https://github.com/Swadesh-c0de/vibe-fi.git
cd vibe-fi

# Configure release build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build using all CPU threads
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

# Install for current user (no root required)
mkdir -p ~/.local/bin
install -m 755 ./build/vibe_fi ~/.local/bin/vibe
```

Make sure `~/.local/bin` is in your shell `$PATH`:
```bash
export PATH="$HOME/.local/bin:$PATH"
```

---

## Usage & Workflow

### CLI Commands & Search

Launch the player or start playing tracks directly from your command line:

```bash
# Launch the interactive player
vibe

# Search and play a track from YouTube
vibe "miles davis so what"
vibe "lofi hip hop radio"

# Stream audio directly from a YouTube URL
vibe "https://www.youtube.com/watch?v=5qap5aO4i9A"

# Play a local audio file
vibe ~/Music/song.flac

# Useful Flags
vibe --bottle      # Check isolated bottle dependencies and status
vibe --update      # Check for and apply latest updates
vibe --uninstall   # Uninstall Vibe-Fi and clean dependencies
vibe --no-update   # Launch without checking for updates
vibe --version     # Display version information
vibe --help        # Display usage guide
```

### Window Manager Setup

If you use a window manager (such as Hyprland, i3, Sway, or bspwm), you can bind a key to open Vibe-Fi in a floating scratchpad window:

```ini
# Example for Hyprland (~/.config/hypr/hyprland.conf)
bind = $mainMod, M, exec, alacritty --class "music-player" -e vibe
windowrulev2 = float, class:^(music-player)$
windowrulev2 = size 1000 620, class:^(music-player)$
```

### Uninstallation

You can cleanly uninstall Vibe-Fi at any time using either command:

**From anywhere in your terminal:**
```bash
vibe --uninstall
```

**Or from the repository folder:**
```bash
./uninstall.sh
```

Both methods provide a safe, complete uninstallation:
1. **Binary Removal**: Removes installed `vibe` binaries from system folders.
2. **Bottle Cleanup**: Deletes the isolated bottle directory (`~/.vibe-fi/bottle/`).
3. **Dependency Guard**: Checks whether other installed applications rely on tracked packages before prompting (`[y/N]`) to remove unused ones.
4. **Data Preservation**: Prompts whether you want to keep or delete your playlists and settings (`~/.vibe-fi`).

---

## Supported Systems

- [x] **Linux** (Arch, Ubuntu, Debian, Fedora, openSUSE, NixOS, Void, etc.)
  - Native D-Bus MPRIS media keys (`playerctl`, Waybar, Polybar)
  - Native Unix domain socket Discord Rich Presence
- [x] **macOS** (Homebrew audio pipeline, headless mpv)

---

## Hotkeys

> [!TIP]
> You can navigate menus using standard arrow keys or Vim keys (<kbd>j</kbd> / <kbd>k</kbd> / <kbd>h</kbd>).

### Global Navigation

| Key | Action |
| :---: | :--- |
| <kbd>ESC</kbd> | Back / Close modal / Exit |
| <kbd>↑</kbd> / <kbd>k</kbd> | Move selection up |
| <kbd>↓</kbd> / <kbd>j</kbd> | Move selection down |
| <kbd>ENTER</kbd> | Play track / Open folder / Confirm |

### Playback Screen

| Key | Action |
| :---: | :--- |
| <kbd>SPACE</kbd> | Play / Pause toggle |
| <kbd>N</kbd> / <kbd>></kbd> | Next track |
| <kbd>B</kbd> / <kbd><</kbd> | Previous track |
| <kbd>←</kbd> / <kbd>→</kbd> | Seek backward / forward 5 seconds |
| <kbd>+</kbd> / <kbd>-</kbd> | Volume up / down |
| <kbd>S</kbd> | Search YouTube |
| <kbd>L</kbd> | Open local music library |
| <kbd>P</kbd> | Open playlists |
| <kbd>C</kbd> | View current play queue |
| <kbd>U</kbd> | Paste YouTube URL to play |
| <kbd>V</kbd> | Switch visualizer (`Cava Wave` &rarr; `Neon Flame` &rarr; `Stereo Bars`) |
| <kbd>T</kbd> | Switch theme (`Midnight` &rarr; `Matrix` &rarr; `Nord` &rarr; `HyDE`) |
| <kbd>O</kbd> | Toggle autoplay (`ON` / `OFF`) |
| <kbd>R</kbd> | Replay track (or restore session from home screen) |
| <kbd>Q</kbd> | Jump to active playlist or search queue |
| <kbd>↑</kbd> / <kbd>↓</kbd> | Scroll synced lyrics manually |
| <kbd>ESC</kbd> | Exit player (prompts confirmation) |

### Library & Playlists

| View | Key | Action |
| :--- | :---: | :--- |
| **Library Browser** | <kbd>ENTER</kbd> | Play track / Open folder |
| | <kbd>BKSP</kbd> / <kbd>h</kbd> | Go up one directory |
| | <kbd>A</kbd> | Add selected track to playlist |
| **Playlists Overview** | <kbd>ENTER</kbd> | Open selected playlist |
| | <kbd>N</kbd> | Create new playlist |
| | <kbd>R</kbd> | Rename playlist |
| | <kbd>D</kbd> | Delete playlist |
| | <kbd>E</kbd> | Export playlist to `.m3u` file |
| **Playlist Editor** | <kbd>ENTER</kbd> | Play track immediately |
| | <kbd>D</kbd> | Remove track from playlist |
| | <kbd>M</kbd> | Move track to another playlist |
| **Play Queue** | <kbd>ENTER</kbd> | Play selected track from queue |
| | <kbd>D</kbd> | Remove track from queue |

---

## Themes & Visualizers

### Color Themes (<kbd>T</kbd>)

| Theme | Border & Accents | Visualizer Spectrum | Progress Bar |
| :--- | :---: | :---: | :---: |
| **Midnight** *(Default)* | Deep Indigo | Electric Cyan | Magenta |
| **Matrix** | Terminal Green | Phosphor Green | Bright Green |
| **Nord** | Arctic Cyan | Clean Frost White | Cool Blue |
| **HyDE** | Velvet Magenta | Violet & Lavender Wave | Vivid Cyan |

### Visualizers (<kbd>V</kbd>)

1. **Cava Wave** *(Default)*: Smooth, continuous wave spectrum with fluid frequency balancing.
2. **Neon Flame**: Mirrored equalizer columns with floating peak markers and metronome indicator.
3. **Stereo Bars**: Classic graphic equalizer showing left and right audio channels.

---

## Configuration & Dotfiles

All user data and settings are stored as human-readable plain text files in `~/.vibe-fi/`:

```
~/.vibe-fi/
├── state.ini                   # Saved session: last track, seek position, volume, theme
├── playlists/                  # Plain text playlists (Title|URL|Duration)
│   ├── Chill.txt
│   ├── Synthwave.txt
│   └── Favorites.txt
├── bottle/                     # Isolated dependency environment
│   ├── manifest.json           # Tracked packages and dependencies
│   ├── env.sh                  # Shell PATH export script
│   └── bin/                    # Bottled standalone binaries (e.g. yt-dlp)
└── cache/
    └── lyrics/                 # Cached lyrics from lrclib.net
        └── Daft+Punk_Get+Lucky.json
```

Because playlists and settings are saved as simple text files, you can easily track them in your dotfiles, share them with friends, or edit them with any text editor.

---

## Architecture

Vibe-Fi is built with decoupled, modular domains:

```
                            +--------------------+
                            |      main.cpp      |
                            |  (CLI Bootstrap)   |
                            +---------+----------+
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
|    Views    |       |   Visualizer    |<--------+ Audio Analysis  |
|  (Playback, |       | (Physics Engine)|         | (@astats filter)|
| Library, etc|       +-----------------+         +-----------------+
+------+------+
       |
+------v-----------------------------------------------------+
|                       src/services/                        |
|  - Library: Non-blocking crawler & in-memory cache         |
|  - Lyrics: REST lrclib.net client & disk cache             |
|  - PlaylistManager: Plaintext CRUD (.txt) & M3U exporter   |
|  - Search: Parameterized yt-dlp query pipeline             |
+------------------------------------------------------------+
       |
+------v-----------------------------------------------------+
|                     src/integrations/                      |
|  - MPRIS: Thread-safe D-Bus loop for Linux media keys      |
|  - Discord RPC: Native Unix domain socket IPC daemon       |
+------------------------------------------------------------+
```

---

## Notes & Invariants

If you are modifying the code or contributing:

- **Locale Setting**: We explicitly keep `LC_NUMERIC="C"` so `libmpv` parses decimal timestamps reliably across all international locales.
- **Memory Management**: Audio analysis nodes are cleanly released on every frame (`mpv_free_node_contents`) to prevent memory leaks during long playback sessions.
- **Smooth Rendering**: Terminal drawing uses curses double-buffering (`werase` + `wnoutrefresh` + single `doupdate` per frame at 30 FPS) to prevent screen flickering.
- **Bottle Tool Discovery**: Standalone tools (such as `yt-dlp`) are resolved first from `~/.vibe-fi/bottle/bin/` so users can run without root permissions.

---

## Troubleshooting

<details>
<summary><b>1. YouTube streaming errors or 403 Forbidden</b></summary>
<br>

YouTube regularly updates its streaming formats. Updating `yt-dlp` to the latest version usually resolves streaming issues immediately:

```bash
# Arch Linux
sudo pacman -Syu yt-dlp

# Ubuntu / Debian
sudo yt-dlp -U 2>/dev/null || sudo curl -L https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -o /usr/local/bin/yt-dlp && sudo chmod a+rx /usr/local/bin/yt-dlp

# Check version
yt-dlp --version
```
</details>

<details>
<summary><b>2. Media keys not responding</b></summary>
<br>

Check if `playerctl` detects Vibe-Fi on your system:

```bash
playerctl -l
# Expected output: vibe_fi

# Test manual trigger:
playerctl --player=vibe_fi play-pause
playerctl --player=vibe_fi next
```
Make sure `libdbus-1-dev` was installed when compiling on Linux.
</details>

<details>
<summary><b>3. Discord Rich Presence not displaying</b></summary>
<br>

1. Make sure the Discord desktop app is running.
2. In Discord Settings &rarr; **Activity Privacy**, enable **"Display current activity as a status message"**.
3. Ensure `$XDG_RUNTIME_DIR` is set in your shell (`echo $XDG_RUNTIME_DIR`). Vibe-Fi communicates with `discord-ipc-0` inside this directory.
</details>

<details>
<summary><b>4. Misaligned borders or visualizer blocks</b></summary>
<br>

Ensure your terminal uses UTF-8 and a monospace font that supports box-drawing characters (such as *JetBrains Mono*, *Fira Code*, or *Hack*):

```bash
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export TERM=xterm-256color
```
</details>

---

## Contributing

Contributions, bug reports, and suggestions are welcome!

Please check our **[Contributing Guide](./CONTRIBUTING.md)** for architecture guidelines, visualizer playbooks, and pull request steps.

---

## Thanks

### Support

- If you enjoy using Vibe-Fi, please consider starring the repository on GitHub! ⭐
- Share it with your friends and fellow terminal users.

### Maintainer

- **[@Swadesh-c0de](https://github.com/Swadesh-c0de)** - Creator and maintainer

---

<div align="center">

## d[-_-]b Turn up the volume & keep vibing.

</div>

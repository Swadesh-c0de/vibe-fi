# Vibe-Fi 🎵

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)

**Vibe-Fi** is an aesthetic, high-performance terminal-based music player designed for developers. It seamlessly integrates YouTube search, local library management, custom playlist management, synced lyrics display, retro visualizers, and desktop notifications into a cohesive and premium TUI experience.

---

## ✨ Features

- **YouTube Streaming & Search**: Search and stream high-quality audio directly from YouTube.
- **Local Library Browser**: Browse, queue, and play your local audio files.
- **Advanced Playlist Management**:
  - Create, rename, delete, and view custom playlists.
  - Smart **Duplicate Prevention** for songs and playlist names.
  - Easily **Add and Move songs** within or between playlists.
- **Interactive Queue Manager**: View, navigate, and modify the upcoming track queue.
- **Live Reacting Visualizer**: A beautiful terminal visualizer reacting dynamically to audio output with multiple modes:
  - `Stereo Bars`: Standard left/right frequency bars.
  - `Waveform`: Real-time wave oscillation line.
  - `Pulse`: Center-outward pulsing visualizer.
- **Synced Lyrics Fetching**: Automatically fetches and highlights synced lyrics using the `lrclib.net` API (cached locally in `~/.vibe-fi/cache/lyrics/` for offline use).
- **Personalized Themes**: Switch dynamically between curated UI palettes:
  - `Midnight`: Sleek dark-blue and purple layout.
  - `Matrix`: Classic terminal green glow.
  - `Nord`: Elegant arctic-blue and white.
- **Desktop & OS Integrations**:
  - **MPRIS Integration**: Connects via DBus (`dbus-1`) to enable media keys and tools like `playerctl` to control play, pause, next, and previous actions.
  - **Discord Rich Presence (RPC)**: Automatically displays your current song and artist status on Discord via a custom Unix socket implementation.
- **Autoplay**: Automatically transitions to the next song in the active queue or playlist.

---

## 🛠️ Tech Stack

Vibe-Fi is engineered using lightweight and highly performance-oriented technologies:

- **Core Language**: C++17 (for robustness, execution speed, and native OS APIs).
- **Audio Engine**: **[libmpv](https://mpv.io/)** (client-side C API) to handle audio parsing, streaming, and decoding.
- **Terminal UI**: **[ncurses](https://invisible-island.net/ncurses/)** for windows, menus, controls, and high-refresh visualizers.
- **Desktop Integrations**:
  - **D-Bus (`dbus-1`)** for standard Linux desktop MPRIS controls.
  - Custom Unix Socket client for Discord RPC presence updating.
- **Web APIs**: **[lrclib.net](https://lrclib.net/)** API fetched via `curl` subprocess calls.
- **Helper Utilities**: `yt-dlp` for YouTube extraction, `ffmpeg` for media stream handling, and `stb_image.h` header utilities.
- **Build System**: CMake (minimum version 3.10).

---

## 📁 Project Structure

Below is the directory layout and description of the source modules:

```
vibe-fi/
├── CMakeLists.txt              # CMake build configuration and dependency links
├── LICENSE                     # MIT license file
├── README.md                   # Project documentation
├── install.sh                  # Automation script to install dependencies and compile
└── src/                        # C++ Source files
    ├── main.cpp                # App entrypoint; parses args and initializes UI & Player
    ├── player.cpp / .hpp       # libmpv wrapper for media controls and audio properties
    ├── ui.cpp / .hpp           # Ncurses drawing loops, input handlers, layouts, and themes
    ├── mpris.cpp / .hpp        # D-Bus MPRIS server thread handling playerctl callbacks
    ├── discord_rpc.cpp / .hpp  # IPC client writing song presence info to Discord local socket
    ├── lyrics.cpp / .hpp       # Synced lyrics fetcher (caching curl calls to ~/.vibe-fi/)
    ├── library.cpp / .hpp      # Local audio directory crawler and file list parser
    ├── playlist_manager.cpp    # JSON-based playlist storage, validation, and modifiers
    ├── search.cpp / .hpp       # YouTube search engine querying streams via yt-dlp
    ├── utils.cpp / .hpp        # Common helpers (e.g. is_url, stream extract, string formatting)
    └── stb_image.h             # Single-header image loader library (reserved/utility)
```

---

## 🚀 Installation

### Automatic Installation (Recommended)

The installation script automatically detects your distribution/OS, installs standard system dependencies, and builds the codebase:

```bash
git clone https://github.com/Swadesh-c0de/vibe-fi.git
cd vibe-fi
chmod +x install.sh
./install.sh
```

**Supported Environments:**
- ✅ Arch Linux (and derivatives)
- ✅ Ubuntu/Debian (and derivatives)
- ✅ macOS (via Homebrew)

### Manual Installation

If you prefer to configure components yourself:

1. **Install Dependencies:**
   - **Compilers**: `cmake`, `make`, `g++` (C++17 support)
   - **Libraries**: `libmpv-dev`, `libncurses-dev`, `libdbus-1-dev`
   - **CLI Tools**: `mpv`, `yt-dlp`, `ffmpeg`, `curl`

2. **Build and Install:**
   ```bash
   mkdir build && cd build
   cmake ..
   make
   sudo cp vibe_fi /usr/local/bin/vibe
   ```

---

## 🎧 Usage & Keybindings

Run the application:
```bash
vibe
```

Or play a URL / file directly from CLI:
```bash
vibe "https://www.youtube.com/watch?v=dQw4w9WgXcQ"
```

### ⌨️ Global Controls & Navigation

- **ESC**: Go Back / Cancel / Quit (from main screens)
- **Arrow Keys (Up/Down)**: Navigate list selections
- **ENTER**: Select, open, or play selected item

### Mode-Specific Keybindings

| Screen / Mode | Key | Action |
| :--- | :---: | :--- |
| **Intro Screen** | `L` | Open Local Library |
| | `S` | Start YouTube Search |
| | `P` | Browse Playlists |
| | `Q` / `ESC` | Quit Vibe-Fi |
| **Playback Control** | `SPACE` | Play / Pause toggle |
| | `←` / `→` | Seek backward / forward 5 seconds |
| | `+` / `-` | Volume Up / Down (by 5%) |
| | `Q` | Return to active queue (Playlist / Search Results) |
| | `O` | Toggle Autoplay (ON / OFF) |
| | `U` | Load and play a YouTube URL directly |
| | `C` | View interactive Play Queue |
| | `T` | Cycle UI Theme (`Midnight` ➔ `Matrix` ➔ `Nord`) |
| | `V` | Cycle Visualizer Mode (`Stereo Bars` ➔ `Waveform` ➔ `Pulse`) |
| | `R` | Replay the current track from beginning |
| | `L` | Open Library |
| | `S` | New YouTube Search |
| | `P` | Open Playlists |
| | `Arrow Up/Down`| Manual Lyrics Scroll |
| **Search / Library** | `A` | Add selected track to a Playlist |
| | `S` | Open a new Search query dialog |
| **Playlists Panel** | `N` | Create a new Playlist |
| | `R` | Rename selected Playlist |
| | `D` | Delete selected Playlist |
| **Playlist View** | `D` | Remove song from Playlist |
| | `M` | Move song (reorder in playlist or transfer to another) |

---

## 🛠️ Troubleshooting

- **"Failed to extract stream URL"**: Usually occurs when a YouTube video is region-restricted or age-gated. Try another search result.
- **Audio Output Issues**: Ensure your system audio server (PipeWire, PulseAudio, or ALSA) is working and verify that `mpv --version` works in the console.
- **yt-dlp Errors**: Update the YouTube parser to the latest stream decryption rules:
  ```bash
  sudo yt-dlp -U
  ```

---

## 📄 License & Credits

- Distributed under the **MIT License**. See [LICENSE](LICENSE) for details.
- Special thanks to:
  - **[libmpv](https://mpv.io/)** for the low-level media playback.
  - **[ncurses](https://invisible-island.net/ncurses/)** for layout controls.
  - **[lrclib.net](https://lrclib.net/)** for providing a free, open-source synced lyrics catalog.
  - **[yt-dlp](https://github.com/yt-dlp/yt-dlp)** for stream metadata resolution.

# Vibe-Fi 🎵

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)

**Vibe-Fi** is a asthetic terminal-based music player designed for the modern developer. It seamlessly integrates YouTube search, local library management, and a retro-futuristic visualizer into a high-performance TUI experience.

---

## ✨ Features

- **YouTube Integration**: Search and stream high-quality audio directly from YouTube.
- **Local Library**: Browse and play your local music collection with ease.
- **Playlist Management**: Create, manage, and play custom playlists.
    - **Duplicate Prevention**: Smartly prevents duplicate songs and playlist names.
    - **Contextual Navigation**: Jump back to your current playlist or search results instantly.
- **Autoplay**: Automatically plays the next song from your playlist or search results.
- **Live Visualizer**: A responsive, retro-style audio visualizer that reacts to your music.
- **Synced Lyrics**: Automatically fetches and displays synced lyrics for the current track.
- **Unified UI**: Professional split-screen layout with symmetric visualizer and lyrics.
- **Modern TUI**: A polished, keyboard-driven interface with centered dialogs and intuitive navigation.

---

## 🚀 Installation

### Automatic Installation (Recommended)

The install script automatically detects your OS and installs all dependencies:

```bash
git clone https://github.com/Swadesh-c0de/vibe-fi.git
cd vibe-fi
chmod +x install.sh
./install.sh
```

**Supported Systems:**
- ✅ Arch Linux (and derivatives)
- ✅ Ubuntu/Debian (and derivatives)
- ✅ macOS (Apple Silicon & Intel)

### Manual Installation

**Dependencies:**
- `cmake`, `make`, `g++`
- `libmpv-dev`, `libncurses-dev`
- `mpv`, `yt-dlp`, `ffmpeg`

**Build Instructions:**

```bash
mkdir build && cd build
cmake ..
make
sudo cp vibe_fi /usr/local/bin/vibe
```

---

## 🎧 Usage

Start the application from your terminal:

```bash
vibe
```

### ⌨️ Keybindings

#### **Global**
- **ESC**: Quit application / Back / Cancel
- **Arrow Keys**: Navigate lists

#### **Intro Screen**
- **L**: Go to Library
- **S**: Search YouTube
- **P**: Browse Playlists
- **Q**: Quit

#### **Playback Mode**
- **SPACE**: Play/Pause
- **Q**: Return to current queue (Playlist/Search Results)
- **O**: Toggle Autoplay (ON/OFF)
- **L**: Go to Library
- **S**: New Search
- **P**: Go to Playlists
- **R**: Replay last track
- **←/→**: Seek backward/forward (5s)
- **+/-**: Volume up/down

#### **Search Results & Playlists**
- **ENTER**: Play selected track
- **A**: Add to Playlist
- **S**: Start new search
- **ESC**: Back

#### **Playlist Management**
- **N**: Create New Playlist (from "Add to Playlist" or Browser)
- **R**: Rename Playlist
- **D**: Delete Playlist
- **ENTER**: Select / View

#### **Playlist View**
- **ENTER**: Play
- **D**: Remove Song
- **M**: Move Song
- **ESC**: Back

---

## 🛠️ Troubleshooting

- **"Failed to extract stream URL"**: Some YouTube videos may be restricted. Try another result.
- **No audio**: Ensure `mpv` is installed and working correctly (`mpv --version`).
- **yt-dlp errors**: Update to the latest version: `sudo yt-dlp -U`.

---

## 📄 License

MIT License — See [LICENSE](LICENSE) for details.

---

## 🙌 Credits

Built with ❤️ using:
- **[libmpv](https://mpv.io/)** - Powerful media playback core.
- **[ncurses](https://invisible-island.net/ncurses/)** - Robust terminal UI framework.
- **[yt-dlp](https://github.com/yt-dlp/yt-dlp)** - Reliable YouTube media extraction.
- **[lrclib.net](https://lrclib.net/)** - Free and open lyrics API.

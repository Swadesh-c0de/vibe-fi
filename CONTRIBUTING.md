# Contributing to Vibe-Fi

Welcome to **Vibe-Fi**! This guide will help you get started contributing to the project, whether you're fixing bugs, building new audio visualizers, adding themes, or sharing ideas.

There are many ways to contribute:

* Reporting bugs
* Fixing issues & improving documentation
* Adding dynamic audio visualizers
* Adding curated color themes
* Improving streaming & local audio crawlers
* Suggesting new features & sharing feedback

---

## 🐞 Issues

### Found a bug?

1. Check if there's already an open or closed issue for it.
2. If not, open a new issue and describe the problem clearly.
3. Please include:
   - Your operating system and desktop environment (e.g. Arch Linux, KDE Wayland)
   - Your terminal emulator (e.g. Alacritty, Kitty, WezTerm)
   - Output of `yt-dlp --version` and `vibe --version`
   - Reproduction steps

### Want to fix an issue?

1. Fork this repository.
2. Create a new branch for the issue: `git checkout -b fix/issue-description`.
3. Commit your changes with clear, descriptive commit messages.
4. Open a Pull Request (PR) with context on the bug and your proposed fix.

---

## 🎨 Adding a New Visualizer Mode

Adding a visualizer to Vibe-Fi is clean and modular. Follow these steps:

1. **Update Enum**: Add your identifier to `VisualizerMode` in `src/ui/visualizer.hpp`:
   ```cpp
   enum class VisualizerMode {
       CAVA_WAVE,
       NEON_FLAME,
       STEREO_BARS,
       YOUR_NEW_MODE // Add here
   };
   ```
2. **Add Animation State Vectors**: In `src/ui/visualizer.hpp`, declare history or physics vectors inside the `Visualizer` class:
   ```cpp
   std::vector<float> custom_bars;
   std::vector<float> custom_peaks;
   ```
3. **Reset State**: In `src/ui/visualizer.cpp`, clear your vectors inside `Visualizer::reset()`.
4. **Implement Renderer**: Add `void Visualizer::render_your_mode(...)` in `src/ui/visualizer.cpp`. Use the passed `AudioLevelStats` for real-time RMS loudness, peak spikes, and zero-crossing pitch data.
5. **Dispatch**: Add a case branch inside `Visualizer::render()`.
6. **Cycle Logic**: In `src/ui/ui.cpp` (`UI::cycle_visualizer()`), update `vmode = (vmode + 1) % N;` and add your display name.
7. **Verify**: Build and test in your terminal using the <kbd>V</kbd> key.

---

## 🖌️ Adding a Color Theme

Themes customize borders, progress bars, visualizer gradients, and text colors:

1. Open `src/ui/ui.cpp` and locate `UI::load_themes()`.
2. Add your new theme palette to the `themes` vector:
   ```cpp
   themes.push_back({"YourTheme", COLOR_BORDER, COLOR_PROGRESS, COLOR_VIZ, COLOR_ALERT, -1, COLOR_FG, COLOR_BG});
   ```
3. Test cycling through themes with the <kbd>T</kbd> key.

---

## ⚠️ Core Architectural Invariants (Do Not Break)

To ensure stability across all Linux distributions and long listening sessions, please uphold these rules:

1. **Decimal Locale (`LC_NUMERIC="C"`)**:
   - `libmpv` internally uses `strtod` for decimal timestamp parsing.
   - Never remove or alter `std::setlocale(LC_NUMERIC, "C");`.
   - On European locales with decimal commas (e.g. `1,5` instead of `1.5`), altering this causes `libmpv` to fail or crash on seeking.

2. **Memory Safety on Node Extraction**:
   - Whenever calling `mpv_get_property(mpv, "...", MPV_FORMAT_NODE, &node)` to extract `@astats` audio data, always pair it with `mpv_free_node_contents(&node)`.
   - Failing to free nodes leaks memory inside the 30 FPS audio extraction loop.

3. **External Command Security**:
   - Never concatenate unsanitized user search queries into `system()` or raw `popen()`.
   - Always use sanitized pipelines and argument vectors as implemented in `src/services/search.cpp`.

4. **Curses Double-Buffering & Flicker Elimination**:
   - Terminal rendering runs at ~30 FPS.
   - Never call `wrefresh(win)` or `refresh()` inside individual draw routines.
   - Use `werase(win)` &rarr; draw elements &rarr; `wnoutrefresh(win)`.
   - Let the main loop invoke `doupdate()` **once** per frame.

5. **Autoplay Queue Progression & Error Recovery**:
   - Autoplay must strictly advance to the next track upon confirmed natural EOF (`consume_track_finished()`).
   - Never advance the queue based solely on `is_idle()`. Stream or decoding errors must trigger automatic retry or pause safely to avoid runaway queue skipping.

---

## 🛠️ Local Development & Build

### 1. Build Debug Binary
```bash
# Configure debug build
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Compile on all CPU threads
cmake --build build -j$(nproc 2>/dev/null || echo 2)

# Run local executable
./build/vibe_fi
```

### 2. Fast Test Run
```bash
# Test with a YouTube search query
./build/vibe_fi "lofi beats"

# Test with a local audio file
./build/vibe_fi ~/Music/test.flac
```

---

## 💡 Sharing Ideas & Discussions

Got a cool feature idea or question?
Open an issue or start a discussion on our GitHub repo:
[https://github.com/Swadesh-c0de/vibe-fi/issues](https://github.com/Swadesh-c0de/vibe-fi/issues)

---

## ✅ Pull Request Checklist

Before submitting your PR, please verify:

- [ ] Code compiles cleanly without warnings (`-Wall -Wextra -Wpedantic`)
- [ ] Code follows C++17 standards
- [ ] Core audio invariants are preserved (`LC_NUMERIC="C"`, node cleanup)
- [ ] Tested in terminal (UTF-8, 256 colors) with smooth rendering and no memory leaks
- [ ] No temporary files or debug `std::cout` statements committed
- [ ] PR title follows [Conventional Commits](https://www.conventionalcommits.org/) (`feat: ...`, `fix: ...`, `docs: ...`, `refactor: ...`)

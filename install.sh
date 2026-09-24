#!/usr/bin/env bash

set -e  # Exit on error

echo "======================================"
echo "          Vibe-Fi Installer           "
echo "  Terminal Music Player for Developers"
echo "======================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Detect OS and package manager
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ -f /etc/os-release ]]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        if [[ "$ID" == "arch" ]] || [[ "$ID_LIKE" == *"arch"* ]]; then
            echo "arch"
        elif [[ "$ID" == "ubuntu" ]] || [[ "$ID" == "debian" ]] || [[ "$ID_LIKE" == *"debian"* ]] || [[ "$ID_LIKE" == *"ubuntu"* ]]; then
            echo "ubuntu"
        elif [[ "$ID" == "fedora" ]] || [[ "$ID_LIKE" == *"fedora"* ]] || [[ "$ID_LIKE" == *"rhel"* ]]; then
            echo "fedora"
        elif [[ "$ID" == "opensuse"* ]] || [[ "$ID_LIKE" == *"suse"* ]]; then
            echo "opensuse"
        else
            echo "unknown"
        fi
    else
        echo "unknown"
    fi
}

detect_pkg_mgr() {
    local os="$1"
    case "$os" in
        arch) echo "pacman" ;;
        ubuntu) echo "apt" ;;
        fedora) echo "dnf" ;;
        opensuse) echo "zypper" ;;
        macos) echo "brew" ;;
        *) echo "unknown" ;;
    esac
}

is_package_installed() {
    local pkg="$1"
    local mgr="$2"
    case "$mgr" in
        apt)
            dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed"
            ;;
        pacman)
            pacman -Qi "$pkg" &>/dev/null
            ;;
        dnf|zypper)
            rpm -q "$pkg" &>/dev/null
            ;;
        brew)
            brew list --formula "$pkg" &>/dev/null
            ;;
        *)
            return 1
            ;;
    esac
}

to_json_array() {
    local arr=("$@")
    if [[ ${#arr[@]} -eq 0 ]]; then
        echo "[]"
        return
    fi
    local items=()
    for item in "${arr[@]}"; do
        if [[ -n "$item" ]]; then
            items+=("\"$item\"")
        fi
    done
    if [[ ${#items[@]} -eq 0 ]]; then
        echo "[]"
        return
    fi
    local IFS=", "
    echo "[${items[*]}]"
}

OS=$(detect_os)
PKG_MGR=$(detect_pkg_mgr "$OS")
BOTTLE_DIR="$HOME/.vibe-fi/bottle"
BOTTLE_BIN_DIR="$BOTTLE_DIR/bin"

echo -e "${GREEN}Detected Platform:${NC}        ${CYAN}$OS${NC} (Package Manager: ${CYAN}$PKG_MGR${NC})"
echo -e "${GREEN}Bottle Environment:${NC}       ${CYAN}$BOTTLE_DIR${NC}"
echo ""

# Tracking arrays for bottle manifest
PREINSTALLED_DEPS=()
INSTALLED_SYS_PKGS=()
BOTTLED_BINS=()

# Setup system dependencies and bottle
setup_dependencies() {
    local sys_pkgs=()

    case "$OS" in
        arch)
            sys_pkgs=(base-devel cmake mpv ncurses ffmpeg dbus pkgconf curl)
            ;;
        ubuntu)
            sys_pkgs=(build-essential cmake libmpv-dev libncurses-dev libdbus-1-dev mpv ffmpeg python3 curl pkg-config)
            ;;
        fedora)
            sys_pkgs=(gcc-c++ cmake mpv-devel ncurses-devel dbus-devel mpv ffmpeg curl pkgconf-pkg-config)
            ;;
        opensuse)
            sys_pkgs=(gcc-c++ cmake mpv-devel ncurses-devel dbus-1-devel mpv ffmpeg curl pkg-config)
            ;;
        macos)
            if ! command -v brew &> /dev/null; then
                echo -e "${RED}Homebrew not found. Please install Homebrew first:${NC}"
                echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
                exit 1
            fi
            sys_pkgs=(cmake mpv ncurses ffmpeg pkg-config curl)
            ;;
        *)
            echo -e "${YELLOW}Warning: Unknown or custom Linux distribution.${NC}"
            echo "Please ensure C++17 compiler, cmake, libmpv, ncurses, and ffmpeg are installed."
            ;;
    esac

    local missing_pkgs=()
    if [[ ${#sys_pkgs[@]} -gt 0 ]]; then
        echo -e "${YELLOW}Inspecting host system packages...${NC}"
        for pkg in "${sys_pkgs[@]}"; do
            if is_package_installed "$pkg" "$PKG_MGR"; then
                PREINSTALLED_DEPS+=("$pkg")
            else
                missing_pkgs+=("$pkg")
            fi
        done
    fi

    if [[ ${#missing_pkgs[@]} -gt 0 ]]; then
        echo -e "${YELLOW}Installing missing system libraries via $PKG_MGR:${NC} ${missing_pkgs[*]}"
        case "$PKG_MGR" in
            apt)
                sudo apt update
                sudo apt install -y "${missing_pkgs[@]}"
                ;;
            pacman)
                sudo pacman -Sy --needed --noconfirm "${missing_pkgs[@]}"
                ;;
            dnf)
                sudo dnf install -y "${missing_pkgs[@]}"
                ;;
            zypper)
                sudo zypper install -y "${missing_pkgs[@]}"
                ;;
            brew)
                brew install "${missing_pkgs[@]}"
                ;;
        esac
        INSTALLED_SYS_PKGS=("${missing_pkgs[@]}")
    else
        echo -e "${GREEN}All required system libraries are already preinstalled on host.${NC}"
    fi

    # Bottle Standalone Tool Isolation (yt-dlp)
    echo ""
    echo -e "${YELLOW}Inspecting standalone stream resolver (yt-dlp)...${NC}"
    if command -v yt-dlp &> /dev/null; then
        local ytdl_ver
        ytdl_ver=$(yt-dlp --version 2>/dev/null || echo "OK")
        echo -e "${GREEN}Host yt-dlp detected:${NC} $ytdl_ver [using preinstalled host binary]"
        PREINSTALLED_DEPS+=("yt-dlp")
    elif [[ -x "$BOTTLE_BIN_DIR/yt-dlp" ]]; then
        echo -e "${GREEN}Existing bottled yt-dlp detected:${NC} $("$BOTTLE_BIN_DIR/yt-dlp" --version 2>/dev/null || echo "OK")"
        BOTTLED_BINS+=("yt-dlp")
    else
        echo -e "${YELLOW}yt-dlp not found on system. Installing isolated binary to Vibe Bottle...${NC}"
        mkdir -p "$BOTTLE_BIN_DIR"
        if curl -sL https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -o "$BOTTLE_BIN_DIR/yt-dlp"; then
            chmod a+rx "$BOTTLE_BIN_DIR/yt-dlp"
            BOTTLED_BINS+=("yt-dlp")
            echo -e "${GREEN}Isolated yt-dlp successfully installed to:${NC} ${CYAN}$BOTTLE_BIN_DIR/yt-dlp${NC} (zero root permission needed)"
        else
            echo -e "${RED}Failed to download isolated yt-dlp. YouTube search and streaming may be limited.${NC}"
        fi
    fi

    # Generate Bottle Manifest and env.sh
    mkdir -p "$BOTTLE_DIR"
    local created_ts
    created_ts=$(date -u +"%Y-%m-%dT%H:%M:%SZ" 2>/dev/null || date +"%Y-%m-%d")

    cat <<EOF > "$BOTTLE_DIR/manifest.json"
{
  "bottle_version": "1.1.0",
  "bottle_dir": "$BOTTLE_DIR",
  "created_at": "$created_ts",
  "platform": "$OS",
  "package_manager": "$PKG_MGR",
  "bottled_binaries": $(to_json_array "${BOTTLED_BINS[@]}"),
  "installed_system_packages": $(to_json_array "${INSTALLED_SYS_PKGS[@]}"),
  "preinstalled_dependencies": $(to_json_array "${PREINSTALLED_DEPS[@]}")
}
EOF

    cat <<EOF > "$BOTTLE_DIR/env.sh"
#!/usr/bin/env bash
# Vibe-Fi Bottle Environment Script
export PATH="$BOTTLE_BIN_DIR:\$PATH"
export VIBE_BOTTLE_DIR="$BOTTLE_DIR"
EOF
    chmod +x "$BOTTLE_DIR/env.sh"

    export PATH="$BOTTLE_BIN_DIR:$PATH"
    export VIBE_BOTTLE_DIR="$BOTTLE_DIR"
    echo -e "${GREEN}Bottle manifest generated:${NC} ${CYAN}$BOTTLE_DIR/manifest.json${NC}"
}

# Build the application
build_app() {
    echo ""
    echo -e "${YELLOW}Configuring and building Vibe-Fi...${NC}"
    
    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    
    local cores
    cores=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
    cmake --build build -j"$cores"
    
    echo -e "${GREEN}Build completed successfully!${NC}"
}

# Install the binary
install_binary() {
    echo ""
    echo "Select binary installation target:"
    echo "  [1] User directory: ~/.local/bin/vibe (Recommended, no sudo required)"
    echo "  [2] System-wide:    /usr/local/bin/vibe (Requires sudo)"
    echo "  [3] Skip installation (Run locally from ./build/vibe_fi)"
    read -p "Enter choice [1/2/3] (default: 1): " -r CHOICE
    CHOICE=${CHOICE:-1}

    case "$CHOICE" in
        1|u|U)
            mkdir -p "$HOME/.local/bin"
            install -m 755 ./build/vibe_fi "$HOME/.local/bin/vibe"
            echo -e "${GREEN}Installed successfully to $HOME/.local/bin/vibe!${NC}"
            if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
                echo -e "${YELLOW}Notice:${NC} Make sure $HOME/.local/bin is in your PATH."
            fi
            ;;
        2|s|S)
            echo -e "${YELLOW}Installing to /usr/local/bin via sudo...${NC}"
            sudo cmake --install build --prefix /usr/local
            echo -e "${GREEN}Installed successfully to /usr/local/bin/vibe!${NC}"
            ;;
        *)
            echo -e "${YELLOW}Skipping binary installation.${NC}"
            echo -e "You can run the application directly from: ${GREEN}./build/vibe_fi${NC}"
            ;;
    esac
}

# Main execution flow
main() {
    echo "Step 1: Inspecting and preparing Bottle dependencies..."
    setup_dependencies
    
    echo ""
    echo "Step 2: Compiling Vibe-Fi..."
    build_app
    
    echo ""
    echo "Step 3: Binary Installation..."
    install_binary
    
    echo ""
    echo -e "${GREEN}======================================"
    echo "       Installation Complete!"
    echo "======================================${NC}"
    echo ""
    echo "Usage:"
    if command -v vibe &> /dev/null; then
        echo "  vibe [query / url / audio_file]"
    elif [[ -x "$HOME/.local/bin/vibe" ]]; then
        echo "  ~/.local/bin/vibe [query / url / audio_file]"
    else
        echo "  ./build/vibe_fi [query / url / audio_file]"
    fi
    echo ""
    echo "Inspect Isolated Bottle:"
    echo "  vibe --bottle       (Inspect bottled dependencies & tracking)"
    echo ""
    echo "Keybindings:"
    echo "  - [SPACE]    Play / Pause"
    echo "  - [S]        YouTube Search"
    echo "  - [L]        Local Audio Library"
    echo "  - [P]        Custom Playlists"
    echo "  - [C]        View Play Queue"
    echo "  - [T]        Cycle Themes (Midnight / Matrix / Nord / HyDE)"
    echo "  - [V]        Cycle Visualizers (Cava Wave / Neon Flame / Stereo Bars)"
    echo "  - [ESC / Q]  Back / Quit"
    echo ""
    echo "Clean Uninstallation:"
    echo "  Run 'vibe --uninstall' or './uninstall.sh' anytime to remove Vibe-Fi"
    echo "  and all isolated bottle dependencies with zero leftover system junk."
    echo ""
}

main


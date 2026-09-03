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

# Detect OS
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

OS=$(detect_os)
echo -e "${GREEN}Detected Platform:${NC} ${CYAN}$OS${NC}"
echo ""

# Install dependencies based on OS
install_dependencies() {
    case $OS in
        arch)
            echo -e "${YELLOW}Installing dependencies for Arch Linux...${NC}"
            sudo pacman -Sy --needed --noconfirm base-devel cmake mpv ncurses yt-dlp ffmpeg dbus pkgconf
            ;;
        ubuntu)
            echo -e "${YELLOW}Installing dependencies for Ubuntu/Debian...${NC}"
            sudo apt update
            sudo apt install -y build-essential cmake libmpv-dev libncurses-dev libdbus-1-dev mpv ffmpeg python3 curl pkg-config
            
            # Install yt-dlp if not available or old
            if ! command -v yt-dlp &> /dev/null; then
                echo -e "${YELLOW}Installing latest yt-dlp release...${NC}"
                sudo curl -L https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -o /usr/local/bin/yt-dlp
                sudo chmod a+rx /usr/local/bin/yt-dlp
            fi
            ;;
        fedora)
            echo -e "${YELLOW}Installing dependencies for Fedora / RHEL...${NC}"
            sudo dnf install -y gcc-c++ cmake mpv-devel ncurses-devel dbus-devel mpv ffmpeg yt-dlp curl pkgconf-pkg-config
            ;;
        opensuse)
            echo -e "${YELLOW}Installing dependencies for openSUSE...${NC}"
            sudo zypper install -y gcc-c++ cmake mpv-devel ncurses-devel dbus-1-devel mpv ffmpeg yt-dlp curl pkg-config
            ;;
        macos)
            echo -e "${YELLOW}Installing dependencies for macOS...${NC}"
            if ! command -v brew &> /dev/null; then
                echo -e "${RED}Homebrew not found. Please install Homebrew first:${NC}"
                echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
                exit 1
            fi
            brew install cmake mpv ncurses yt-dlp ffmpeg pkg-config
            ;;
        *)
            echo -e "${RED}Unsupported or undetected distribution.${NC}"
            echo "Please ensure the following packages are installed on your system:"
            echo "  - C++17 compiler (g++ / clang++)"
            echo "  - cmake (>= 3.16)"
            echo "  - libmpv (and dev headers)"
            echo "  - ncurses (and dev headers)"
            echo "  - libdbus-1-dev (Linux only, optional)"
            echo "  - yt-dlp"
            echo "  - ffmpeg"
            echo "  - curl"
            ;;
    esac

    # Verify yt-dlp installation
    if command -v yt-dlp &> /dev/null; then
        echo -e "${GREEN}yt-dlp detected:${NC} $(yt-dlp --version 2>/dev/null || echo 'OK')"
    else
        echo -e "${YELLOW}Warning: yt-dlp not found in PATH. YouTube streaming will be limited until yt-dlp is installed.${NC}"
    fi
}

# Build the application
build_app() {
    echo ""
    echo -e "${YELLOW}Configuring and building Vibe-Fi...${NC}"
    
    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    
    CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
    cmake --build build -j"$CORES"
    
    echo -e "${GREEN}Build completed successfully!${NC}"
}

# Install the binary
install_binary() {
    echo ""
    read -p "Install 'vibe' system-wide to /usr/local/bin? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}Installing to /usr/local/bin...${NC}"
        sudo cmake --install build --prefix /usr/local
        echo -e "${GREEN}Installed successfully! You can now launch Vibe-Fi by typing 'vibe'.${NC}"
    else
        echo -e "${YELLOW}Skipping system installation.${NC}"
        echo -e "You can run the application directly from: ${GREEN}./build/vibe_fi${NC}"
    fi
}

# Main execution flow
main() {
    echo "Step 1: Checking and installing dependencies..."
    install_dependencies
    
    echo ""
    echo "Step 2: Compiling Vibe-Fi..."
    build_app
    
    echo ""
    echo "Step 3: Installation..."
    install_binary
    
    echo ""
    echo -e "${GREEN}======================================"
    echo "       Installation Complete!"
    echo "======================================${NC}"
    echo ""
    echo "Usage:"
    if [[ -x /usr/local/bin/vibe ]]; then
        echo "  vibe [query / url / audio_file]"
    else
        echo "  ./build/vibe_fi [query / url / audio_file]"
    fi
    echo ""
    echo "Keybindings:"
    echo "  - [SPACE]    Play / Pause"
    echo "  - [S]        YouTube Search"
    echo "  - [L]        Local Audio Library"
    echo "  - [P]        Custom Playlists"
    echo "  - [C]        View Play Queue"
    echo "  - [T]        Cycle Themes (Midnight / Matrix / Nord)"
    echo "  - [V]        Cycle Visualizers (Neon Flame / Stereo Bars / Pulse)"
    echo "  - [ESC / Q]  Back / Quit"
    echo ""
}

main

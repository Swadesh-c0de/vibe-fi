#!/bin/bash

set -e  # Exit on error

echo "============"
echo "  Vibe-Fi"
echo "============"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ -f /etc/os-release ]]; then
        . /etc/os-release
        if [[ "$ID" == "arch" ]] || [[ "$ID_LIKE" == *"arch"* ]]; then
            echo "arch"
        elif [[ "$ID" == "ubuntu" ]] || [[ "$ID" == "debian" ]] || [[ "$ID_LIKE" == *"debian"* ]]; then
            echo "ubuntu"
        else
            echo "unknown"
        fi
    else
        echo "unknown"
    fi
}

OS=$(detect_os)

echo -e "${GREEN}Detected OS: $OS${NC}"
echo ""

# Install dependencies based on OS
install_dependencies() {
    case $OS in
        arch)
            echo -e "${YELLOW}Installing dependencies for Arch Linux...${NC}"
            sudo pacman -Sy --needed --noconfirm base-devel cmake mpv ncurses yt-dlp ffmpeg
            ;;
        ubuntu)
            echo -e "${YELLOW}Installing dependencies for Ubuntu/Debian...${NC}"
            sudo apt update
            sudo apt install -y build-essential cmake libmpv-dev libncurses-dev mpv ffmpeg python3 curl
            
            # Install yt-dlp (not in default repos for older Ubuntu)
            if ! command -v yt-dlp &> /dev/null; then
                echo -e "${YELLOW}Installing yt-dlp...${NC}"
                sudo wget https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -O /usr/local/bin/yt-dlp
                sudo chmod a+rx /usr/local/bin/yt-dlp
            fi
            
            # Verify yt-dlp installation
            if ! yt-dlp --version &> /dev/null; then
                echo -e "${RED}Error: yt-dlp failed to run. Please check if python3 is installed correctly.${NC}"
                exit 1
            fi
            ;;
        macos)
            echo -e "${YELLOW}Installing dependencies for macOS...${NC}"
            
            # Check if Homebrew is installed
            if ! command -v brew &> /dev/null; then
                echo -e "${RED}Homebrew not found. Please install Homebrew first:${NC}"
                echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
                exit 1
            fi
            
            brew install cmake mpv ncurses yt-dlp ffmpeg pkg-config
            ;;
        *)
            echo -e "${RED}Unsupported OS. Please install dependencies manually:${NC}"
            echo "  - cmake"
            echo "  - mpv (and libmpv-dev)"
            echo "  - ncurses (and libncurses-dev)"
            echo "  - yt-dlp"
            echo "  - ffmpeg"
            echo "  - build tools (gcc/g++ or clang)"
            exit 1
            ;;
    esac
}

# Build the application
build_app() {
    echo ""
    echo -e "${YELLOW}Building Vibe-Fi...${NC}"
    
    # Clean previous build
    rm -rf build
    mkdir -p build
    cd build
    
    # Configure and build
    cmake ..
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
    
    cd ..
    
    echo -e "${GREEN}Build successful!${NC}"
}

# Install the binary
install_binary() {
    echo ""
    read -p "Install to /usr/local/bin? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}Installing vibe-fi to /usr/local/bin...${NC}"
        sudo cp build/vibe_fi /usr/local/bin/vibe
        sudo chmod +x /usr/local/bin/vibe
        echo -e "${GREEN}Installed! You can now run 'vibe' from anywhere.${NC}"
    else
        echo -e "${YELLOW}Skipping system installation.${NC}"
        echo -e "You can run the app with: ${GREEN}./build/vibe_fi${NC}"
    fi
}

# Main installation flow
main() {
    echo "Step 1: Installing dependencies..."
    install_dependencies
    
    echo ""
    echo "Step 2: Building application..."
    build_app
    
    echo ""
    echo "Step 3: Installation..."
    install_binary
    
    echo ""
    echo -e "${GREEN}======================================"
    echo "  Installation Complete!"
    echo "======================================${NC}"
    echo ""
    echo "Usage:"
    if [[ -f /usr/local/bin/vibe ]]; then
        echo "  Run: vibe"
    else
        echo "  Run: ./build/vibe"
    fi
    echo ""
    echo "Controls:"
    echo "  - Navigate with arrow keys"
    echo "  - Press 'L' for Library"
    echo "  - Press 'S' for Search"
    echo "  - Press 'Q' to return to queue/results"
    echo "  - Press 'R' to replay last track"
    echo "  - Press ESC to quit"
    echo ""
}

# Run main installation
main

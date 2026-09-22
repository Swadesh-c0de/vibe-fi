#!/usr/bin/env bash

set -e  # Exit on error

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

echo -e "${CYAN}─────────────────────────────────────────────────────────────${NC}"
echo -e "  ${BOLD}Vibe-Fi Uninstaller${NC}"
echo -e "${CYAN}─────────────────────────────────────────────────────────────${NC}"
echo "This script will remove Vibe-Fi binaries and setup from your system."
echo ""

# Detect installed binary paths
CANDIDATE_PATHS=(
    "$HOME/.local/bin/vibe"
    "$HOME/.local/bin/vibe_fi"
    "/usr/local/bin/vibe"
    "/usr/local/bin/vibe_fi"
    "/usr/bin/vibe"
    "/usr/bin/vibe_fi"
)

FOUND_BINS=()
for path in "${CANDIDATE_PATHS[@]}"; do
    if [[ -f "$path" || -L "$path" ]]; then
        FOUND_BINS+=("$path")
    fi
done

if [[ ${#FOUND_BINS[@]} -gt 0 ]]; then
    echo -e "${YELLOW}Detected installed binary locations:${NC}"
    for bin in "${FOUND_BINS[@]}"; do
        echo -e "  -> ${CYAN}$bin${NC}"
    done
    echo ""
else
    echo -e ":: No installed Vibe-Fi binaries found on standard paths."
    echo ""
fi

# Confirmation prompt
read -p "Are you sure you want to uninstall Vibe-Fi? (y/N): " -r CONFIRM
echo ""
if [[ ! "$CONFIRM" =~ ^[Yy]$ ]]; then
    echo -e "-> Uninstallation cancelled. No changes were made."
    exit 0
fi

# Remove binaries
if [[ ${#FOUND_BINS[@]} -gt 0 ]]; then
    echo -e "Removing binaries..."
    for bin in "${FOUND_BINS[@]}"; do
        if [[ -w "$bin" && -w "$(dirname "$bin")" ]]; then
            rm -f "$bin"
            echo -e "  [removed] $bin"
        else
            echo -e "  :: Administrator permission required for $bin..."
            sudo rm -f "$bin"
            echo -e "  [removed] $bin"
        fi
    done
    echo ""
fi

# Clean local build directory if in repository
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -d "$SCRIPT_DIR/build" ]]; then
    read -p "Clean local build directory ($SCRIPT_DIR/build)? (y/N): " -r CLEAN_BUILD
    if [[ "$CLEAN_BUILD" =~ ^[Yy]$ ]]; then
        rm -rf "$SCRIPT_DIR/build"
        echo -e "  [removed] $SCRIPT_DIR/build"
        echo ""
    fi
fi

# Optional data directory removal (~/.vibe-fi)
VIBE_CONFIG_DIR="$HOME/.vibe-fi"
if [[ -d "$VIBE_CONFIG_DIR" ]]; then
    echo -e "${YELLOW}User playlists and settings are stored in:${NC} ${CYAN}$VIBE_CONFIG_DIR${NC}"
    read -p "Do you also want to remove your playlists and config (~/.vibe-fi)? (y/N): " -r REMOVE_DATA
    echo ""
    if [[ "$REMOVE_DATA" =~ ^[Yy]$ ]]; then
        rm -rf "$VIBE_CONFIG_DIR"
        echo -e "  [removed] $VIBE_CONFIG_DIR"
    else
        echo -e "  [retained] $VIBE_CONFIG_DIR (playlists and settings preserved)"
    fi
    echo ""
fi

echo -e "${CYAN}─────────────────────────────────────────────────────────────${NC}"
echo -e "  ${GREEN}${BOLD}Vibe-Fi has been successfully uninstalled.${NC}"
echo -e "${CYAN}─────────────────────────────────────────────────────────────${NC}"
echo ""

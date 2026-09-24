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

# Clean isolated bottle dependencies (~/.vibe-fi/bottle)
BOTTLE_DIR="$HOME/.vibe-fi/bottle"
MANIFEST_FILE="$BOTTLE_DIR/manifest.json"

PKG_MGR=""
INSTALLED_SYS_PKGS=()

if [[ -f "$MANIFEST_FILE" ]]; then
    PKG_MGR=$(grep -o '"package_manager"[[:space:]]*:[[:space:]]*"[^"]*"' "$MANIFEST_FILE" 2>/dev/null | head -n1 | sed -E 's/.*:[[:space:]]*"([^"]*)".*/\1/')
    while IFS= read -r line; do
        if [[ -n "$line" ]]; then
            INSTALLED_SYS_PKGS+=("$line")
        fi
    done < <(sed -n '/"installed_system_packages"/,/\]/p' "$MANIFEST_FILE" 2>/dev/null | grep -o '"[^"]*"' | sed 's/"//g' | grep -v '^installed_system_packages$')
fi

if [[ -d "$BOTTLE_DIR" ]]; then
    echo -e "Cleaning isolated bottle dependencies..."
    rm -rf "$BOTTLE_DIR"
    echo -e "  [removed] $BOTTLE_DIR (isolated dependencies cleaned)"
    echo ""
fi

# Reverse-dependency check on tracked system packages
if [[ ${#INSTALLED_SYS_PKGS[@]} -gt 0 ]]; then
    echo -e "${YELLOW}Checking tracked system packages installed for Vibe-Fi...${NC}"
    REMOVABLE_PKGS=()

    for pkg in "${INSTALLED_SYS_PKGS[@]}"; do
        in_use=0
        case "$PKG_MGR" in
            apt)
                rdeps_count=$(apt-cache rdepends --installed "$pkg" 2>/dev/null | grep -E '^  [a-zA-Z0-9]' | grep -v "$pkg" | wc -l)
                if [[ $rdeps_count -gt 0 ]]; then
                    in_use=1
                fi
                ;;
            pacman)
                if ! pacman -Qi "$pkg" 2>/dev/null | grep -E '^Required By\s*:\s*(None|none)' >/dev/null 2>&1; then
                    in_use=1
                fi
                ;;
            dnf|zypper)
                if rpm -q --whatrequires "$pkg" 2>/dev/null | grep -v 'no package requires' | grep -v 'is not installed' | grep -q '[^[:space:]]'; then
                    in_use=1
                fi
                ;;
            brew)
                brew_rdeps=$(brew uses --installed "$pkg" 2>/dev/null)
                if [[ -n "$brew_rdeps" ]]; then
                    in_use=1
                fi
                ;;
        esac

        if [[ $in_use -eq 1 ]]; then
            echo -e "  :: Dependency Guard: $pkg is now required by other software on your system."
            echo -e "  [retained] $pkg (skipping removal to prevent breaking other apps)"
        else
            REMOVABLE_PKGS+=("$pkg")
        fi
    done

    if [[ ${#REMOVABLE_PKGS[@]} -gt 0 ]]; then
        echo ""
        echo -e "${YELLOW}The following system package(s) were installed for Vibe-Fi and are not needed by other apps:${NC}"
        for pkg in "${REMOVABLE_PKGS[@]}"; do
            echo -e "  -> ${CYAN}$pkg${NC}"
        done
        read -p "Do you also want to remove these packages from your system? (y/N): " -r REMOVE_PKGS
        echo ""
        if [[ "$REMOVE_PKGS" =~ ^[Yy]$ ]]; then
            echo -e "Removing package(s)..."
            case "$PKG_MGR" in
                apt)
                    sudo apt-get remove -y "${REMOVABLE_PKGS[@]}"
                    ;;
                pacman)
                    sudo pacman -R --noconfirm "${REMOVABLE_PKGS[@]}"
                    ;;
                dnf)
                    sudo dnf remove -y "${REMOVABLE_PKGS[@]}"
                    ;;
                zypper)
                    sudo zypper remove -y "${REMOVABLE_PKGS[@]}"
                    ;;
                brew)
                    brew uninstall "${REMOVABLE_PKGS[@]}"
                    ;;
            esac
            echo -e "  [removed] Tracked system packages uninstalled."
        else
            echo -e "  [retained] System packages preserved."
        fi
        echo ""
    fi
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


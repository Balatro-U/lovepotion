#!/bin/bash

# Simple dependency installer for LOVE Potion Wii U build
# Run as: bash install_deps.sh

set -e

# List of required packages
PKGS="pacman
cmake
ninja-build
zip
wget
dos2unix
build-essential
git
python3
python3-pip
cmake
dos2unix
wget
python3-setuptools
python3-mako
bison
flex
meson
ninja-build
zip
openjdk-11-jre-headless"

DevkitPro="wut-tools
wut
ppc-zlib
ppc-libpng
ppc-libjpeg-turbo
ppc-freetype
ppc-libogg
ppc-libvorbis
ppc-libvorbisidec
ppc-bzip2"

# Detect package manager
if command -v apt-get &> /dev/null; then
    PM="apt-get"
    INSTALL="sudo apt-get install -y"
elif command -v dnf &> /dev/null; then
    PM="dnf"
    INSTALL="sudo dnf install -y"
elif command -v pacman &> /dev/null; then
    PM="pacman"
    INSTALL="sudo pacman -S --noconfirm"
else
    echo "Unsupported package manager. Please install dependencies manually: $PKGS"
    exit 1
fi

# Update and install
echo "Updating package lists..."
sudo $PM update -y || true

echo "Installing dependencies: $PKGS"
$INSTALL $PKGS

echo "Installing DevkitPro..."
sudo dkp-pacman -S --needed --noconfirm $DevkitPro
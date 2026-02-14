#!/usr/bin/env bash

# setup.sh
# Installs dependencies required to build and run the project on Ubuntu/Debian-based systems

set -e

echo "Updating package lists..."
sudo apt update

echo "Installing base development tools..."
sudo apt install -y build-essential pkg-config

echo "Installing required libraries..."
sudo apt install -y \
    libglfw3-dev \
    libglm-dev \
    libassimp-dev \
    libx11-dev \
    libxi-dev \
    libxrandr-dev \
    libxxf86vm-dev \
    libxcursor-dev \
    libxinerama-dev

echo ""
echo "=================================================="
echo "Vulkan SDK is NOT installed by this script."
echo "Please install it manually from:"
echo "https://vulkan.lunarg.com/"
echo "=================================================="
echo ""

echo "Setup completed successfully."

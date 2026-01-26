#!/bin/bash
set -e

echo "Installing oatmux dependencies..."

if command -v apt-get &> /dev/null; then
    sudo apt-get update
    sudo apt-get install -y build-essential cmake libssl-dev
elif command -v dnf &> /dev/null; then
    sudo dnf install -y gcc make cmake openssl-devel
elif command -v pacman &> /dev/null; then
    sudo pacman -S --needed base-devel cmake openssl
elif command -v apk &> /dev/null; then
    sudo apk add build-base cmake openssl-dev
else
    echo "Unknown package manager. Please install manually:"
    echo "  - gcc/build-essential"
    echo "  - cmake"
    echo "  - libssl-dev / openssl-devel"
    exit 1
fi

echo "Dependencies installed."

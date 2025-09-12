#!/bin/bash

# Script to check and install Python 3.12 or higher
# Supports: macOS, Ubuntu, Arch Linux

set -e

REQUIRED_MAJOR=3
REQUIRED_MINOR=12

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check Python version
check_python_version() {
    local python_cmd=$1
    
    if ! command -v "$python_cmd" &> /dev/null; then
        return 1
    fi
    
    local version_output
    version_output=$($python_cmd --version 2>&1)
    
    # Extract version numbers
    local version
    version=$(echo "$version_output" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
    
    if [[ -z "$version" ]]; then
        return 1
    fi
    
    local major minor patch
    IFS='.' read -r major minor patch <<< "$version"
    
    echo "Found $python_cmd version: $version"
    
    if [[ $major -gt $REQUIRED_MAJOR ]] || 
       [[ $major -eq $REQUIRED_MAJOR && $minor -ge $REQUIRED_MINOR ]]; then
        return 0
    else
        return 1
    fi
}

# Function to detect OS
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ -f /etc/arch-release ]]; then
        echo "arch"
    elif [[ -f /etc/debian_version ]] || [[ -f /etc/ubuntu-release ]]; then
        echo "ubuntu"
    else
        echo "unknown"
    fi
}

# Function to install Python on macOS
install_python_macos() {
    print_info "Installing Python on macOS..."
    
    if ! command -v brew &> /dev/null; then
        print_error "Homebrew is not installed. Please install Homebrew first:"
        echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi
    
    print_info "Updating Homebrew..."
    brew update
    
    print_info "Installing Python 3.12..."
    brew install python@3.12
    
    # Create symlinks if needed
    if [[ ! -L /usr/local/bin/python3.12 ]]; then
        brew link python@3.12
    fi
}

# Function to install Python on Ubuntu
install_python_ubuntu() {
    print_info "Installing Python on Ubuntu..."
    
    print_info "Updating package list..."
    sudo apt update
    
    # Add deadsnakes PPA for newer Python versions
    print_info "Adding deadsnakes PPA..."
    sudo apt install -y software-properties-common
    sudo add-apt-repository -y ppa:deadsnakes/ppa
    sudo apt update
    
    print_info "Installing Python 3.12..."
    sudo apt install -y python3.12 python3.12-venv python3.12-pip python3.12-dev
    
    # Set up alternatives
    sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.12 1
}

# Function to install Python on Arch Linux
install_python_arch() {
    print_info "Installing Python on Arch Linux..."
    
    print_info "Updating package database..."
    sudo pacman -Sy
    
    print_info "Installing Python..."
    sudo pacman -S --noconfirm python python-pip
}

# Main function
main() {
    print_info "Checking Python version requirements (>= ${REQUIRED_MAJOR}.${REQUIRED_MINOR})"
    
    # Check various Python commands
    local python_found=false
    local python_commands=("python3.12" "python3.13" "python3.14" "python3" "python")
    
    for cmd in "${python_commands[@]}"; do
        if check_python_version "$cmd"; then
            print_success "Python requirement satisfied with $cmd"
            python_found=true
            break
        fi
    done
    
    if [[ "$python_found" == "true" ]]; then
        print_success "Python version check passed!"
        exit 0
    fi
    
    print_warning "Python ${REQUIRED_MAJOR}.${REQUIRED_MINOR}+ not found or version is too old"
    
    # Detect OS
    local os
    os=$(detect_os)
    print_info "Detected OS: $os"
    
    # Ask user for permission to install
    echo
    print_warning "Do you want to install/update Python? (y/N)"
    read -r response
    
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        print_info "Installation cancelled by user"
        exit 0
    fi
    
    # Install based on OS
    case $os in
        "macos")
            install_python_macos
            ;;
        "ubuntu")
            install_python_ubuntu
            ;;
        "arch")
            install_python_arch
            ;;
        *)
            print_error "Unsupported operating system: $os"
            print_info "Please install Python ${REQUIRED_MAJOR}.${REQUIRED_MINOR}+ manually"
            exit 1
            ;;
    esac
    
    # Verify installation
    print_info "Verifying installation..."
    sleep 2
    
    python_found=false
    for cmd in "${python_commands[@]}"; do
        if check_python_version "$cmd"; then
            print_success "Installation successful! Python requirement satisfied with $cmd"
            python_found=true
            break
        fi
    done
    
    if [[ "$python_found" == "false" ]]; then
        print_error "Installation completed but Python ${REQUIRED_MAJOR}.${REQUIRED_MINOR}+ still not found"
        print_info "You may need to restart your terminal or add Python to your PATH"
        exit 1
    fi
}

# Run main function
main "$@" 
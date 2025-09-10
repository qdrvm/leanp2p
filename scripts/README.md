# Python Version Check Script

This script checks for Python version 3.12 or higher and offers installation if requirements are not met.

## Supported Operating Systems

- **macOS** - uses Homebrew
- **Ubuntu/Debian** - uses apt with deadsnakes PPA
- **Arch Linux** - uses pacman

## Usage

1. Make the script executable:
   ```bash
   chmod +x scripts/check_python.sh
   ```

2. Run the script:
   ```bash
   ./scripts/check_python.sh
   ```

## What the Script Does

1. **Python Version Check**: Searches for available Python commands and checks their versions
2. **OS Detection**: Automatically detects the operating system
3. **Installation Offer**: If a suitable version is not found, offers to install
4. **Installation**: Uses the appropriate package manager for installation
5. **Result Verification**: Ensures the installation was successful

## Example Output

### Successful Check
```
[INFO] Checking Python version requirements (>= 3.12)
Found python3.12 version: 3.12.1
[SUCCESS] Python requirement satisfied with python3.12
[SUCCESS] Python version check passed!
```

### Installation on Ubuntu
```
[INFO] Checking Python version requirements (>= 3.12)
Found python3 version: 3.10.6
[WARNING] Python 3.12+ not found or version is too old
[INFO] Detected OS: ubuntu
[WARNING] Do you want to install/update Python? (y/N)
y
[INFO] Installing Python on Ubuntu...
[INFO] Updating package list...
[INFO] Adding deadsnakes PPA...
[INFO] Installing Python 3.12...
[INFO] Verifying installation...
Found python3.12 version: 3.12.7
[SUCCESS] Installation successful! Python requirement satisfied with python3.12
```

## Requirements

### macOS
- Homebrew must be installed
- If Homebrew is not installed, the script will provide installation instructions

### Ubuntu/Debian
- Sudo privileges for package installation
- Internet access to add PPA

### Arch Linux
- Sudo privileges to use pacman
- Internet access for package updates

## Security

The script:
- Always asks for permission before installation
- Uses official package repositories
- Does not modify system files without permission
- Can be safely interrupted at any time (Ctrl+C)

## Troubleshooting

### Script cannot find Python after installation
Restart your terminal or run:
```bash
source ~/.bashrc  # for bash
source ~/.zshrc   # for zsh
```

### Permission errors
Make sure you have sudo privileges and your user can install packages.

### Homebrew not found (macOS)
Install Homebrew:
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

## Script Features

- **Multi-platform support**: Works on macOS, Ubuntu/Debian, and Arch Linux
- **Smart detection**: Checks multiple Python command variants (python3.12, python3.13, python3, python)
- **Safe installation**: Always prompts before making changes
- **Colored output**: Easy-to-read status messages
- **Error handling**: Graceful handling of edge cases and errors
- **Verification**: Post-installation verification to ensure success 
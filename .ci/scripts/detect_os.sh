#!/usr/bin/env bash
set -euo pipefail
# set -x

detect_os() {
  if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if command -v apt >/dev/null 2>&1; then
      echo "linux_deb"
    else
      echo "linux_other"
    fi
  elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "macos"
  else
    echo "unknown"
  fi
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  echo "This script is intended to be sourced or used as a library."
  echo "Exported functions: detect_os"
  exit 1
fi

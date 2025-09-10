#!/usr/bin/env bash
set -euo pipefail
# set -x

init_vcpkg() {
  if [[ ! -d $VCPKG || -z "$(ls -A $VCPKG 2>/dev/null)" ]]; then
    echo "Directory $VCPKG does not exist or is empty. Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git $VCPKG
  fi

  if [[ ! -e $VCPKG/vcpkg ]]; then
    echo "vcpkg executable not found. Bootstrapping vcpkg..."
    $VCPKG/bootstrap-vcpkg.sh -disableMetrics
  fi

  echo "vcpkg is initialized at $VCPKG."
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  echo "This script is intended to be sourced or used as a library."
  echo "Exported functions: init_vcpkg"
  exit 1
fi

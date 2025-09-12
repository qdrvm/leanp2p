#!/usr/bin/env bash
set -euo pipefail
# set -x

init_py() {
  echo "$VENV"
  python3 -m venv "$VENV"
  source $VENV/bin/activate
  pip3 install cmake==${CMAKE_VERSION}
  pip3 install --no-cache-dir asn1tools
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  echo "This script is intended to be sourced or used as a library."
  echo "Exported functions: init_py"
  exit 1
fi

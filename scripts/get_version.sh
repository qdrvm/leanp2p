#!/bin/sh -eu
#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#
#
# Version String Generator for Git Repositories
#
# This script generates a version string based on the current Git state.
# It retrieves the latest tag, calculates the number of commits since the last tag,
# includes the current branch (if different from master), and appends the commit hash.
# If the working directory has uncommitted changes, it marks the version as "dirty".
#
# Usage:
#   ./get_version.sh [--sanitized]
#
# Options:
#   --sanitized  Replaces non-alphanumeric characters in the version string
#                with hyphens for compatibility with package managers.
#
# Output:
#   Prints a version string in the format:
#   <latest_tag>-<commits_since_tag>-<branch>-<commits_since_fork>-<commit_hash>
#   If the repository is dirty, "-dirty" is appended.
#
# Example:
#   v1.2.3-5-feature-branch-2-a1b2c3d-dirty
#
# Requirements:
#   - Git
#   - sed (for --sanitized mode)
#
# Notes:
#   - If the repository has no tags, an initial tag "_init" is created.
#   - If the repository is not a Git repo, outputs "Unknown(no git)".
#   - If Git is not installed, it exits with an error.
#

sanitize_version() {
  echo "$1" | sed -E 's/[^a-zA-Z0-9.+~:-]/-/g'
}

realpath() {
  case "$1" in
    /*) echo "$1" ;;
    *) echo "$(pwd)/$1" ;;
  esac
}

cd "$(dirname "$(realpath "$0")")"

SANITIZED=false
[ "$#" -gt 0 ] && [ "$1" = "--sanitized" ] && SANITIZED=true

if [ -x "$(command -v git)" ] && [ -d "$(git rev-parse --git-dir 2>/dev/null)" ]; then
  HEAD=$(git rev-parse --short HEAD)

  # Determine the main branch (fallback to default names if necessary)
  MAIN_BRANCH=$(git symbolic-ref refs/remotes/origin/HEAD 2>/dev/null | sed 's@^refs/remotes/origin/@@')
  if [ -z "$MAIN_BRANCH" ]; then
    MAIN_BRANCH=$(git branch --format='%(refname:short)' | grep -E '^(main|master)$' | head -n1)
  fi
  if [ -z "$MAIN_BRANCH" ]; then
    MAIN_BRANCH="master"  # Fallback to "master" if no main branch detected
  fi

  COMMON=$(git merge-base HEAD "$MAIN_BRANCH" 2>/dev/null || echo "$HEAD")

  if ! git tag | grep -q .; then
    ROOT_COMMIT=$(git rev-list --max-parents=0 HEAD || echo "")
    [ -n "$ROOT_COMMIT" ] && ! git tag | grep -q "_init" && git tag _init "$ROOT_COMMIT"
  fi

  DESCR=$(git describe --tags --long "$COMMON" 2>/dev/null || echo "")
  [ -z "$DESCR" ] && DESCR="$HEAD-0-g$HEAD"

  TAG_IN_MASTER=$(echo "$DESCR" | sed -E "s/v?(.*)-([0-9]+)-g[a-f0-9]+/\1/")
  TAG_TO_FORK_DISTANCE=$(echo "$DESCR" | sed -E "s/v?(.*)-([0-9]+)-g[a-f0-9]+/\2/")

  BRANCH=$(git branch --show-current 2>/dev/null || echo "$HEAD")
  FORK_TO_HEAD_DISTANCE=$(git rev-list --count "$COMMON..HEAD" 2>/dev/null || echo "0")

  RESULT=$TAG_IN_MASTER
  [ "$TAG_TO_FORK_DISTANCE" != "0" ] && RESULT="$RESULT-$TAG_TO_FORK_DISTANCE"
  [ "$BRANCH" != "$MAIN_BRANCH" ] && RESULT="$RESULT-$BRANCH-$FORK_TO_HEAD_DISTANCE-$HEAD"

  git diff --quiet || DIRTY="-dirty"
  RESULT="$RESULT${DIRTY:-}"
else
  RESULT="Unknown(no git)"
fi

[ "$SANITIZED" = true ] && RESULT=$(sanitize_version "$RESULT")

printf "%s" "$RESULT"

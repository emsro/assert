#!/usr/bin/env sh
# Run a CI preset inside a Docker container that mirrors the GitHub Actions
# ubuntu-latest environment.
#
# Usage:
#   ./docker-ci.sh <preset>
#
# Presets: debug | asan | ubsan | release | coverage | clang-tidy
#
# Each preset gets its own named Docker volume for the build directory so
# multiple presets can be run without clobbering each other, and incremental
# builds are preserved between runs.

set -e

PRESET=${1:-debug}
IMAGE=assert-ci
VOLUME=assert-ci-build-${PRESET}
REPO_ROOT=$(cd "$(dirname "$0")" && pwd)

docker build -t "$IMAGE" "$REPO_ROOT"

if [ "$PRESET" = "clang-tidy" ]; then
    docker run --rm \
        -v "${REPO_ROOT}":/src:ro \
        -v "${VOLUME}":/src/_build \
        -w /src \
        "$IMAGE" \
        sh -c 'cmake --preset debug && make clang-tidy'
else
    docker run --rm \
        -v "${REPO_ROOT}":/src:ro \
        -v "${VOLUME}":/src/_build \
        -w /src \
        "$IMAGE" \
        cmake --workflow --preset "$PRESET"
fi

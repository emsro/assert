FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Base build tools + gcc-15 / clang (matches CI)
RUN apt-get update && apt-get install -y --no-install-recommends \
        software-properties-common \
        ca-certificates \
    && add-apt-repository ppa:ubuntu-toolchain-r/test -y \
    && apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        git \
        make \
        gcc-15 \
        g++-15 \
        clang \
        clang-tidy \
        python3 \
        python3-pip \
    && pip3 install --break-system-packages cmake gcovr \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

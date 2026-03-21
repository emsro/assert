FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Base build tools + clang-18 (matches ubuntu-latest in CI)
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        git \
        make \
        clang \
        clang-tidy \
        python3 \
        python3-pip \
        ca-certificates \
    && pip3 install --break-system-packages cmake gcovr \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

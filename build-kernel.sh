#!/usr/bin/env bash
set -x -e

# Make sure we have the latest version of the image
docker pull simonvanderveldt/rpi3-kernel-builder

# run build
docker run --rm -ti \
  -v "${PWD}":/workdir/linux \
  -v "${PWD}/build":/workdir/build \
  -e DEFCONFIG=norns_defconfig \
  simonvanderveldt/rpi3-kernel-builder

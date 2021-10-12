#!/usr/bin/env bash
set -x -e

# Make sure we have the latest version of the image
docker pull ghcr.io/monome/norns-kernel-builder/image:latest

# run build
docker run --rm \
  -v "${PWD}":/workdir \
  -e DEFCONFIG=norns_defconfig \
  ghcr.io/monome/norns-kernel-builder/image:latest

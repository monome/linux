#!/usr/bin/env bash
set -x -e

# Make sure we have the latest version of the image
docker pull docker.pkg.github.com/monome/norns-kernel-builder/image:latest

# run build
docker run --rm -ti \
  -v "${PWD}":/workdir \
  -e DEFCONFIG=norns_defconfig \
  docker.pkg.github.com/monome/norns-kernel-builder/image:latest

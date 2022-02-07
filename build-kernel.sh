#!/usr/bin/env bash

CONFIGS=()
PULL_IMAGE=YES
BUILDER_IMAGE_NAME="ghcr.io/monome/norns-kernel-builder/image"
BUILDER_IMAGE_TAG="latest"

while [[ $# -gt 0 ]]; do
  case $1 in
    --skip-pull)
      PULL_IMAGE=NO
      shift # past argument
      ;;
    --image-tag)
      BUILDER_IMAGE_TAG="$2"
      shift # past argument
      shift # past value
      ;;
    --image-name)
      BUILDER_IMAGE_NAME="$2"
      shift
      shift
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      CONFIGS+=("$1") # save positional arg
      shift # past argument
      ;;
  esac
done

if [[ -z "${CONFIGS}" ]]; then
  CONFIGS+=("norns")
fi

echo "Building configs: ${CONFIGS}"

set -e

BUILDER_IMAGE="${BUILDER_IMAGE_NAME}:${BUILDER_IMAGE_TAG}"
if [[ ${PULL_IMAGE} == YES ]]; then
  CMD="docker pull ${BUILDER_IMAGE}"
  echo $CMD; ${CMD}
fi

for config in "${CONFIGS[@]}"; do
  CMD="docker run --rm \
    -v "${PWD}":/workdir \
    -e DEFCONFIG=${config}_defconfig \
    ${BUILDER_IMAGE}"
  echo $CMD; ${CMD}
done

#!/bin/bash
set -e

DEVICE="$1"
if [ -z "$DEVICE" ]; then
  echo "Usage: sh build.sh <device>  (e.g. sh build.sh 1)"
  exit 1
fi

BOARD="xiao_nrf54l15/nrf54l15/cpuapp"

if [ "$DEVICE" = "1" ]; then
  west build -b "$BOARD"
else
  west build -b "$BOARD" -- -DEXTRA_CONF_FILE=device${DEVICE}.conf
fi

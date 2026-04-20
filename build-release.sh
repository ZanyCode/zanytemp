#!/bin/bash
set -e

DEVICE="$1"
if [ -z "$DEVICE" ]; then
  echo "Usage: sh build-release.sh <device>  (e.g. sh build-release.sh 1)"
  exit 1
fi

BOARD="xiao_nrf54l15/nrf54l15/cpuapp"

if [ "$DEVICE" = "1" ]; then
  west build -b "$BOARD" -- -DCONF_FILE="prj_release.conf"
else
  west build -b "$BOARD" -- -DCONF_FILE="prj_release.conf" -DEXTRA_CONF_FILE=device${DEVICE}.conf
fi

#!/usr/bin/env bash
set -euo pipefail

LABEL="SegmeCam"
VIDEO_NR=9
DEVICES=1
EXCLUSIVE=1
PERSIST=0

usage() {
  cat <<EOU
Usage: $0 [options]
  --label NAME        Set loopback card_label (default: SegmeCam)
  --video-nr N        Set /dev/videoN number (default: 9)
  --devices N         Number of devices (default: 1)
  --exclusive 0|1     Set exclusive_caps (default: 1)
  --persist           Write modprobe config to persist across reboots
  -h, --help          Show this help

Examples:
  sudo $0 --label SegmeCam --video-nr 9
EOU
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --label) LABEL="$2"; shift 2;;
    --video-nr) VIDEO_NR="$2"; shift 2;;
    --devices) DEVICES="$2"; shift 2;;
    --exclusive) EXCLUSIVE="$2"; shift 2;;
    --persist) PERSIST=1; shift;;
    -h|--help) usage; exit 0;;
    *) echo "Unknown arg: $1"; usage; exit 1;;
  esac
done

if [[ $EUID -ne 0 ]]; then
  if command -v sudo >/dev/null 2>&1; then
    exec sudo "$0" "$@"
  else
    echo "Please run as root or install sudo." >&2
    exit 1
  fi
fi

# Unload if loaded, ignore errors
modprobe -r v4l2loopback 2>/dev/null || true

modprobe v4l2loopback \
  devices="${DEVICES}" \
  video_nr="${VIDEO_NR}" \
  card_label="${LABEL}" \
  exclusive_caps="${EXCLUSIVE}"

echo "Loaded v4l2loopback: label='${LABEL}' video_nr=${VIDEO_NR} devices=${DEVICES} exclusive_caps=${EXCLUSIVE}"

if [[ "$PERSIST" -eq 1 ]]; then
  echo v4l2loopback > /etc/modules-load.d/v4l2loopback.conf
  cat > /etc/modprobe.d/segmecam-loopback.conf <<EOF2
options v4l2loopback devices=${DEVICES} video_nr=${VIDEO_NR} card_label=${LABEL} exclusive_caps=${EXCLUSIVE}
EOF2
  echo "Persisted module options to /etc/modprobe.d/segmecam-loopback.conf"
fi

exit 0

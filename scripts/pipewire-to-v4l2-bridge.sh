#!/bin/bash
# SegmeCam PipeWire to V4L2 Bridge Script
# This script bridges SegmeCam's PipeWire output to a v4l2loopback device

set -euo pipefail

# Get video device number from argument or default to 30
VIDEO_NR="${1:-30}"
if ! [[ "$VIDEO_NR" =~ ^[0-9]+$ ]]; then
  echo "Usage: $0 [video_nr]"; exit 1
fi
V4L2_DEVICE="/dev/video${VIDEO_NR}"

# Check if v4l2loopback device exists
if [ ! -c "$V4L2_DEVICE" ]; then
    echo "Error: v4l2loopback device $V4L2_DEVICE not found."
    echo "Please install v4l2loopback and create a device:"
    echo "sudo modprobe v4l2loopback devices=1 video_nr=2 card_label='SegmeCam Virtual Camera' exclusive_caps=0"
    echo ""
    echo "To make it persistent, add to /etc/modprobe.d/v4l2loopback.conf:"
    echo "options v4l2loopback devices=1 video_nr=2 card_label='SegmeCam Virtual Camera' exclusive_caps=0"
    exit 1
fi

# Check if SegmeCam is running and has PipeWire output
if ! pw-cli ls | grep -q "SegmeCam Virtual Camera"; then
    echo "Error: SegmeCam PipeWire output not found."
    echo "Please start SegmeCam first."
    exit 1
fi

echo "Starting PipeWire to V4L2 bridge..."
echo "Bridging 'SegmeCam Virtual Camera' → $V4L2_DEVICE"
echo "Note: This bridge has known buffer compatibility issues between PipeWire and GStreamer."
echo "The v4l2loopback device is detected by applications but video transmission may fail."
echo "Press Ctrl+C to stop"

# Bridge PipeWire output to v4l2loopback device
echo "Starting PipeWire → V4L2 bridge to $V4L2_DEVICE (press Ctrl+C to stop)"
gst-launch-1.0 \
  pipewiresrc target-object="SegmeCam Virtual Camera" always-copy=true do-timestamp=true ! \
  identity check-imperfect-timestamp=true ! \
  videoconvert ! \
  video/x-raw,format=YUY2,width=640,height=480 ! \
  v4l2sink device="$V4L2_DEVICE"

echo "Bridge stopped."
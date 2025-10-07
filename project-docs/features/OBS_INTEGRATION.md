# SegmeCam OBS Integration

SegmeCam provides virtual camera output through PipeWire, which is compatible with Flatpak and Flathub. For OBS Studio integration, use the provided bridge script.

## For Flatpak/Flathub Users

Since Flatpak cannot access V4L2 devices directly, use the PipeWire to V4L2 bridge:

1. Install v4l2loopback:

   ```bash
   sudo apt install v4l2loopback-dkms
   sudo modprobe v4l2loopback devices=1 video_nr=2 card_label="SegmeCam Virtual Camera"
   ```

2. Start SegmeCam (Flatpak version)

3. Run the bridge script:

   ```bash
   ./pipewire-to-v4l2-bridge.sh
   ```

4. In OBS Studio, add a "Video Capture Device" source and select "SegmeCam Virtual Camera" (/dev/video2)

## How It Works

- SegmeCam outputs to PipeWire (no device permissions needed)
- Bridge script reads from PipeWire and feeds to v4l2loopback
- OBS sees the v4l2loopback device as a regular camera

## For Native Installation

If running SegmeCam natively (not Flatpak), it outputs directly to v4l2loopback without needing the bridge script.

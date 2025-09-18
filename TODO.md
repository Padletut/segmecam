# SegmeCam TODO

## Outstanding Issues

- [ ] Fix virtual webcam device reinitialization bug
  - Investigate and fix the bug where SegmeCam requires rerunning setup_vcam.sh after stopping and restarting the virtual cam stream. Ensure the app properly closes and reopens the v4l2loopback device, and test with latest v4l2loopback-dkms.

## How to update this file
- Add new bugs, features, or tasks as checklist items.
- Mark completed items with [x].
- Keep this file in sync with GitHub Issues and in-session todo list for best results.

# SegmeCam → Flathub Readiness: TODO

> Purpose: Make SegmeCam acceptable for Flathub by tightening sandboxing, removing broad device/filesystem permissions, and migrating camera & file selection to portals.

---

## 0) High-Level Goals
- [ ] Ship a Flatpak that runs **without** `--filesystem=host`, `--device=all`, or copied system libs.
- [ ] Use **PipeWire + Camera portal** (no direct `/dev/video*` access).
- [ ] Use **FileChooser portal** for custom background images (no host FS reads).
- [ ] Keep permissions minimal and justifiable for Flathub review.

---

## 1) Manifest Cleanup (Flatpak YAML)
- [x] Remove: `--filesystem=host`, `--filesystem=home:ro`, `--device=all`, `--device=shm`, `--filesystem=/tmp`, `--filesystem=/tmp/.X11-unix`
- [ ] Keep minimal finish-args:
  ```yaml
  finish-args:
    - --share=ipc
    - --socket=wayland
    - --socket=fallback-x11
    - --device=dri
    - --socket=pipewire
    - --filesystem=xdg-pictures:ro
    # optional if users often store backgrounds in Downloads:
    # - --filesystem=xdg-download:ro
  ```
- [ ] Remove ad-hoc env vars (`DISPLAY`, `MESA_*`, `QT_X11_NO_MITSHM`) unless proven necessary.
- [ ] Stop copying system libraries (e.g., `libGLU.so`) from `/usr/lib...`.
- [ ] If GLU is really required, depend on the proper extension or build it in:
  - [ ] Prefer runtime/extension (e.g., `org.freedesktop.Sdk.Extension.glu`) or vendor the exact lib in a module built from source.

---

## 2) Camera: V4L2 → PipeWire via Portal
**Target:** No direct `/dev/video*` access. Avoid `--device=all`.

- [ ] Integrate **org.freedesktop.portal.Camera** (via libportal or D-Bus).
  - [ ] Request camera access and obtain PipeWire node ID/FD when the user grants permission.
- [ ] Consume the camera stream via **PipeWire**:
  - **Recommended path:** GStreamer pipeline: `pipewiresrc ! videoconvert ! appsink`
  - [ ] Connect appsink frames to existing OpenCV/processing pipeline.
- [ ] Add graceful error handling if camera permission is denied or no camera is present.
- [ ] Verify performance (latency, FPS) equals/approaches V4L2 path.

**Temporary local dev option (not for Flathub):**
- If you still need V4L2 for testing, run locally with an override:
  ```bash
  flatpak override --user --device=all org.segmecam.SegmeCam
  ```
  Do **not** request this permission in the published manifest.

---

## 3) Custom Backgrounds via Portal
- [ ] Replace any direct file reads from host with **FileChooser portal** (`org.freedesktop.portal.FileChooser`).
- [ ] On selection, **import** the image into the sandbox (e.g., `~/.local/share/SegmeCam/backgrounds/`).
- [ ] Load backgrounds from the app data dir rather than absolute host paths.
- [ ] Provide a simple “Import background…” UI action and show a preview/confirmation.

---

## 4) Desktop Integration & Metadata
- [ ] Verify `.desktop` entry matches the app-id and command (wrapper or binary).  
  - `Exec=segmecam` (or the wrapper if absolutely necessary)
- [ ] Ensure `org.segmecam.SegmeCam.appdata.xml` includes:
  - [ ] Summary, description, and release notes
  - [ ] At least one 16:9 screenshot of the running app
  - [ ] Categories (e.g., `Graphics;Video;Utility;`), project/homepage links
  - [ ] Correct `<id>` matching `app-id`
- [ ] Provide icons at required sizes (e.g., 64/128/256, SVG preferred if available).

---

## 5) Local Build & Test Checklist
- [ ] Build:
  ```bash
  flatpak-builder --user --force-clean build-dir org.segmecam.SegmeCam.yml
  ```
- [ ] Run:
  ```bash
  flatpak-builder --run build-dir org.segmecam.SegmeCam.yml segmecam
  ```
- [ ] Validate no warnings/errors about missing permissions.
- [ ] Test camera flow (portal consent → video frames arrive via PipeWire).
- [ ] Test background import (portal → file copied to app data → loaded).

---

## 6) Repository Structure for Flathub
- [ ] Create a separate repo (e.g., `segmecam-flatpak`) containing only:
  - [ ] Flatpak manifest (`org.segmecam.SegmeCam.yml`)
  - [ ] `.desktop`, `.appdata.xml`/`.metainfo.xml`, icons
  - [ ] Any build modules (sources) required by the manifest
- [ ] Tie the manifest to a **tagged release** of SegmeCam (use `tag`/`commit` fields in sources).

---

## 7) Flathub Submission
- [ ] Read and follow: Flathub App Submission guidelines
- [ ] Open PR to `flathub/flathub` with your app-id `org.segmecam.SegmeCam`
- [ ] Address reviewer feedback:
  - [ ] Explain why each permission is needed (keep them minimal).
  - [ ] Prove no broad device/filesystem access is required.
  - [ ] Confirm the app runs on stable runtime (e.g., `org.freedesktop.Platform//23.08` or newer).

---

## 8) Post-Publish Tasks
- [ ] Update SegmeCam README with one-liner install:
  ```bash
  flatpak install flathub org.segmecam.SegmeCam
  ```
- [ ] Remove GitHub Releases `.flatpak` uploads (Flathub handles distribution).
- [ ] Add CI to bump the Flatpak manifest on new tags (optional).

---

## 9) Nice-to-Have (Later)
- [ ] Wayland-only path (drop `fallback-x11` once stable).
- [ ] Migrate audio stack (if ever needed) to PipeWire-only.
- [ ] Collect anonymized crash logs/metrics with explicit opt-in.
- [ ] Provide a portable “safe mode” that disables GPU effects for debugging.

---

## 10) Acceptance Criteria
- [ ] App launches from Flatpak without overrides.
- [ ] Camera works via portal/PipeWire.
- [ ] Custom background import works via portal; no host reads.
- [ ] No broad permissions in manifest; review passes on Flathub.

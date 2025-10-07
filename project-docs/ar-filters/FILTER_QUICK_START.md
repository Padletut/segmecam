# 🎭 AR Filter Quick Start Guide

## Keyboard Shortcuts

### Filter Selection (NEW!)
- **1-9** = Load filter #1-9 from available filters
- **0** = Unload current filter (disable all filters)

### Crown Position Adjustment
- **U** = Move crown UP (higher above forehead)
- **D** = Move crown DOWN (closer to forehead)
- **W** = Move crown BACKWARD (away from camera, toward top/back of head)
- **S** = Move crown FORWARD (closer to camera, toward face)
- **Ctrl+R** = RESET crown position (Y: 40%, Z: 0%)

### General
- **ESC** = Exit application

## Quick Start Workflow

1. **Launch SegmeCam:**
   ```bash
   ./segmecam mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt
   ```

2. **Load a Filter:**
   - Press **1** to load the first available filter (likely pro_beanie)
   - Or press **2**, **3**, etc. for other filters
   - Console will show: "🎭 Loading filter 1: Cozy Beanie (pro_beanie)"

3. **Adjust Position:**
   - Press **U** several times to move beanie higher
   - Press **W** several times to move beanie backward (to top of head)
   - Press **S** to bring it forward if it goes too far back
   - Press **D** to lower it if it's too high

4. **Fine-tune:**
   - Tilt your head left/right - beanie should rotate with you
   - Move your head around - beanie should follow smoothly
   - If position isn't perfect, keep adjusting with U/D/W/S keys

5. **Reset if Needed:**
   - Press **Ctrl+R** to reset to default position
   - Press **0** to unload the filter completely

## Available Filters

Filters are loaded from `assets/filters/` directory. Check available filters:
```bash
ls -1 assets/filters/
```

Each filter has a `filter.json` configuration file.

## Troubleshooting

**"Filter X not found"** → You pressed a number higher than available filters count
- Solution: Press **1** or **2** for the first/second filter

**"AR filter manager not available"** → AR system not initialized
- Solution: Restart the application

**Beanie doesn't follow head tilt** → Head pose tracking issue
- Check console for "[HEAD ROTATION]" debug logs
- Ensure good lighting and face visibility

**Beanie position wrong** → Needs adjustment
- Use U/D keys for vertical adjustment
- Use W/S keys for depth (forward/backward) adjustment

## Tips

- **Start with filter #1** (pro_beanie) to test basic functionality
- **Good lighting** improves head tracking and tilt detection
- **Face the camera** directly for best initial positioning
- **Adjust in small increments** - each key press is 5% (U/D) or 20% (W/S)
- **Save your adjustments** by modifying the filter.json file's offset values

## Example Session

```
$ ./segmecam mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt
[app starts]

[Press 1]
🎭 Loading filter 1: Cozy Beanie (pro_beanie)
   ✅ Filter loaded successfully!

[Press U 5 times]
⬆️ Crown UP (U key)
   Crown offset now: 0.45 (45%)
⬆️ Crown UP (U key)
   Crown offset now: 0.50 (50%)
...

[Press W 10 times]
⬅️ Crown BACKWARD (W key)
   Crown depth now: -0.2 (-20% backward)
...

[Beanie now fits perfectly on top of head!]
[Tilt head left/right - beanie tilts with you]
```

## Next Steps

After finding the perfect position:
1. Note the final offset values from console
2. Update filter.json to make it permanent
3. Try other filters with keys 2, 3, 4, etc.
4. Experiment with different hairstyles/head positions

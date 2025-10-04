# New AR Filters Added - Testing Reference

**Date**: January 26, 2025  
**Author**: Padletut  
**Total Filters**: 8 (5 original + 3 new)

---

## 🆕 Newly Added Filters

### 1. **Cozy Beanie** (`pro_beanie`)

**Metadata**:
- **ID**: `cozy-beanie`
- **Category**: headwear
- **Anchor**: head_top
- **Author**: Padletut

**Assets**:
- Model: `beanie.obj` (1,234 vertices estimated)
- Texture: `beanie_tex.png`
- Material: `beanie.mtl`

**Position**:
- Offset: `[0.0, 0.06, -0.03]` (slightly forward and up)
- Scale: `[1.0, 1.0, 1.0]`

**Description**: Simple knit beanie demo model for SegmeCam

**Expected Appearance**: Beanie sits on top of head, covers upper portion of forehead

---

### 2. **Holo Visor** (`pro_holo`)

**Metadata**:
- **ID**: `holo-visor`
- **Category**: futuristic
- **Anchor**: forehead
- **Author**: Padletut

**Assets**:
- Model: `visor.obj`
- Texture: `visor_tex.png`
- Material: `visor.mtl`

**Position**:
- Offset: `[0.0, 0.02, 0.08]` (forward of forehead)
- Rotation: `-6.0°` (slight tilt)
- Scale: `[1.0, 1.0, 1.0]`

**Description**: Curved holographic visor demo asset

**Expected Appearance**: Futuristic sci-fi visor across forehead/eyes, holographic style

---

### 3. **Pixel Shades** (`pro_sunglasses`)

**Metadata**:
- **ID**: `pixel-shades`
- **Category**: accessories
- **Anchor**: nose_bridge
- **Author**: Padletut

**Assets**:
- Model: `sunglasses.obj`
- Texture: `sunglasses_tex.png`
- Material: `sunglasses.mtl`

**Position**:
- Offset: `[0.0, 0.0, 0.0]` (default position)
- Rotation: `[0.0, 0.0, 0.0]` (no rotation)
- Scale: `[1.0, 1.0, 1.0]`

**Description**: Low-poly pixel sunglasses for SegmeCam (demo assets)

**Expected Appearance**: Pixelated/low-poly style sunglasses on nose bridge

---

## 📊 Complete Filter Inventory

### Original Filters (5)

1. **Cat Ears** (`cat-ears/`) - Animal ears on head
2. **Classic Glasses** (`classic-glasses/`) - Black-rimmed glasses
3. **Classic Glasses v1** (`classic-glasses-v1/`) - Variant glasses
4. **Simple Glasses** (`glasses-simple/`) - Simple black frames with blink behavior
5. **Party Hat** (`party-hat/`) - Festive cone hat

### New Filters (3)

6. **Cozy Beanie** (`pro_beanie/`) - Knit beanie headwear
7. **Holo Visor** (`pro_holo/`) - Futuristic visor
8. **Pixel Shades** (`pro_sunglasses/`) - Low-poly sunglasses

### Test Filters (1)

9. **Snap Lens** (`snap-lens/`) - Snapchat lens test (may not render)

**Total**: 9 filters available

---

## 🧪 Testing Checklist

### Phase 9 Testing (Single Filter Selection)

Run the app and test each filter individually:

```bash
./segmecam mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt
```

**For Each Filter**:

- [ ] **Cozy Beanie**
  - [ ] Filter appears in grid
  - [ ] Thumbnail displays correctly
  - [ ] Click loads filter successfully
  - [ ] Beanie renders on head_top anchor
  - [ ] Position looks natural
  - [ ] Texture displays correctly
  - [ ] Save profile → Restart → Filter restores

- [ ] **Holo Visor**
  - [ ] Filter appears in grid
  - [ ] Thumbnail displays correctly
  - [ ] Click loads filter successfully
  - [ ] Visor renders on forehead anchor
  - [ ] Rotation (-6°) looks correct
  - [ ] Holographic texture visible
  - [ ] Save profile → Restart → Filter restores

- [ ] **Pixel Shades**
  - [ ] Filter appears in grid
  - [ ] Thumbnail displays correctly
  - [ ] Click loads filter successfully
  - [ ] Sunglasses render on nose_bridge
  - [ ] Low-poly style visible
  - [ ] Texture displays correctly
  - [ ] Save profile → Restart → Filter restores

### Common Issues to Watch For

1. **Model Not Visible**:
   - Check console for loading errors
   - Verify .obj file format (ASCII, not binary)
   - Check scale values (too small/large?)

2. **Wrong Position**:
   - Adjust offset values in filter.json
   - Check anchor point is correct
   - Verify rotation values

3. **Texture Issues**:
   - Ensure PNG files are valid
   - Check MTL file references correct texture path
   - Verify UV coordinates in OBJ file

4. **Performance**:
   - Monitor render time in performance panel
   - Should be < 5ms per filter
   - Check FPS impact (should maintain 30+ FPS)

---

## 📝 Testing Notes Template

**Filter**: ________________  
**Date**: ________________  
**Tester**: ________________

**Appearance**:
- [ ] Model loads successfully
- [ ] Position looks natural
- [ ] Texture displays correctly
- [ ] Scale is appropriate

**Behavior**:
- [ ] Tracks face movement smoothly
- [ ] No jittering or artifacts
- [ ] Handles head rotation well
- [ ] Handles distance changes (near/far)

**Performance**:
- Render Time: _______ ms
- FPS Impact: _______ fps
- Memory Usage: _______ MB

**Issues Found**:
```
(List any problems, visual glitches, or unexpected behavior)
```

**Screenshots**:
```
(Attach screenshots if possible)
```

---

## 🎯 Next Steps

### Phase 9 Completion (Remaining)
- [ ] Manual testing of all 8 filters (2 hours)
- [ ] Document testing results
- [ ] Update phase progress documentation
- [ ] Mark Phase 9 complete

### Phase 10 Preview (Multi-Filter Support)
- [ ] Refactor ARFilterManager for multiple active filters
- [ ] Implement filter layering system
- [ ] Add z-ordering controls
- [ ] UI redesign for multi-select (checkboxes)
- [ ] Conflict detection (incompatible filter combinations)

---

## 📚 Filter Categories

**Accessories** (3):
- Classic Glasses, Simple Glasses, Pixel Shades

**Headwear** (2):
- Party Hat, Cozy Beanie

**Futuristic** (1):
- Holo Visor

**Animals** (1):
- Cat Ears

**Test** (1):
- Snap Lens

---

## 🔧 Quick Commands

**Build app**:
```bash
./build-app.sh
```

**Run with GPU graph**:
```bash
./segmecam mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt
```

**Check filter discovery**:
```bash
ls -la assets/filters/
```

**Validate JSON**:
```bash
cat assets/filters/pro_beanie/filter.json | jq .
cat assets/filters/pro_holo/filter.json | jq .
cat assets/filters/pro_sunglasses/filter.json | jq .
```

---

**Document Version**: 1.0  
**Last Updated**: January 26, 2025  
**Status**: Ready for Testing

# Finding & Testing 3D Snapchat Lenses

**Date**: October 4, 2025  
**Goal**: Find 3D model-based Snapchat lenses to test Phase 8's Assimp integration  
**Expected Compatibility**: 80-90% for FBX/GLB/GLTF-based lenses

---

## ✅ Current Test Results

### Existing SegmeCam Filters

We already have 2 working 3D filters that Assimp loads successfully!

#### 1. `glasses-simple` ✅

```bash
$ ./test_snap_lens_load assets/filters/glasses-simple/glasses_simple.obj

✅ Assimp successfully loaded the file!

Scene Information:
- Meshes: 1 (glasses_simple)
- Vertices: 12
- Faces: 6
- Materials: 2 (DefaultMaterial, glasses_mat)
- Texture: glasses_texture.png

Compatibility Score: 65/100 (MEDIUM)
✅ Has 3D meshes (+30)
✅ Has materials (+20)
✅ Has texture references (+15)
⚠️  No animations (0)
```

**Structure**:
```
glasses-simple/
├── glasses_simple.obj        (442 bytes - 3D model)
├── glasses_simple.mtl        (116 bytes - material)
├── glasses_texture.png       (295 bytes - texture)
└── filter.json               (1.2KB - SegmeCam config)
```

#### 2. `classic-glasses-v1` ✅

```bash
$ ./test_snap_lens_load assets/filters/classic-glasses-v1/glasses.obj

✅ Assimp successfully loaded the file!

Scene Information:
- Meshes: 1 (defaultobject)
- Vertices: 20
- Faces: 10
- Materials: 1 (DefaultMaterial)
- Texture: None

Compatibility Score: 50/100 (MEDIUM)
✅ Has 3D meshes (+30)
✅ Has materials (+20)
⚠️  No texture references (0)
⚠️  No animations (0)
```

**Structure**:
```
classic-glasses-v1/
├── glasses.obj               (1012 bytes - 3D model)
├── glasses_texture.png       (empty - needs texture)
└── filter.json               (1.2KB - SegmeCam config)
```

---

## 🎯 Where to Find 3D Snapchat Lenses

### Official Sources

1. **Lens Studio by Snap**
   - URL: https://ar.snap.com/lens-studio
   - Download: Free lens creation tool
   - Includes: Sample lenses with 3D models
   - Formats: Exports to Lens Studio format (.lns)

2. **Snap Lens Library**
   - URL: https://lensstudio.snapchat.com/templates
   - Content: Official templates with 3D models
   - Categories:
     - Face Effects (dog ears, cat nose, etc.)
     - World Effects (3D objects in space)
     - Body Effects (full body tracking)

3. **Snap Camera Archive** (Deprecated)
   - Note: Discontinued in 2023, but lenses may still be available
   - Alternative: Lens Studio templates

### Community Sources

1. **GitHub Repositories**
   ```bash
   # Search patterns
   site:github.com "snapchat lens" "3d model" fbx
   site:github.com "lens studio" "3d asset" gltf
   site:github.com snapchat lens obj file
   ```

   **Notable Repos**:
   - `snapchat/lens-studio-templates` (if exists)
   - Community-created lens collections
   - AR filter development samples

2. **3D Model Libraries** (for creating custom lenses)
   - **Sketchfab**: Free 3D models (many in GLB/GLTF)
     - URL: https://sketchfab.com
     - Search: "dog ears", "glasses", "cat nose"
     - Formats: GLB, GLTF, FBX, OBJ
   
   - **TurboSquid**: Professional 3D models
     - URL: https://www.turbosquid.com
     - Formats: FBX, OBJ, 3DS, DAE
   
   - **Free3D**: Free 3D models
     - URL: https://free3d.com
     - Formats: OBJ, FBX, 3DS, MAX

3. **Reddit Communities**
   - r/LensStudio
   - r/SnapchatLenses
   - r/AR
   - Search: "3d model lens download"

### Extracting from Snapchat App

⚠️ **Legal Note**: Only extract lenses you have permission to use!

**Android Method**:
1. Install Snapchat on Android device
2. Use a lens in Snapchat
3. Lens cache location: `/data/data/com.snapchat.android/cache/lens/`
4. Copy to computer with `adb pull`
5. Extract assets (may be encrypted)

**iOS Method**:
1. Requires jailbroken device
2. Lens cache location: `/var/mobile/Containers/Data/Application/Snapchat/Library/Caches/SCLensCache/`
3. Extract with file manager

---

## 📥 Recommended Test Cases

### Priority 1: Face-Attached 3D Objects

**Best for testing** - These attach to face landmarks (exactly what SegmeCam does!)

**Examples**:
- 🐶 **Dog Filter**: Floppy ears, nose, tongue
  - Format: Likely FBX or OBJ
  - Attachments: Ears to forehead, nose to nose tip
  - Expected Compatibility: 85-90%

- 😺 **Cat Filter**: Pointy ears, whiskers, nose
  - Format: FBX/OBJ with materials
  - Attachments: Ears to head top
  - Expected Compatibility: 85-90%

- 👓 **Glasses**: Various styles
  - Format: OBJ (simple), FBX (complex)
  - Attachments: Nose bridge, temples
  - Expected Compatibility: 90-95%

- 👑 **Crown/Tiara**: Head accessories
  - Format: FBX with metallic materials
  - Attachment: Top of head
  - Expected Compatibility: 80-85%

- 🎅 **Santa Hat**: Seasonal accessories
  - Format: OBJ/FBX
  - Attachment: Top of head
  - Expected Compatibility: 85-90%

### Priority 2: Full Face Masks

**Good for testing** - Replace entire face area

**Examples**:
- 🎭 **Masquerade Mask**: Partial face coverage
- 👽 **Alien Face**: Full face replacement
- 🤡 **Clown Makeup**: Painted face effect
- 🦸 **Superhero Mask**: Eye area coverage

### Priority 3: Animated 3D Objects

**Advanced testing** - Include skeletal animations

**Examples**:
- 🦴 **Animated Ears**: Floppy dog ears with physics
- 💫 **Particle Effects**: With 3D geometry
- 🦋 **Flying Butterflies**: Animated wing movement

---

## 🛠️ How to Test a New Lens

### Step 1: Extract Lens Files

If you have a Snapchat lens file (`.zip` or `.lns`):

```bash
# Extract the archive
unzip lens_file.zip -d test_lens

# Or for .lns (Lens Studio format)
unzip lens_file.lns -d test_lens

# Look for 3D models
find test_lens -type f \( -name "*.fbx" -o -name "*.glb" -o -name "*.gltf" -o -name "*.obj" -o -name "*.dae" \)
```

### Step 2: Test with Assimp Tool

```bash
cd /home/padletut/segmecam

# Test the 3D model
./bazel-bin/mediapipe/examples/desktop/segmecam/test_snap_lens_load test_lens/model.fbx

# Expected output:
# ✅ Assimp successfully loaded the file!
# === Scene Information ===
# Meshes: X
# Materials: Y
# Textures: Z (or embedded)
# ...
# Compatibility Score: XX/100
```

### Step 3: Analyze Results

**High Compatibility (70-100%)**:
```
✅ Has 3D meshes
✅ Has materials
✅ Has textures (embedded or referenced)
✅ Normals and UVs present
```
→ **Ready to import!** Proceed to Step 4.

**Medium Compatibility (40-69%)**:
```
✅ Has 3D meshes
✅ Has materials
⚠️  Missing textures OR
⚠️  Missing UVs OR
⚠️  Complex shader materials
```
→ **Needs adaptation** - May need texture extraction or material conversion.

**Low Compatibility (<40%)**:
```
⚠️  Custom format
⚠️  Encrypted assets
⚠️  Proprietary shaders
```
→ **Skip or manual conversion** required.

### Step 4: Create SegmeCam Filter

If compatibility is good, create a SegmeCam filter:

```bash
# Create filter directory
mkdir -p assets/filters/test-lens

# Copy 3D model and textures
cp test_lens/model.fbx assets/filters/test-lens/
cp test_lens/textures/* assets/filters/test-lens/

# Create filter.json
cat > assets/filters/test-lens/filter.json << 'EOF'
{
  "id": "test-lens",
  "name": "Test Snapchat Lens",
  "description": "Imported from Snapchat",
  "author": "Snapchat + SegmeCam",
  "version": "1.0.0",
  "category": "Face",
  "thumbnail_path": "filters/test-lens/thumbnail.png",
  "enabled": true,
  
  "models": [
    {
      "path": "filters/test-lens/model.fbx",
      "anchor": "nose_tip",
      "scale": 1.0,
      "position": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0]
    }
  ],
  
  "textures": [],
  "beauty_effects": {},
  "background_effects": {}
}
EOF
```

### Step 5: Test in SegmeCam

```bash
# Build and run
./build-app.sh
./segmecam

# In the app:
# 1. Open AR Filters panel
# 2. Find "Test Snapchat Lens"
# 3. Click to activate
# 4. Verify 3D model appears on face
```

---

## 📊 Expected Compatibility by Format

| Format | Assimp Support | SegmeCam Ready? | Expected Score |
|--------|----------------|-----------------|----------------|
| **FBX** | ✅ Excellent | ✅ Yes (Phase 8) | 85-95% |
| **GLB/GLTF** | ✅ Excellent | ✅ Yes (Phase 8) | 85-95% |
| **OBJ** | ✅ Perfect | ✅ Yes (Phase 8) | 80-90% |
| **DAE (COLLADA)** | ✅ Good | ✅ Yes (Phase 8) | 75-85% |
| **3DS** | ✅ Good | ✅ Yes (Phase 8) | 70-80% |
| **SceneKit (.scn)** | ❌ Not Supported | ❌ No | 0-10% |
| **USD/USDZ** | ❌ Not in Assimp | ❌ No | 0-10% |
| **Custom/Encrypted** | ❌ No | ❌ No | 0% |

---

## 🎯 Best Test Candidates

### Recommended Downloads

1. **Lens Studio Dog Filter Template**
   - Download Lens Studio (free)
   - File → New Project → Templates → "Puppy"
   - Export assets
   - Test FBX models

2. **Sketchfab Free Models**
   - Search: "cartoon dog ears low poly"
   - Filter: Free downloads, GLB format
   - Download and test
   - Create custom filter

3. **TurboSquid Free Section**
   - Search: "glasses 3d model free"
   - Filter: OBJ or FBX
   - Download and test

### Creating Your Own Test Lens

**Quick Method**:
```bash
# Using Blender (free)
1. Create simple 3D object (cube, sphere, etc.)
2. Model dog ears or glasses
3. UV unwrap
4. Export as FBX or OBJ
5. Test with SegmeCam
```

---

## 🔧 Troubleshooting

### Issue: Assimp fails to load

**Check**:
```bash
# Verify file format
file model_file

# Try loading with verbose output
./test_snap_lens_load model_file 2>&1 | grep -i error

# List supported formats
./test_snap_lens_load 2>&1 | grep "Supported"
```

**Solutions**:
- Convert format (Blender: Import → Export as FBX/OBJ)
- Check for corruption: `hexdump -C model_file | head`
- Try different export settings

### Issue: Low compatibility score

**Common Causes**:
- Missing textures → Copy texture files
- Missing UVs → Re-export with UVs
- Complex materials → Simplify in Blender
- Animations → SegmeCam doesn't support yet

### Issue: Model loads but looks wrong in SegmeCam

**Check**:
- Scale: Adjust in `filter.json`
- Position: Adjust anchor point
- Rotation: Fix up-axis (Y-up vs Z-up)
- Textures: Verify paths in `filter.json`

---

## 📈 Success Metrics

After testing 5-10 lenses, you should see:

**Assimp Loading**:
- ✅ 80-90% of FBX/GLB/GLTF files load successfully
- ✅ 90-100% of OBJ files load successfully
- ❌ 0-10% of SceneKit files load (expected)

**SegmeCam Integration**:
- ✅ Loaded models render in app
- ✅ Face tracking works with 3D objects
- ✅ Textures apply correctly
- ⚠️ May need manual positioning/scaling

**Performance**:
- ✅ GPU rendering: <1ms per frame (Phase 8 target)
- ✅ 60 FPS with 3D models
- ✅ No UI freezing

---

## 📝 Documentation

After testing each lens, document:

1. **Lens Name**: Dog filter, glasses, etc.
2. **Source**: Lens Studio, Sketchfat, etc.
3. **Format**: FBX, GLB, OBJ
4. **File Size**: In KB/MB
5. **Assimp Score**: X/100
6. **SegmeCam Result**: Working / Needs fixes / Not compatible
7. **Issues**: Any problems encountered
8. **Screenshots**: Before/after comparison

Create test report:
```bash
cat > test_results.md << 'EOF'
# 3D Lens Test Results

## Lens 1: Dog Ears
- Source: Lens Studio template
- Format: FBX
- Assimp Score: 85/100
- SegmeCam: ✅ Working perfectly
- Notes: Needed scale adjustment (0.5)

## Lens 2: ...
EOF
```

---

## 🎓 Key Learnings

From testing existing filters:

1. **OBJ is simplest**: Best for first tests
2. **Textures matter**: +15 compatibility points
3. **Animations**: Not yet supported (Phase 10?)
4. **Scale varies**: Always test and adjust
5. **Face anchors**: Critical for proper placement

---

## 🚀 Next Steps

1. **Download Lens Studio** (30 min)
   - Install from https://ar.snap.com/lens-studio
   - Open sample projects
   - Export 3D assets

2. **Test 3 sample lenses** (30 min)
   - Dog filter (FBX)
   - Glasses (OBJ)
   - Cat ears (GLB if available)

3. **Document results** (15 min)
   - Compatibility scores
   - SegmeCam integration notes
   - Screenshots

4. **Create comprehensive report** (15 min)
   - Success rate
   - Common issues
   - Recommendations

**Total Time**: 1.5-2 hours
**Expected Outcome**: Clear understanding of 3D lens compatibility

---

**Status**: 📝 Guide Complete  
**Ready**: ✅ Test tool built and validated  
**Waiting**: 🔽 Download test lenses to begin testing

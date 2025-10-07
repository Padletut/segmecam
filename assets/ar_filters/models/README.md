# AR Filters 3D Models

This directory contains 3D models for testing the AR filter system (Phase 3).

## Available Models

### 1. Simple Glasses (`simple_glasses.obj` + `simple_glasses.mtl`)

**Description**: Basic sunglasses model with two cylindrical lenses and a connecting bridge.

**Specifications**:
- **Vertices**: 36
- **Triangles**: ~48
- **Materials**: 2 (lens_material, bridge_material)
- **Anchor Point**: `nose_bridge`
- **Size**: Left lens centered at (-0.04, 0, 0), right lens at (0.04, 0, 0)
- **Radius**: 0.025 units per lens
- **Features**:
  - Dark glass material with 60% transparency (d 0.6)
  - High specular reflection (Ks 0.9) for realistic glass appearance
  - Bridge connects lenses at nose bridge position

**Material Properties**:
- `lens_material`:
  - Ambient: (0.1, 0.1, 0.1) - Dark
  - Diffuse: (0.2, 0.2, 0.2) - Dark gray
  - Specular: (0.9, 0.9, 0.9) - Very shiny
  - Shininess: 100.0 - High gloss
  - Transparency: 0.6 (60% opaque, 40% transparent)

- `bridge_material`:
  - Ambient: (0.05, 0.05, 0.05) - Very dark
  - Diffuse: (0.1, 0.1, 0.1) - Black
  - Specular: (0.5, 0.5, 0.5) - Moderate shine
  - Shininess: 50.0 - Medium gloss
  - Transparency: 1.0 (100% opaque)

**Usage Example**:
```cpp
auto glasses = CreateFromModel("nose_bridge", "Test Glasses",
                               "assets/ar_filters/models/simple_glasses.obj");
```

---

### 2. Simple Top Hat (`simple_hat.obj` + `simple_hat.mtl`)

**Description**: Classic top hat with cylindrical body and flat brim.

**Specifications**:
- **Vertices**: 34
- **Triangles**: ~56
- **Materials**: 2 (hat_body_material, brim_material)
- **Anchor Point**: `forehead` or `top_of_head`
- **Size**: 
  - Body: radius 0.03, height 0.08 (8cm tall)
  - Brim: inner radius 0.03, outer radius 0.05
- **Position**: Body starts at y=0.01, extends to y=0.09
- **Features**:
  - Black shiny material (top hat appearance)
  - Separate materials for body and brim
  - High specular reflection for silk-like finish

**Material Properties**:
- `hat_body_material`:
  - Ambient: (0.05, 0.05, 0.05) - Very dark
  - Diffuse: (0.1, 0.1, 0.1) - Black
  - Specular: (0.8, 0.8, 0.8) - Very shiny
  - Shininess: 80.0 - High gloss (silk-like)
  - Transparency: 1.0 (100% opaque)

- `brim_material`:
  - Ambient: (0.05, 0.05, 0.05) - Very dark
  - Diffuse: (0.1, 0.1, 0.1) - Black
  - Specular: (0.7, 0.7, 0.7) - Shiny
  - Shininess: 60.0 - Medium-high gloss
  - Transparency: 1.0 (100% opaque)

**Usage Example**:
```cpp
auto hat = CreateFromModel("forehead", "Top Hat",
                          "assets/ar_filters/models/simple_hat.obj");
hat.offset = cv::Vec3f(0, 0.05, 0);  // Position above head
```

---

### 3. Simple Cube (`simple_cube.obj` + `simple_cube.mtl`)

**Description**: Basic 8-vertex cube for testing and debugging.

**Specifications**:
- **Vertices**: 8
- **Triangles**: 12 (2 per face)
- **Materials**: 1 (cube_material)
- **Anchor Point**: Any (good for testing any anchor point)
- **Size**: 0.05 × 0.05 × 0.05 units (5cm cube)
- **Position**: Centered at origin
- **Features**:
  - Minimal geometry (fastest to load)
  - Red diffuse color for high visibility
  - Perfect for debugging anchor point positioning

**Material Properties**:
- `cube_material`:
  - Ambient: (0.2, 0.2, 0.2) - Gray
  - Diffuse: (0.8, 0.2, 0.2) - Red
  - Specular: (0.5, 0.5, 0.5) - Moderate shine
  - Shininess: 50.0 - Medium gloss
  - Transparency: 1.0 (100% opaque)

**Usage Example**:
```cpp
auto cube = CreateFromModel("nose_bridge", "Debug Cube",
                           "assets/ar_filters/models/simple_cube.obj");
// Useful for visualizing anchor points!
```

---

## Coordinate System

All models use the **OpenGL coordinate system**:
- **+X**: Right
- **+Y**: Up
- **+Z**: Forward (toward camera)

Models are designed to be positioned at anchor points on a face mesh.

---

## Material System

### OBJ/MTL Material Properties

Models use Wavefront OBJ/MTL material definitions:

- **Ka (Ambient)**: Color under ambient lighting
- **Kd (Diffuse)**: Main surface color under direct lighting
- **Ks (Specular)**: Highlight color (shininess)
- **Ns (Shininess)**: Specular exponent (0-1000, higher = glossier)
- **d (Dissolve)**: Opacity (0.0 = fully transparent, 1.0 = fully opaque)
  - Also written as `Tr` (Transparency) in some files

### Material Rendering

When rendering with OpenGL:
```cpp
glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat.ambient.val);
glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat.diffuse.val);
glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat.specular.val);
glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat.shininess);
```

---

## Adding New Models

### Option 1: Export from Blender

1. **Create Model**: Design your filter in Blender
2. **Scale**: Use real-world units (meters). Face is ~0.2m wide
3. **Origin**: Set origin to attachment point
4. **Export Settings**:
   - Format: Wavefront (.obj)
   - ✅ Include Materials
   - ✅ Triangulate Faces
   - ✅ Write Normals
   - ✅ Write UVs (if using textures)
   - Path Mode: Copy (embeds materials)
5. **Save**: Export to `assets/ar_filters/models/`

### Option 2: Hand-Write OBJ (Simple Models)

For basic shapes (cubes, cylinders), you can write OBJ files manually:

**Structure**:
```obj
# Comments start with #
mtllib your_model.mtl   # Reference material file

v x y z                 # Vertex position
vn x y z                # Vertex normal
vt u v                  # Texture coordinate

usemtl material_name    # Activate material
f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3  # Triangle face
```

**Tips**:
- Vertices are 1-indexed (first vertex is 1, not 0)
- Faces can be quads (4 vertices) - Assimp auto-triangulates
- Use `usemtl` before faces to assign materials
- Normals are optional (Assimp generates if missing)

### Option 3: Convert from Other Formats

Use Assimp command-line tool to convert:
```bash
assimp export input.fbx output.obj
assimp export input.gltf output.obj
```

---

## Testing Models

### Quick Test

```cpp
// Test loading
ar_filters::ModelLoader loader;
auto model_or = loader.LoadModel("assets/ar_filters/models/simple_cube.obj");
if (model_or.ok()) {
    LOG(INFO) << "Model loaded successfully!";
    LOG(INFO) << "Meshes: " << model_or->meshes.size();
    LOG(INFO) << "Vertices: " << model_or->vertex_count;
    LOG(INFO) << "Triangles: " << model_or->triangle_count;
    LOG(INFO) << "Materials: " << model_or->materials.size();
}
```

### Integration Test

```cpp
// Test in AR system
auto filter = CreateFromModel("nose_bridge", "Test",
                             "assets/ar_filters/models/simple_glasses.obj");
attachment_controller.AttachFilter(filter);
// Should appear on face at nose bridge position
```

### Validation Checklist

- ✅ File loads without errors
- ✅ Meshes have > 0 vertices
- ✅ Materials are defined
- ✅ Model renders at correct position
- ✅ Scale is appropriate for face (not too big/small)
- ✅ Orientation is correct (facing forward)
- ✅ Materials look correct (colors, transparency)

---

## Model Guidelines

### Scale Guidelines

Human face dimensions (approximate):
- **Face width**: 0.14-0.18m (14-18cm)
- **Face height**: 0.18-0.24m (18-24cm)
- **Nose width**: 0.035-0.045m (3.5-4.5cm)
- **Eye spacing**: 0.06-0.07m (6-7cm)

Design models with these dimensions in mind:
- **Glasses**: Lens radius ~0.025m (2.5cm), spacing ~0.08m
- **Hat**: Radius ~0.08-0.10m (8-10cm) for adult head
- **Mask**: Cover face area ~0.15m × 0.20m

### Performance Guidelines

Target specifications for real-time AR:
- **Vertices**: < 1000 per model (preferably < 500)
- **Triangles**: < 2000 per model (preferably < 1000)
- **Materials**: 1-3 materials per model
- **Textures**: 512×512 or 1024×1024 (Phase 4+)

### Best Practices

1. **Geometry**:
   - Keep polygon count low
   - Use triangles or quads only (no n-gons)
   - Merge duplicate vertices
   - Remove internal/hidden faces

2. **Materials**:
   - Use descriptive material names
   - Test materials without textures first
   - Set transparency < 1.0 for see-through objects
   - Use high shininess (80-100) for glass/metal

3. **Orientation**:
   - Model should face +Z (toward camera)
   - Up should be +Y
   - Right should be +X

4. **Origin**:
   - Set origin to attachment point
   - For glasses: origin at nose bridge
   - For hat: origin at top of head
   - For mask: origin at face center

---

## Format Support

Via Assimp, we support 50+ formats:

### Most Common
- **OBJ** (.obj) - Universal, human-readable ✅
- **GLTF** (.gltf, .glb) - Modern, PBR materials ✅
- **FBX** (.fbx) - Autodesk, animations ✅
- **STL** (.stl) - 3D printing, simple geometry ✅
- **Collada** (.dae) - Game engines, animations ✅

### Also Supported
- 3DS Max (.3ds)
- Blender (.blend)
- X3D (.x3d)
- IQM (.iqm)
- MD2/MD3/MD5 (Quake/Doom models)
- And 40+ more!

See [Assimp documentation](https://github.com/assimp/assimp) for full list.

---

## Troubleshooting

### Model Doesn't Load

Check:
1. File path is correct (absolute or relative to binary)
2. MTL file is in same directory as OBJ
3. File permissions allow reading
4. OBJ syntax is valid (use validator tool)

### Model Renders Black

Check:
1. Materials are defined in MTL file
2. `usemtl` is before face definitions in OBJ
3. Lighting is enabled in renderer
4. Normals are present (or enable auto-generation)

### Model Too Big/Small

Fix:
1. Scale in Blender before export
2. Or use `filter.scale` to adjust:
   ```cpp
   filter.scale = cv::Vec3f(2.0f, 2.0f, 2.0f);  // 2x bigger
   ```

### Model Wrong Orientation

Fix:
1. Rotate in Blender before export
2. Or use `filter.local_rotation_euler`:
   ```cpp
   filter.local_rotation_euler = cv::Vec3f(0, 90, 0);  // Rotate 90° around Y
   ```

### Materials Look Wrong

Check:
1. Kd (diffuse) values are 0-1 range
2. Ks (specular) + high Ns for shiny materials
3. d (dissolve) < 1.0 for transparency
4. Alpha blending enabled in renderer

---

## Future Enhancements (Phase 4+)

- **Textures**: Load PNG/JPEG textures from MTL files
- **Normal Maps**: For detailed lighting without extra geometry
- **PBR Materials**: Physically-based rendering (metallic, roughness)
- **Animations**: Skeletal animation support (GLTF, FBX)
- **Model Caching**: Avoid reloading same models
- **LOD**: Level-of-detail for performance
- **Asset Bundles**: Package multiple models together

---

## License

These test models are provided under the Apache 2.0 license for testing purposes only.

For production filters, ensure you have proper rights to any 3D models you use.

---

**Last Updated**: October 2, 2025
**Phase**: 3 - Step 4
**Status**: Test Assets Created ✅

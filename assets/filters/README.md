# AR Filter Assets - Documentation

## Overview

SegmeCam AR filters are defined using JSON configuration files that describe 3D models, materials, and behaviors. Each filter is a self-contained directory with all assets needed for rendering.

## Filter Directory Structure

```
assets/filters/
├── classic-glasses/
│   ├── filter.json          # Filter definition (required)
│   ├── thumbnail.png         # Preview thumbnail 256x256 (required)
│   ├── icon.png             # Category icon 64x64 (required)
│   ├── models/              # 3D model files (.obj)
│   └── textures/            # Texture images (.png, .jpg)
├── party-hat/
│   └── ...
└── cat-ears/
    └── ...
```

## Filter JSON Schema

### Required Sections

#### 1. Filter Metadata

```json
{
  "filter": {
    "name": "Display Name",           // Human-readable name
    "id": "unique-filter-id-v1",      // Unique identifier
    "category": "accessories",         // Category: accessories, fun, beauty, etc.
    "version": "1.0.0",               // Semantic version
    "author": "Creator Name",          // Author/creator
    "description": "Short description", // User-facing description
    "thumbnail": "thumbnail.png",      // Preview image path
    "icon": "icon.png"                // Category icon path
  }
}
```

**Required fields:** name, id, version  
**Optional fields:** category, author, description, thumbnail, icon

#### 2. Attachments

Attachments define 3D models to render at face anchor points.

```json
{
  "attachments": [
    {
      "id": "unique_attachment_id",      // Unique ID within filter
      "anchor": "nose_bridge",            // Face anchor point (see below)
      "model": "models/object.obj",       // Path to 3D model file
      "texture": "textures/tex.png",      // Path to texture image
      "scale": [1.0, 1.0, 1.0],          // XYZ scale multipliers
      "offset": [0.0, 0.0, 0.0],         // XYZ position offset (meters)
      "rotation": [0.0, 0.0, 0.0],       // XYZ Euler angles (degrees)
      "visible": true,                    // Initial visibility
      "opacity": 1.0                      // Transparency (0.0-1.0)
    }
  ]
}
```

**Required fields:** id, anchor, model  
**Optional fields:** texture, scale, offset, rotation, visible, opacity

**Available Face Anchors:**
- `nose_bridge` - Center of nose bridge
- `left_ear` - Left ear position
- `right_ear` - Right ear position
- `forehead` - Center of forehead
- `chin` - Chin position
- `left_eye` - Left eye center
- `right_eye` - Right eye center
- `mouth_center` - Center of mouth
- `face_center` - Center of face

### Optional Sections

#### 3. Materials

Materials define OpenGL rendering properties for attachments.

```json
{
  "materials": {
    "attachment_id": {
      "ambient": [0.2, 0.2, 0.2],      // Ambient color RGB (0.0-1.0)
      "diffuse": [0.8, 0.8, 0.8],      // Diffuse color RGB (0.0-1.0)
      "specular": [1.0, 1.0, 1.0],     // Specular color RGB (0.0-1.0)
      "shininess": 32.0,                // Specular exponent (1-128)
      "opacity": 1.0                    // Material transparency (0.0-1.0)
    }
  }
}
```

**Default values:**
- ambient: [0.2, 0.2, 0.2]
- diffuse: [0.8, 0.8, 0.8]
- specular: [0.5, 0.5, 0.5]
- shininess: 32.0
- opacity: 1.0

#### 4. Behaviors

Behaviors define how attachments respond to facial expressions (blendshapes).

```json
{
  "behaviors": {
    "behavior_id": {
      "type": "shake",                   // Behavior type (see below)
      "target": "attachment_id",         // Target attachment ID
      "blendshape": "mouthSmile",       // ARKit blendshape name
      "threshold": 0.5,                  // Activation threshold (0.0-1.0)
      "intensity": 1.0,                  // Effect intensity multiplier
      "gravity": 9.8,                    // Gravity (fall_off only, m/s²)
      "target_scale": [1.2, 1.2, 1.2],  // Target scale (scale only)
      "target_rotation": [0, 0, 10],    // Target rotation (rotate only)
      "target_color": [1.0, 0.5, 0.5]   // Target color (color_change only)
    }
  }
}
```

**Behavior Types:**
- `shake` - Shake/wobble effect based on head movement
- `fall_off` - Physics-based falling when threshold exceeded
- `scale` - Scale object when blendshape activated
- `rotate` - Rotate object when blendshape activated
- `hide` - Hide/show object based on blendshape
- `color_change` - Change color based on blendshape

**Common Blendshapes:**
- `mouthSmile` - Smile intensity (0.0-1.0)
- `eyeBlink` - Eye blink (0.0-1.0 per eye)
- `jawOpen` - Mouth open amount (0.0-1.0)
- `browInnerUp` - Eyebrow raise (0.0-1.0)
- `headRotate` - Head rotation angle
- `headTilt` - Head tilt angle
- See ARKit blendshape documentation for full list

## Example Filters

### Classic Glasses (Simple)

Single attachment with material definition, no behaviors.

```json
{
  "filter": {
    "name": "Classic Glasses",
    "id": "classic-glasses-v1",
    "version": "1.0.0"
  },
  "attachments": [
    {
      "id": "glasses_frame",
      "anchor": "nose_bridge",
      "model": "models/glasses_frame.obj",
      "texture": "textures/glasses_black.png",
      "scale": [1.0, 1.0, 1.0],
      "offset": [0.0, 0.0, 0.02]
    }
  ],
  "materials": {
    "glasses_frame": {
      "ambient": [0.1, 0.1, 0.1],
      "diffuse": [0.2, 0.2, 0.2],
      "specular": [0.8, 0.8, 0.8],
      "shininess": 128.0
    }
  }
}
```

### Party Hat (With Physics)

Attachment with physics-based behaviors.

```json
{
  "filter": {
    "name": "Party Hat",
    "id": "party-hat-v1",
    "version": "1.0.0"
  },
  "attachments": [
    {
      "id": "party_hat_main",
      "anchor": "forehead",
      "model": "models/party_hat.obj",
      "texture": "textures/party_colors.png",
      "offset": [0.0, 0.05, -0.03],
      "rotation": [10.0, 0.0, 0.0]
    }
  ],
  "behaviors": {
    "head_shake_tilt": {
      "type": "shake",
      "target": "party_hat_main",
      "blendshape": "headRotate",
      "threshold": 15.0,
      "intensity": 0.8
    },
    "head_tilt_physics": {
      "type": "fall_off",
      "target": "party_hat_main",
      "blendshape": "headTilt",
      "threshold": 30.0,
      "gravity": 9.8
    }
  }
}
```

### Cat Ears (Multi-Attachment)

Multiple attachments with coordinated behaviors.

```json
{
  "filter": {
    "name": "Cat Ears",
    "id": "cat-ears-v1",
    "version": "1.0.0"
  },
  "attachments": [
    {
      "id": "left_ear",
      "anchor": "left_ear",
      "model": "models/cat_ear.obj",
      "texture": "textures/cat_fur_pink.png",
      "offset": [-0.02, 0.08, -0.01],
      "rotation": [0.0, 0.0, -15.0]
    },
    {
      "id": "right_ear",
      "anchor": "right_ear",
      "model": "models/cat_ear.obj",
      "texture": "textures/cat_fur_pink.png",
      "offset": [0.02, 0.08, -0.01],
      "rotation": [0.0, 0.0, 15.0]
    }
  ],
  "behaviors": {
    "ear_wiggle_smile": {
      "type": "scale",
      "target": "left_ear",
      "blendshape": "mouthSmile",
      "threshold": 0.5,
      "intensity": 0.3,
      "target_scale": [1.1, 1.1, 1.1]
    },
    "ear_wiggle_smile_right": {
      "type": "scale",
      "target": "right_ear",
      "blendshape": "mouthSmile",
      "threshold": 0.5,
      "intensity": 0.3,
      "target_scale": [1.1, 1.1, 1.1]
    }
  }
}
```

## Usage in Code

### Loading a Filter

```cpp
#include "ar_filters/filter_asset.h"

using namespace segmecam::ar_filters;

// Load from directory
auto filter = FilterAsset::LoadFromDirectory("assets/filters/classic-glasses");
if (!filter.ok()) {
  ABSL_LOG(ERROR) << "Failed to load filter: " << filter.status();
  return;
}

// Access metadata
const FilterMetadata& metadata = filter->GetMetadata();
ABSL_LOG(INFO) << "Loaded filter: " << metadata.name;

// Access attachments
const auto& attachments = filter->GetAttachments();
for (const auto& attachment : attachments) {
  ABSL_LOG(INFO) << "Attachment: " << attachment.id 
                 << " at anchor: " << attachment.anchor_name;
}
```

### Enumerating Available Filters

```cpp
// Find all filters in directory
std::vector<std::string> filter_dirs = 
    FilterAsset::EnumerateFilters("assets/filters");

for (const auto& dir : filter_dirs) {
  auto filter = FilterAsset::LoadFromDirectory(dir);
  if (filter.ok()) {
    ABSL_LOG(INFO) << "Found: " << filter->GetMetadata().name;
  }
}
```

### Validating Assets

```cpp
auto filter = FilterAsset::LoadFromDirectory("assets/filters/my-filter");
if (filter.ok()) {
  // Check that all referenced files exist
  absl::Status validation = filter->ValidateAssets();
  if (!validation.ok()) {
    ABSL_LOG(ERROR) << "Missing assets: " << validation;
  }
}
```

## Creating Custom Filters

### 1. Create Directory Structure

```bash
mkdir -p assets/filters/my-filter/models
mkdir -p assets/filters/my-filter/textures
```

### 2. Add 3D Models

- Export models as Wavefront OBJ files
- Keep polygon count reasonable (< 10,000 triangles)
- Use relative paths in filter.json

### 3. Add Textures

- PNG or JPG format
- Power-of-2 dimensions (512x512, 1024x1024, etc.)
- Use relative paths in filter.json

### 4. Create filter.json

Start with minimal configuration:

```json
{
  "filter": {
    "name": "My Filter",
    "id": "my-filter-v1",
    "version": "1.0.0"
  },
  "attachments": [
    {
      "id": "main_object",
      "anchor": "nose_bridge",
      "model": "models/my_model.obj"
    }
  ]
}
```

### 5. Test Loading

```cpp
auto filter = FilterAsset::LoadFromDirectory("assets/filters/my-filter");
ABSL_CHECK(filter.ok()) << filter.status();
ABSL_CHECK(filter->ValidateAssets().ok()) << "Missing files";
```

### 6. Add Materials and Behaviors

Refine appearance and interactivity:

```json
{
  "materials": {
    "main_object": {
      "ambient": [0.3, 0.3, 0.3],
      "diffuse": [0.9, 0.9, 0.9],
      "specular": [1.0, 1.0, 1.0],
      "shininess": 64.0
    }
  },
  "behaviors": {
    "smile_scale": {
      "type": "scale",
      "target": "main_object",
      "blendshape": "mouthSmile",
      "threshold": 0.5,
      "intensity": 0.5,
      "target_scale": [1.2, 1.2, 1.2]
    }
  }
}
```

## Best Practices

### Performance

- Keep total polygon count under 50,000 per filter
- Use texture atlases to reduce draw calls
- Optimize texture sizes (1024x1024 max recommended)
- Limit behaviors to 5-10 per filter

### Visual Quality

- Test filters with different face shapes and skin tones
- Use appropriate shininess values (16-128)
- Balance ambient/diffuse/specular for realistic lighting
- Set appropriate opacity for translucent objects

### Behaviors

- Use threshold values appropriate for blendshape ranges
- Test intensity values (0.5-1.0 usually works well)
- Combine multiple behaviors for richer interactions
- Avoid conflicting behaviors on same attachment

### Debugging

- Use `ValidateAssets()` to check file paths
- Enable logging to see parsing errors
- Test with simple filters first
- Add behaviors incrementally

## Version History

- 1.0.0 (2025-01-03) - Initial filter asset system with JSON schema
- Phase 6 Day 1 - FilterAsset class implementation complete

## See Also

- FilterAsset API reference in `filter_asset.h`
- ARRenderer integration guide (Phase 6 Day 2)
- 3D model export guidelines
- Texture optimization guide

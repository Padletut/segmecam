# Face Mesh Deformation for AR Filters - Implementation Plan

## Problem Statement

Currently, AR filters (like the Holo Visor) are rendered as **rigid 3D objects** attached to single anchor points. They don't **conform to face curvature**, making them look flat and unrealistic.

**Example**: A visor should curve around the forehead/temples, not be a flat plane.

## Current vs. Desired Behavior

### Current (Rigid Attachment)
```
┌─────────────┐
│   VISOR     │  ← Flat, single anchor at forehead
│   (FLAT)    │  ← Doesn't follow face curvature
└─────────────┘
    👤 Face
```

### Desired (Mesh-Conforming)
```
   ╭─────────╮
  ╱   VISOR   ╲  ← Curves around face
 │  (CURVED)   │ ← Follows face mesh landmarks
  ╲___________╱
      👤 Face
```

## Available Data

We already have **MediaPipe Face Mesh** with 478 3D landmarks:
- Forehead region: Landmarks 10, 338, 297, 332, 284, 251, 389, 356, 454
- Temple regions: Landmarks 127, 234 (left), 356, 454 (right)
- Face curvature: Full 478-point mesh

**Location**: `face_mesh_processor.cpp` - Already extracting all landmarks!

## Solution: Vertex Displacement System

### Architecture Overview

```
┌──────────────────────┐
│  Face Mesh (478pts)  │ MediaPipe output
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ Landmark → UV Mapper │ Map face regions to UV space
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│   Displacement Map   │ GPU texture: face depth/curvature
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│   Vertex Shader      │ Displace vertices to match face
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Deformed Filter     │ Final curved mesh
└──────────────────────┘
```

## Implementation Options

### Option 1: GPU Shader Displacement (Recommended) ⭐

**Approach**: Modify vertex shader to displace vertices based on face landmarks

**Advantages**:
- ✅ Real-time performance (GPU-accelerated)
- ✅ Smooth deformation
- ✅ No CPU overhead
- ✅ Scalable to complex meshes

**Disadvantages**:
- ⚠️ Shader complexity
- ⚠️ Requires landmark data upload to GPU
- ⚠️ More sophisticated than current system

**Estimated Time**: 6-8 hours

---

### Option 2: CPU Mesh Deformation

**Approach**: Deform mesh vertices on CPU before uploading to GPU

**Advantages**:
- ✅ Easier to debug
- ✅ Full control over deformation
- ✅ No shader changes needed

**Disadvantages**:
- ❌ CPU overhead (~5-10ms per frame)
- ❌ Memory bandwidth (upload deformed mesh every frame)
- ❌ Not scalable to complex meshes

**Estimated Time**: 4-6 hours

---

### Option 3: Multi-Anchor Approximation (Quick Fix)

**Approach**: Use multiple anchor points (forehead, left temple, right temple) with separate model pieces

**Advantages**:
- ✅ No shader changes
- ✅ Fast to implement
- ✅ Works with current system

**Disadvantages**:
- ⚠️ Only approximates curvature
- ⚠️ Requires splitting visor model into 3 pieces
- ⚠️ Not true mesh deformation

**Estimated Time**: 2-3 hours

---

## Recommended Implementation: Option 1 (GPU Shader)

### Phase 1: Face Mesh to Texture (2 hours)

**Goal**: Convert face mesh landmarks to a GPU texture for shader access

**Files to Create**:
- `include/ar_filters/face_mesh_texture.h`
- `src/ar_filters/face_mesh_texture.cpp`

**Key Features**:
```cpp
class FaceMeshTexture {
public:
    // Upload 478 landmarks to GPU texture (478x1 RGB32F texture)
    void UpdateFromLandmarks(const std::vector<cv::Point3f>& landmarks);
    
    // Bind texture to shader uniform
    void BindToShader(render::ShaderProgram* shader, int texture_unit);
    
    // Get texture ID for direct GPU access
    GLuint GetTextureID() const;

private:
    GLuint texture_id_;
    int width_ = 478;   // One texel per landmark
    int height_ = 1;
    std::vector<float> landmark_data_;  // Packed XYZ data
};
```

**Implementation Steps**:
1. Create 478x1 RGB32F texture (stores X,Y,Z in RGB channels)
2. Pack face landmarks into texture data (478 × 3 floats)
3. Upload to GPU with `glTexImage2D`
4. Update every frame when landmarks change

---

### Phase 2: Shader Displacement (3 hours)

**Goal**: Modify vertex shader to read face mesh and displace vertices

**Files to Modify**:
- `shaders/model_vertex.glsl` (add displacement logic)
- `shaders/model_vertex_deform.glsl` (NEW - specialized for face-conforming filters)

**New Vertex Shader**: `model_vertex_deform.glsl`
```glsl
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

// NEW: Face mesh data
uniform sampler2D uFaceMeshTexture;  // 478x1 texture with landmark XYZ
uniform int uFaceMeshEnabled;        // Enable/disable deformation

// NEW: Deformation parameters
uniform float uDeformStrength;       // 0.0 = no deform, 1.0 = full conform
uniform vec3 uAnchorLandmark;        // Base anchor position (e.g., forehead)

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

// Map vertex position to nearest face landmark
vec3 GetFaceDisplacement(vec3 vertexPos) {
    // Use vertex Y position to determine which face region
    // (forehead = high Y, chin = low Y)
    float normalizedY = (vertexPos.y + 1.0) * 0.5;  // Map [-1,1] to [0,1]
    
    // Map to landmark index range
    // Forehead region: landmarks 10, 67, 109, 10, etc.
    // We'll use a simplified mapping for now
    
    // Sample the face mesh texture
    // For simplicity, use normalizedY to index into landmark array
    int landmarkIndex = int(normalizedY * 477.0);  // 0-477
    float texU = float(landmarkIndex) / 478.0;
    
    // Read landmark position from texture
    vec3 faceLandmark = texture(uFaceMeshTexture, vec2(texU, 0.5)).xyz;
    
    // Calculate displacement vector (from rigid position to face position)
    vec3 rigidPos = (uModel * vec4(vertexPos, 1.0)).xyz;
    vec3 displacement = faceLandmark - uAnchorLandmark;
    
    return displacement * uDeformStrength;
}

void main() {
    vec3 displacedPos = aPos;
    
    // Apply face mesh deformation if enabled
    if (uFaceMeshEnabled == 1) {
        vec3 displacement = GetFaceDisplacement(aPos);
        displacedPos += displacement;
    }
    
    // Transform to world space
    vec4 worldPos = uModel * vec4(displacedPos, 1.0);
    FragPos = worldPos.xyz;
    
    // Transform normal
    Normal = normalize(uNormalMatrix * aNormal);
    
    TexCoord = aTexCoord;
    
    // Final position
    gl_Position = uProjection * uView * worldPos;
}
```

**Shader Uniform Setup** (in `ar_renderer.cpp`):
```cpp
// Upload face mesh to texture
face_mesh_texture_->UpdateFromLandmarks(face_landmarks_3d_);
face_mesh_texture_->BindToShader(shader_.get(), 0);  // Texture unit 0

// Set deformation parameters
shader_->SetInt("uFaceMeshEnabled", instance.enable_face_deform);
shader_->SetFloat("uDeformStrength", instance.deform_strength);
shader_->SetVec3("uAnchorLandmark", anchor_position);
```

---

### Phase 3: Filter Configuration (1 hour)

**Goal**: Add face deformation flags to filter.json

**File**: `assets/filters/pro_holo/filter.json`
```json
{
  "attachments": [{
    "id": "visor",
    "anchor": "forehead",
    "model": "visor.obj",
    
    // NEW: Face mesh deformation settings
    "face_deform_enabled": true,
    "face_deform_strength": 0.8,  // 0.0-1.0
    "face_deform_region": "forehead_temples",  // Which face region to follow
    
    "offset": [0.0, 0.02, 0.08]
  }]
}
```

**Deform Regions**:
- `"forehead_temples"` - Visor, headbands
- `"nose_bridge"` - Glasses bridges
- `"full_face"` - Masks, full-face filters
- `"upper_face"` - Eye regions
- `"lower_face"` - Mouth/chin regions

---

### Phase 4: ARRenderer Integration (2 hours)

**Goal**: Wire up face mesh texture and shader system

**Files to Modify**:
- `include/ar_filters/ar_renderer.h` - Add FaceMeshTexture member
- `src/ar_filters/ar_renderer.cpp` - Initialize and update texture
- `include/ar_filters/filter_asset.h` - Add deform fields to FilterAttachment

**ARRenderer Changes**:
```cpp
// ar_renderer.h
class ARRenderer {
private:
    std::unique_ptr<FaceMeshTexture> face_mesh_texture_;
    std::vector<cv::Point3f> face_landmarks_3d_;
    bool face_mesh_available_ = false;
};

// ar_renderer.cpp
absl:Status ARRenderer::Initialize() {
    // ... existing initialization ...
    
    // NEW: Initialize face mesh texture
    face_mesh_texture_ = std::make_unique<FaceMeshTexture>();
    if (!face_mesh_texture_->Initialize()) {
        return absl::InternalError("Failed to initialize face mesh texture");
    }
}

void ARRenderer::UpdateFaceLandmarks(const std::vector<cv::Point3f>& landmarks) {
    face_landmarks_3d_ = landmarks;
    face_mesh_available_ = !landmarks.empty();
    
    // Upload to GPU texture
    if (face_mesh_available_) {
        face_mesh_texture_->UpdateFromLandmarks(landmarks);
    }
}

absl::Status ARRenderer::RenderModelInstances() {
    // ... existing render setup ...
    
    // NEW: Bind face mesh texture for deformable filters
    if (face_mesh_available_) {
        face_mesh_texture_->BindToShader(shader_.get(), 0);
    }
    
    for (auto& [id, instance] : model_instances_) {
        // NEW: Set per-instance deformation uniforms
        shader_->SetInt("uFaceMeshEnabled", instance.face_deform_enabled);
        shader_->SetFloat("uDeformStrength", instance.deform_strength);
        
        // ... existing render code ...
    }
}
```

---

## Advanced: Proper UV-Based Displacement

For **production-quality** face conforming, we need a more sophisticated approach:

### Barycentric Coordinate System

**Concept**: Map filter vertices to face mesh triangles using barycentric coordinates

```cpp
struct FaceMeshMapping {
    int triangle_idx;     // Which face mesh triangle
    glm::vec3 barycentric; // (u, v, w) coordinates within triangle
};

// For each filter vertex, find nearest face mesh triangle
std::vector<FaceMeshMapping> MapVertexToFaceMesh(
    const std::vector<glm::vec3>& filter_vertices,
    const std::vector<cv::Point3f>& face_landmarks) {
    
    std::vector<FaceMeshMapping> mappings;
    
    for (const auto& vertex : filter_vertices) {
        // Find 3 nearest face landmarks
        auto [idx1, idx2, idx3] = FindNearestTriangle(vertex, face_landmarks);
        
        // Calculate barycentric coordinates
        glm::vec3 bary = CalculateBarycentricCoords(
            vertex,
            face_landmarks[idx1],
            face_landmarks[idx2],
            face_landmarks[idx3]
        );
        
        mappings.push_back({idx1, idx2, idx3, bary});
    }
    
    return mappings;
}

// At render time, interpolate position from face mesh
glm::vec3 GetDeformedPosition(
    const FaceMeshMapping& mapping,
    const std::vector<cv::Point3f>& face_landmarks) {
    
    const glm::vec3& p1 = face_landmarks[mapping.triangle[0]];
    const glm::vec3& p2 = face_landmarks[mapping.triangle[1]];
    const glm::vec3& p3 = face_landmarks[mapping.triangle[2]];
    
    // Interpolate using barycentric coordinates
    return mapping.bary.x * p1 +
           mapping.bary.y * p2 +
           mapping.bary.z * p3;
}
```

**Estimated Time**: Additional 4-6 hours for full barycentric system

---

## Quick Start: Multi-Anchor Approximation (Option 3)

If you want to see face-conforming **today**, here's the quick approach:

### Step 1: Split Visor Model (30 min)

Split `visor.obj` into 3 pieces:
- `visor_left.obj` - Left temple region
- `visor_center.obj` - Forehead/nose bridge region  
- `visor_right.obj` - Right temple region

### Step 2: Update filter.json (15 min)

```json
{
  "attachments": [
    {
      "id": "visor_center",
      "anchor": "forehead",
      "model": "visor_center.obj",
      "offset": [0.0, 0.02, 0.08]
    },
    {
      "id": "visor_left",
      "anchor": "left_ear",
      "model": "visor_left.obj",
      "offset": [-0.05, 0.0, 0.05]
    },
    {
      "id": "visor_right",
      "anchor": "right_ear",
      "model": "visor_right.obj",
      "offset": [0.05, 0.0, 0.05]
    }
  ]
}
```

### Step 3: Test (5 min)

The visor will now **approximate** face curvature by following 3 separate anchor points!

**Result**: Not true mesh deformation, but **much better** than single rigid anchor.

---

## Recommended Roadmap

### Phase 10 (Post-Filter Panel): Face Mesh Deformation ⏳

**Week 1**: Option 1 Implementation (GPU Shader)
- Day 1-2: Face mesh texture system
- Day 3-4: Vertex displacement shader
- Day 5: ARRenderer integration
- Day 6: Testing and refinement

**Week 2**: Advanced Features
- Day 1-2: Barycentric coordinate system
- Day 3-4: Per-region deformation (forehead, temples, etc.)
- Day 5-6: Performance optimization

**Success Criteria**:
- ✅ Visor curves around forehead/temples naturally
- ✅ < 2ms GPU overhead for deformation
- ✅ Works with existing filter system
- ✅ Configurable per-filter (enable/disable)

---

## Alternative: Immediate Workaround

**Want curved visor NOW?** Use Blender to pre-curve the model!

1. Open `visor.obj` in Blender
2. Add **Curve Modifier** or use **Lattice Deformation**
3. Manually curve the visor to match typical face curvature
4. Export as new `visor_curved.obj`
5. Use in filter (no code changes!)

**Trade-off**: Fixed curvature (won't adapt to different faces), but **instant solution**!

---

## Resources

### MediaPipe Face Mesh Topology
- 478 landmarks with known triangulation
- Face regions: https://github.com/google/mediapipe/blob/master/mediapipe/modules/face_geometry/data/canonical_face_model.obj

### Similar Implementations
- Snapchat Lens Studio: Uses "Face Mesh" deformation
- Spark AR (Facebook): "Face Tracker" with vertex displacement
- Unity AR Foundation: Face mesh anchoring

### Shader References
- Learn OpenGL Displacement Mapping: https://learnopengl.com/Advanced-Lighting/Parallax-Mapping
- GPU Gems 3 - Mesh Deformation: https://developer.nvidia.com/gpugems/gpugems3

---

## Summary

| Option | Time | Quality | Performance | Difficulty |
|--------|------|---------|-------------|------------|
| **GPU Shader** | 6-8h | ⭐⭐⭐⭐⭐ | ⚡⚡⚡⚡⚡ | 🧠🧠🧠🧠 |
| **CPU Deform** | 4-6h | ⭐⭐⭐⭐ | ⚡⚡⚡ | 🧠🧠🧠 |
| **Multi-Anchor** | 2-3h | ⭐⭐⭐ | ⚡⚡⚡⚡⚡ | 🧠🧠 |
| **Pre-Curved Model** | 30min | ⭐⭐ | ⚡⚡⚡⚡⚡ | 🧠 |

**Recommendation**: Start with **Option 3 (Multi-Anchor)** for Phase 9 testing, then implement **Option 1 (GPU Shader)** in Phase 10 for production quality.


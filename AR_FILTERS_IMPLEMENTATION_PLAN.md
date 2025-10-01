# 🎭 AR Filters Implementation Plan for SegmeCam

## Executive Summary

This document outlines the implementation plan for adding native augmented reality (AR) face filters to SegmeCam. The implementation will leverage existing MediaPipe face tracking (468 landmarks), OpenGL rendering pipeline, and SDL2 infrastructure to create a performant, modular AR filter system.

**Target**: Native C++/OpenGL implementation that maintains 30 FPS performance with existing beauty effects.

---

## 1. Overview & Architecture

### 1.1 Current SegmeCam Advantages

SegmeCam is already well-positioned for AR filters:

- ✅ **Face Detection & Tracking**: MediaPipe provides 468 facial landmarks in real-time
- ✅ **OpenGL Rendering Pipeline**: SDL2 + OpenGL context already established
- ✅ **Real-time Processing**: Maintains 30 FPS with complex effects
- ✅ **Face Mesh Data**: Landmark positions updated every frame
- ✅ **Modular Architecture**: Phase 1-7 refactoring complete, easy to extend
- 🔄 **Blendshapes (TO RE-IMPLEMENT)**: MediaPipe FaceLandmarker can output 52 facial expression coefficients

### 1.2 What We Need to Add

- **Re-implement MediaPipe blendshapes** (52 facial expression coefficients: 0.0-1.0)
- 3D face mesh construction from MediaPipe landmarks
- 3D model loading system (OBJ/GLTF formats)
- Texture mapping and projection onto face mesh
- Real-time 3D transforms (rotation, scale, position tracking)
- **Expression-driven filter behaviors** (use blendshapes for dynamic responses)
- Filter asset management system
- Filter selection UI
- Performance optimization for 3D rendering

### 1.3 Proposed Architecture

```
mediapipe/examples/desktop/segmecam/
├── include/
│   └── ar_filters/
│       ├── ar_filter_manager.h      # Main AR filter coordinator
│       ├── blendshape_processor.h   # Facial expression analysis (52 coefficients)
│       ├── face_mesh_builder.h      # 3D mesh from MediaPipe landmarks
│       ├── model_loader.h           # OBJ/GLTF 3D model loading
│       ├── texture_manager.h        # Texture loading & caching
│       ├── transform_calculator.h   # Head pose & 3D transforms
│       └── filter_asset.h           # Filter definition & metadata
├── src/
│   └── ar_filters/
│       ├── ar_filter_manager.cpp
│       ├── blendshape_processor.cpp
│       ├── face_mesh_builder.cpp
│       ├── model_loader.cpp
│       ├── texture_manager.cpp
│       ├── transform_calculator.cpp
│       └── filter_asset.cpp
├── assets/
│   └── filters/                     # AR filter assets
│       ├── glasses/
│       │   ├── filter.json          # Filter metadata
│       │   ├── model.obj            # 3D model
│       │   └── texture.png          # Texture map
│       ├── hat/
│       ├── mask/
│       └── bunny_ears/
└── BUILD                            # Add ar_filters targets
```

---

## 2. Implementation Phases

### Phase 0: Re-implement MediaPipe Blendshapes (Week 1) 🎯 PREREQUISITE

**Goal**: Re-enable MediaPipe FaceLandmarker blendshape output for facial expression detection

**Why This Is Important**:

- 📊 **Precise Expressions**: 52 coefficients (0.0-1.0) for exact facial expression values
- 🎭 **Dynamic AR Filters**: Filters respond to expressions (blink, smile, jaw open, etc.)
- ✨ **Enhanced Wrinkles**: Already using smile/squint boost - blendshapes provide exact values
- 🎮 **Interactive Filters**: Trigger animations/effects based on expressions
- 🧮 **Better Than Geometry**: No need to calculate expressions from landmark distances

**Blendshape Categories** (52 total):

- **Brows** (5): browDownLeft, browDownRight, browInnerUp, browOuterUpLeft, browOuterUpRight
- **Cheeks** (3): cheekPuff, cheekSquintLeft, cheekSquintRight  
- **Eyes** (14): eyeBlinkLeft/Right, eyeLookDown/In/Out/Up (L/R), eyeSquint/Wide (L/R)
- **Jaw** (4): jawForward, jawLeft, jawOpen, jawRight
- **Mouth** (24): mouthClose, mouthDimple/Frown/Funnel/Left/Right, mouthLowerDown (L/R), mouthPress/Pucker (L/R), mouthRoll/Shrug (Upper/Lower), mouthSmile/Stretch (L/R), mouthUpperUp (L/R)
- **Nose** (2): noseSneerLeft, noseSneerRight
- **Tongue** (1): tongueOut

**Files to Create**:

- `include/ar_filters/blendshape_processor.h`
- `src/ar_filters/blendshape_processor.cpp`

**Files to Modify**:

- `src/mediapipe_manager/mediapipe_manager.cpp` - Enable blendshape output in FaceLandmarker
- `include/app_state.h` - Add blendshape array storage

**Technical Details**:

```cpp
// include/ar_filters/blendshape_processor.h
namespace segmecam {

enum class BlendshapeIndex {
  BROW_DOWN_LEFT = 0,
  BROW_DOWN_RIGHT = 1,
  BROW_INNER_UP = 2,
  BROW_OUTER_UP_LEFT = 3,
  BROW_OUTER_UP_RIGHT = 4,
  CHEEK_PUFF = 5,
  CHEEK_SQUINT_LEFT = 6,
  CHEEK_SQUINT_RIGHT = 7,
  EYE_BLINK_LEFT = 8,
  EYE_BLINK_RIGHT = 9,
  // ... (all 52 indices)
  TONGUE_OUT = 51
};

struct BlendshapeData {
  std::array<float, 52> coefficients;  // 0.0 - 1.0 values
  int64_t timestamp_us;
  
  float Get(BlendshapeIndex idx) const { return coefficients[static_cast<size_t>(idx)]; }
  void Set(BlendshapeIndex idx, float value) { coefficients[static_cast<size_t>(idx)] = value; }
};

class BlendshapeProcessor {
public:
  BlendshapeProcessor();
  
  // Process raw MediaPipe blendshapes
  void Update(const mediapipe::ClassificationList& blendshapes);
  
  // Get current blendshape data
  const BlendshapeData& GetBlendshapes() const { return current_blendshapes_; }
  
  // Expression queries (higher-level helpers)
  bool IsSmiling() const;           // mouthSmileLeft/Right > threshold
  bool IsBlinking() const;          // eyeBlinkLeft/Right > threshold
  float GetJawOpenness() const;     // jawOpen value
  float GetSmileIntensity() const;  // Average of mouthSmile L/R
  float GetSquintIntensity() const; // Average of cheekSquint L/R
  
  // Smoothing (reduce jitter)
  void SetSmoothingFactor(float alpha);  // 0.0 = no smoothing, 1.0 = heavy smoothing
  
private:
  BlendshapeData current_blendshapes_;
  BlendshapeData smoothed_blendshapes_;
  float smoothing_alpha_ = 0.3f;
};

} // namespace segmecam
```

**MediaPipe Integration**:

```cpp
// In mediapipe_manager.cpp, configure FaceLandmarker:
FaceLandmarkerOptions options;
options.base_options.model_asset_path = "face_landmarker_v2_with_blendshapes.task";
options.num_faces = 1;
options.output_face_blendshapes = true;  // ← ENABLE THIS
options.output_facial_transformation_matrixes = true;
```

**Use Cases for AR Filters**:

1. **Responsive Glasses**: Shake/darken when blinking (eyeBlinkLeft/Right)
2. **Interactive Hat**: Falls off when mouth opens wide (jawOpen > 0.7)
3. **Smile Effects**: Color changes or sparkles when smiling (mouthSmileLeft/Right)
4. **Wink Detection**: Switch filters on wink (one eye closed, other open)
5. **Tongue Triggers**: Special effect when tongue out (tongueOut > 0.5)

**Integration with Existing Wrinkle Detection**:

```cpp
// In advanced_skin_effects.cpp, replace geometric calculations:
// OLD: Calculate smile from landmark distances
// NEW: Use blendshape values directly
float smile_intensity = blendshapes.GetSmileIntensity();  // More accurate!
float squint_intensity = blendshapes.GetSquintIntensity();

// Apply to wrinkle boost
config.smile_boost = smile_intensity * state.fx_skin_smile_boost;
config.squint_boost = squint_intensity * state.fx_skin_squint_boost;
```

**Testing Criteria**:

- ✅ 52 blendshape values extracted from MediaPipe output
- ✅ Values range from 0.0 to 1.0
- ✅ Updates at 30 FPS without frame drops
- ✅ Smoothing reduces jitter
- ✅ Expression queries work correctly (IsSmiling, IsBlinking, etc.)
- ✅ Integration with app_state for downstream usage

**Performance Impact**: ~0.5ms per frame (negligible)

---

### Phase 1: Face Mesh Processing & Coordinate System (Week 2-3) ✅ COMPLETE

**Goal**: Extract MediaPipe face mesh and establish robust coordinate transformation system

**Status**: ✅ ALL STEPS COMPLETE

**Progress**: 100% complete (4/4 steps done)

#### Step 1: Face Mesh Processor ✅ COMPLETE

---

### Phase 2: Head Pose & Transform Calculation (Week 2-3)

**Goal**: Calculate 3D head pose and transforms for accurate filter placement

**Files to Create**:

- `include/ar_filters/transform_calculator.h`
- `src/ar_filters/transform_calculator.cpp`

**Key Responsibilities**:

1. Estimate head pose (yaw, pitch, roll) from landmarks
2. Calculate 3D rotation matrix
3. Estimate head position and scale
4. Provide anchor points for filter attachment (e.g., nose bridge, forehead, ears)
5. Smooth transforms to reduce jitter

**Technical Details**:

```cpp
class TransformCalculator {
public:
  struct HeadPose {
    glm::vec3 position;           // Head center position
    glm::quat rotation;           // Head orientation (quaternion)
    float scale;                  // Head scale factor
    glm::vec3 yaw_pitch_roll;     // Euler angles in degrees
  };
  
  struct AnchorPoint {
    std::string name;             // "nose_bridge", "left_ear", "right_ear", "forehead"
    glm::vec3 position;           // 3D position
    glm::quat rotation;           // Local orientation
  };
  
  HeadPose CalculatePose(const std::vector<mediapipe::NormalizedLandmark>& landmarks);
  std::vector<AnchorPoint> CalculateAnchorPoints(const HeadPose& pose, 
                                                   const std::vector<mediapipe::NormalizedLandmark>& landmarks);
  glm::mat4 SmoothTransform(const glm::mat4& current, const glm::mat4& target, float alpha = 0.3f);
};
```

**Anchor Point Definitions**:

- **Nose Bridge**: Average of landmarks 6, 197, 195 (for glasses)
- **Left Ear**: Landmarks 234, 127, 162 (for earrings, headphones)
- **Right Ear**: Landmarks 454, 356, 389 (for earrings, headphones)
- **Forehead Center**: Landmark 10 (for hats, crowns)
- **Chin**: Landmark 152 (for beards, masks)

**Testing Criteria**:

- Head pose matches actual head orientation
- Anchor points stable during head movement
- Smooth tracking without jitter
- Works with various head sizes and distances

---

### Phase 3: 3D Model Loading System (Week 3-4)

**Goal**: Load and parse 3D models (OBJ format initially)

**Files to Create**:

- `include/ar_filters/model_loader.h`
- `src/ar_filters/model_loader.cpp`

**Key Responsibilities**:

1. Parse OBJ file format (vertices, normals, UVs, faces)
2. Load associated MTL material files
3. Create OpenGL vertex buffers (VBO) and vertex array objects (VAO)
4. Support multiple mesh objects in single OBJ file
5. Optimize mesh data for GPU upload

**Technical Details**:

```cpp
class ModelLoader {
public:
  struct Mesh {
    GLuint vao;                   // Vertex Array Object
    GLuint vbo;                   // Vertex Buffer Object
    GLuint ebo;                   // Element Buffer Object
    size_t index_count;           // Number of indices to draw
    std::string material_name;    // Associated material
  };
  
  struct Model {
    std::vector<Mesh> meshes;
    std::map<std::string, Material> materials;
    glm::vec3 bounds_min;         // Bounding box
    glm::vec3 bounds_max;
  };
  
  absl::StatusOr<Model> LoadOBJ(const std::string& filepath);
  void UnloadModel(Model& model);
};
```

**Dependencies**:

- Consider using **Assimp** (Open Asset Import Library) for robust model loading
  - Supports OBJ, GLTF, FBX, STL, and 50+ formats
  - MIT/BSD license (Apache 2.0 compatible)
  - Bazel integration: `@assimp//:assimp`

**Alternative**: Implement minimal OBJ parser (no external dependency)

- Simpler, lighter weight
- Only supports OBJ format
- ~300-400 lines of code

**Testing Criteria**:

- Load sample models (glasses, hat, mask)
- Verify vertex data correctness
- OpenGL buffers created properly
- No memory leaks

---

### Phase 4: Texture Management (Week 4)

**Goal**: Load, cache, and manage textures for 3D models

**Files to Create**:

- `include/ar_filters/texture_manager.h`
- `src/ar_filters/texture_manager.cpp`

**Key Responsibilities**:

1. Load PNG/JPEG textures using existing OpenCV
2. Upload textures to GPU (OpenGL)
3. Cache loaded textures to avoid reloading
4. Support alpha transparency
5. Generate mipmaps for quality

**Technical Details**:

```cpp
class TextureManager {
public:
  struct Texture {
    GLuint id;                    // OpenGL texture ID
    int width;
    int height;
    int channels;                 // RGB=3, RGBA=4
    bool has_alpha;
  };
  
  absl::StatusOr<Texture> LoadTexture(const std::string& filepath);
  Texture GetTexture(const std::string& filepath);  // Cached lookup
  void UnloadTexture(const std::string& filepath);
  void UnloadAll();
  
private:
  std::map<std::string, Texture> texture_cache_;
};
```

**Integration with Existing Code**:

- Leverage existing `RenderManager` OpenGL context
- Use OpenCV's `cv::imread()` for image loading (already in use)
- Coordinate with existing texture management in `RenderManager`

**Testing Criteria**:

- Load textures successfully
- Textures render on 3D models
- Transparency works correctly
- No texture leaks or corruption

---

### Phase 5: Filter Asset Definition (Week 5)

**Goal**: Define filter metadata and asset packaging format

**Files to Create**:

- `include/ar_filters/filter_asset.h`
- `src/ar_filters/filter_asset.cpp`
- `assets/filters/*/filter.json` (sample filter definitions)

**Key Responsibilities**:

1. Define JSON schema for filter metadata
2. Parse filter configuration files
3. Validate filter assets (models, textures exist)
4. Support multiple attachment points per filter
5. Define filter categories (glasses, hats, masks, effects)

**Filter JSON Schema**:

```json
{
  "name": "Classic Glasses",
  "category": "glasses",
  "version": "1.0",
  "author": "SegmeCam",
  "thumbnail": "thumbnail.png",
  "attachments": [
    {
      "anchor": "nose_bridge",
      "model": "glasses.obj",
      "texture": "glasses_texture.png",
      "scale": 1.0,
      "offset": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0]
    }
  ],
  "lighting": {
    "ambient": [0.4, 0.4, 0.4],
    "diffuse": [0.8, 0.8, 0.8],
    "specular": [1.0, 1.0, 1.0]
  },
  "behaviors": {
    "eyeBlinkLeft": {
      "type": "shake",
      "threshold": 0.7,
      "intensity": 0.05
    },
    "eyeBlinkRight": {
      "type": "shake",
      "threshold": 0.7,
      "intensity": 0.05
    },
    "jawOpen": {
      "type": "fall_off",
      "threshold": 0.8,
      "gravity": 9.8
    }
  }
}
```

**Technical Details**:

```cpp
class FilterAsset {
public:
  struct Attachment {
    std::string anchor_name;      // "nose_bridge", "left_ear", etc.
    std::string model_path;
    std::string texture_path;
    glm::vec3 scale;
    glm::vec3 offset;
    glm::vec3 rotation;
  };
  
  struct FilterMetadata {
    std::string name;
    std::string category;
    std::string version;
    std::string author;
    std::string thumbnail_path;
    std::vector<Attachment> attachments;
  };
  
  static absl::StatusOr<FilterMetadata> LoadFilter(const std::string& filter_dir);
  static std::vector<std::string> EnumerateFilters(const std::string& filters_root);
};
```

**Testing Criteria**:

- Load sample filter definitions
- Validate JSON parsing
- Enumerate available filters
- Handle missing assets gracefully

---

### Phase 6: AR Filter Manager (Week 6-7)

**Goal**: Main coordinator for AR filter system

**Files to Create**:

- `include/ar_filters/ar_filter_manager.h`
- `src/ar_filters/ar_filter_manager.cpp`

**Key Responsibilities**:

1. Coordinate all AR filter subsystems
2. Manage active filter state
3. Render filters onto video frames
4. Handle filter switching
5. Performance monitoring

**Technical Details**:

```cpp
class ARFilterManager {
public:
  ARFilterManager();
  ~ARFilterManager();
  
  absl::Status Initialize(const std::string& filters_path);
  void Cleanup();
  
  // Filter management
  std::vector<std::string> GetAvailableFilters();
  absl::Status SetActiveFilter(const std::string& filter_name);
  void ClearActiveFilter();
  std::string GetActiveFilterName() const;
  
  // Rendering
  void Update(const std::vector<mediapipe::NormalizedLandmark>& landmarks,
              const cv::Mat& frame);
  void Render(GLuint framebuffer_id, int width, int height);
  
  // Performance
  float GetRenderTimeMs() const;
  
private:
  std::unique_ptr<FaceMeshBuilder> mesh_builder_;
  std::unique_ptr<TransformCalculator> transform_calculator_;
  std::unique_ptr<ModelLoader> model_loader_;
  std::unique_ptr<TextureManager> texture_manager_;
  
  std::map<std::string, FilterAsset::FilterMetadata> available_filters_;
  std::string active_filter_name_;
  // ... loaded models, textures, shaders, etc.
};
```

**Integration Points**:

- Add to `ApplicationRun` class as `std::unique_ptr<ARFilterManager> ar_filter_mgr_`
- Call `Update()` after MediaPipe processing in main loop
- Call `Render()` after effects but before UI rendering
- Wire filter selection to new UI panel

**Testing Criteria**:

- Render filters correctly aligned with face
- Switch filters without crashes
- Maintain 30 FPS performance
- No memory leaks

---

### Phase 7: OpenGL Shaders for 3D Rendering (Week 7)

**Goal**: Create GLSL shaders for 3D model rendering with lighting

**Files to Create**:

- `mediapipe/examples/desktop/segmecam/shaders/ar_filter.vert` (vertex shader)
- `mediapipe/examples/desktop/segmecam/shaders/ar_filter.frag` (fragment shader)

**Vertex Shader** (`ar_filter.vert`):

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
```

**Fragment Shader** (`ar_filter.frag`):

```glsl
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform sampler2D texture1;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

void main() {
    // Ambient
    float ambientStrength = 0.4;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec4 texColor = texture(texture1, TexCoord);
    vec3 result = (ambient + diffuse + specular) * texColor.rgb;
    FragColor = vec4(result, texColor.a);
}
```

**Shader Management**:

- Add shader compilation/linking to `ARFilterManager` initialization
- Store shader program IDs
- Set uniforms per frame (model, view, projection matrices)

**Testing Criteria**:

- Shaders compile successfully
- 3D models render with proper lighting
- Textures display correctly
- Performance remains at 30+ FPS

---

### Phase 8: UI Integration (Week 8)

**Goal**: Add filter selection UI to SegmeCam interface

**Files to Modify**:

- `include/ui/ui_panels.h` (add `ARFilterPanel`)
- `src/ui/ar_filter_panel.cpp` (new file)
- `include/ui/ui_manager_enhanced.h` (register new panel)
- `src/ui/ui_manager_enhanced.cpp` (instantiate panel)

**ARFilterPanel Responsibilities**:

1. Display available filters with thumbnails
2. Allow filter selection
3. Show "No Filter" option
4. Display filter name/author
5. Show performance impact

**UI Layout**:

```cpp
class ARFilterPanel : public UIPanel {
public:
  ARFilterPanel(AppState& state, ARFilterManager& ar_mgr);
  void Render() override;
  
private:
  void RenderFilterGrid();
  void RenderFilterInfo();
  void RenderPerformanceInfo();
  
  AppState& state_;
  ARFilterManager& ar_mgr_;
};
```

**UI Mockup**:

```
┌─ AR Filters ──────────────────────────┐
│ ┌────┐ ┌────┐ ┌────┐ ┌────┐          │
│ │None│ │👓 │ │🎩 │ │😺 │  [More...] │
│ └────┘ └────┘ └────┘ └────┘          │
│                                        │
│ Active: Classic Glasses                │
│ Author: SegmeCam                       │
│ Render Time: 2.3ms                     │
│                                        │
│ [ ] Preview Only (don't output)        │
└────────────────────────────────────────┘
```

**Testing Criteria**:

- UI displays available filters
- Clicking filter applies it
- Performance stats update
- UI responsive and clear

---

### Phase 9: Sample Filters Creation (Week 9)

**Goal**: Create 3-5 sample AR filters for testing and demonstration

**Sample Filters**:

1. **Classic Glasses**
   - Simple black-rimmed glasses
   - Attached to nose bridge
   - Tests basic model rendering

2. **Top Hat**
   - Classic top hat
   - Attached to forehead
   - Tests vertical offset and scaling

3. **Cat Ears**
   - Cute cat ears
   - Attached to top of head
   - Tests multiple attachment points

4. **Face Mask**
   - Decorative carnival mask
   - Covers upper face area
   - Tests face mesh mapping

5. **Bunny Ears** (Bonus)
   - Floppy bunny ears
   - Tests animation potential (future)

**Asset Creation Workflow**:

1. Model in Blender (free, open source)
2. Export as OBJ with proper scale
3. Create UV-mapped texture in GIMP/Krita
4. Write filter.json metadata
5. Test in SegmeCam

**Testing Criteria**:

- All sample filters load and render
- Proper alignment with face features
- Smooth tracking during head movement
- Textures display correctly

---

### Phase 9.5: Expression-Driven Filter Behaviors (Week 10)

**Goal**: Make filters respond to facial expressions using blendshape data

**Dependencies**: Phase 0 (Blendshapes), Phase 6 (AR Filter Manager), Phase 9 (Sample Filters)

**Key Responsibilities**:

1. Implement behavior system for filters
2. Parse behavior definitions from filter JSON
3. Map blendshapes to filter transformations
4. Create common behavior presets (shake, bounce, fall, color change)
5. Add behavior preview/testing tools

**Behavior Types**:

```cpp
// include/ar_filters/filter_behavior.h
namespace segmecam {

enum class BehaviorType {
  SHAKE,           // Rapid small movement (e.g., glasses shake on blink)
  BOUNCE,          // Spring-like motion
  FALL_OFF,        // Gravity simulation (e.g., hat falls when jaw opens)
  SCALE,           // Size change
  ROTATE,          // Rotation animation
  COLOR_CHANGE,    // Texture color modulation
  OPACITY_FADE,    // Alpha transparency change
  PARTICLE_EMIT    // Trigger particle effect (future)
};

struct BehaviorConfig {
  BehaviorType type;
  BlendshapeIndex trigger_blendshape;
  float threshold;         // Activation threshold (0.0-1.0)
  float intensity;         // Effect strength
  float duration_ms;       // Animation duration
  bool continuous;         // Continuous or one-shot
};

class FilterBehaviorProcessor {
public:
  FilterBehaviorProcessor();
  
  // Register behaviors from filter JSON
  void RegisterBehavior(const BehaviorConfig& config);
  void ClearBehaviors();
  
  // Process behaviors each frame
  glm::mat4 ProcessBehaviors(const BlendshapeData& blendshapes,
                             const glm::mat4& base_transform,
                             float delta_time_ms);
  
private:
  std::vector<BehaviorConfig> behaviors_;
  std::map<BehaviorType, std::function<glm::mat4(const BehaviorConfig&, float)>> behavior_functions_;
};

} // namespace segmecam
```

**Implementation Examples**:

1. **Shake on Blink**:

```cpp
glm::mat4 ApplyShakeBehavior(const BehaviorConfig& config, float blendshape_value) {
  if (blendshape_value < config.threshold) return glm::mat4(1.0f);
  
  float shake_amount = config.intensity * blendshape_value;
  float offset_x = (rand() / (float)RAND_MAX - 0.5f) * shake_amount;
  float offset_y = (rand() / (float)RAND_MAX - 0.5f) * shake_amount;
  
  return glm::translate(glm::mat4(1.0f), glm::vec3(offset_x, offset_y, 0.0f));
}
```

2. **Fall Off on Jaw Open**:

```cpp
glm::mat4 ApplyFallOffBehavior(const BehaviorConfig& config, float blendshape_value, float delta_time) {
  if (blendshape_value < config.threshold) {
    fall_velocity_ = 0.0f;
    fall_position_ = 0.0f;
    return glm::mat4(1.0f);
  }
  
  // Simulate gravity
  fall_velocity_ += config.gravity * delta_time / 1000.0f;
  fall_position_ += fall_velocity_ * delta_time / 1000.0f;
  
  glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -fall_position_, 0.0f));
  transform = glm::rotate(transform, fall_position_ * 0.5f, glm::vec3(0.0f, 0.0f, 1.0f)); // Tumble
  return transform;
}
```

3. **Color Change on Smile**:

```cpp
glm::vec4 ApplyColorChangeBehavior(const BehaviorConfig& config, float blendshape_value) {
  if (blendshape_value < config.threshold) return glm::vec4(1.0f);
  
  float t = (blendshape_value - config.threshold) / (1.0f - config.threshold);
  glm::vec3 start_color = glm::vec3(1.0f, 1.0f, 1.0f);  // White
  glm::vec3 end_color = glm::vec3(1.0f, 0.5f, 0.8f);    // Pink
  glm::vec3 color = glm::mix(start_color, end_color, t * config.intensity);
  
  return glm::vec4(color, 1.0f);
}
```

**Example Filter with Behaviors** (updated JSON):

```json
{
  "name": "Interactive Sunglasses",
  "category": "glasses",
  "attachments": [...],
  "behaviors": {
    "eyeBlinkLeft": {
      "type": "shake",
      "threshold": 0.6,
      "intensity": 0.03,
      "duration_ms": 100
    },
    "eyeBlinkRight": {
      "type": "shake",
      "threshold": 0.6,
      "intensity": 0.03,
      "duration_ms": 100
    },
    "mouthSmileLeft": {
      "type": "color_change",
      "threshold": 0.4,
      "intensity": 0.8,
      "target_color": [1.0, 0.7, 0.3]
    },
    "mouthSmileRight": {
      "type": "color_change",
      "threshold": 0.4,
      "intensity": 0.8,
      "target_color": [1.0, 0.7, 0.3]
    }
  }
}
```

**Integration Points**:

- Update `FilterAsset` class to parse behaviors section
- Add `FilterBehaviorProcessor` to `ARFilterManager`
- Call `ProcessBehaviors()` in render loop before applying transforms
- UI toggle: "Enable interactive filters" (performance option)

**Testing Criteria**:

- ✅ Behaviors trigger at correct blendshape thresholds
- ✅ Animations smooth and responsive
- ✅ Multiple behaviors can run simultaneously
- ✅ No performance degradation (< 1ms overhead)
- ✅ Behaviors work across all sample filters

**Sample Filters to Update**:

1. **Interactive Glasses**: Shake on blink
2. **Falling Hat**: Falls off when jaw opens wide
3. **Color-Changing Mask**: Changes color when smiling
4. **Bouncing Ears**: Bounce when raising eyebrows

---

### Phase 10: Performance Optimization (Week 11)

**Goal**: Ensure AR filters maintain 30 FPS target

**Optimization Strategies**:

1. **Level of Detail (LOD)**
   - Use simplified models for distant/small faces
   - Reduce polygon count when appropriate

2. **Culling**
   - Don't render filters when face not detected
   - Frustum culling for off-screen elements

3. **Batching**
   - Batch multiple filter attachments in single draw call
   - Minimize state changes

4. **Texture Optimization**
   - Use compressed textures (DXT/BC formats)
   - Mipmap generation for quality/performance balance

5. **CPU-GPU Synchronization**
   - Upload transform matrices efficiently
   - Minimize CPU-GPU data transfer

6. **Profiling**
   - Measure render time per filter
   - Identify bottlenecks
   - Profile with existing `PerformanceMonitor`

**Performance Targets**:

- AR filter rendering: < 3ms per frame
- Total frame time: < 33ms (30 FPS)
- No frame drops during filter switching
- Memory usage: < 100MB for filter assets

**Testing Criteria**:

- Maintain 30 FPS with filters active
- Smooth head tracking
- Quick filter switching (< 100ms)
- Stable memory usage

---

### Phase 11: Testing & Refinement (Week 11-12)

**Goal**: Comprehensive testing and bug fixing

**Test Scenarios**:

1. **Face Detection Edge Cases**
   - Partial face visibility
   - Multiple faces in frame
   - Very close/far faces
   - Profile views

2. **Head Movement**
   - Fast head rotation
   - Sudden movements
   - Nodding/shaking
   - Looking down/up

3. **Lighting Conditions**
   - Bright lighting
   - Low light
   - Backlit faces
   - Mixed lighting

4. **Filter Variety**
   - All sample filters
   - Complex models
   - Large textures
   - Multiple attachments

5. **Performance Stress**
   - AR filters + beauty effects
   - Background blur + AR
   - Virtual camera output
   - High resolution (1920x1080)

**Testing Checklist**:

- [ ] All filters render correctly
- [ ] No crashes or memory leaks
- [ ] Maintains 30 FPS target
- [ ] Smooth head tracking
- [ ] Filter switching works
- [ ] UI is intuitive
- [ ] Assets load properly
- [ ] No visual glitches
- [ ] Works with beauty effects
- [ ] Virtual camera output works

---

## 3. Technical Dependencies

### 3.1 Required Libraries

| Library | Purpose | License | Integration |
|---------|---------|---------|-------------|
| **MediaPipe** | Face landmarks | Apache 2.0 | ✅ Already integrated |
| **OpenGL** | 3D rendering | MIT | ✅ Already integrated |
| **SDL2** | Window/context | Zlib | ✅ Already integrated |
| **OpenCV** | Image I/O | Apache 2.0 | ✅ Already integrated |
| **GLM** | Math library | MIT | ⚠️ Need to add |
| **Assimp** (optional) | Model loading | BSD | ⚠️ Optional dependency |
| **nlohmann/json** | JSON parsing | MIT | ⚠️ Need to add |

### 3.2 Bazel Integration

**Add to WORKSPACE**:

```python
# GLM - OpenGL Mathematics
http_archive(
    name = "glm",
    urls = ["https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.tar.gz"],
    strip_prefix = "glm-0.9.9.8",
    build_file = "@//third_party:glm.BUILD",
)

# nlohmann/json
http_archive(
    name = "json",
    urls = ["https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.tar.gz"],
    strip_prefix = "json-3.11.2",
    build_file = "@//third_party:json.BUILD",
)

# Assimp (optional)
http_archive(
    name = "assimp",
    urls = ["https://github.com/assimp/assimp/archive/refs/tags/v5.3.1.tar.gz"],
    strip_prefix = "assimp-5.3.1",
    build_file = "@//third_party:assimp.BUILD",
)
```

**Add to BUILD**:

```python
cc_library(
    name = "ar_filters",
    srcs = glob(["src/ar_filters/*.cpp"]),
    hdrs = glob(["include/ar_filters/*.h"]),
    deps = [
        "@glm//:glm",
        "@json//:json",
        "//mediapipe/framework:calculator_framework",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//third_party:opencv",
        ":effects_manager",
    ],
    copts = ["-std=c++17"],
)
```

---

## 4. Development Timeline

### Detailed Schedule

| Phase | Duration | Tasks | Deliverables |
|-------|----------|-------|--------------|
| **Phase 0** | 1 week | Re-implement blendshapes | 52 expression coefficients available |
| **Phase 1** | 2 weeks | Face mesh construction | Working 3D face mesh from landmarks |
| **Phase 2** | 1 week | Transform calculation | Head pose estimation, anchor points |
| **Phase 3** | 1 week | Model loading | OBJ loader, OpenGL buffers |
| **Phase 4** | 1 week | Texture management | Texture loading, caching, GPU upload |
| **Phase 5** | 1 week | Filter assets | JSON schema, asset packaging |
| **Phase 6** | 2 weeks | AR Filter Manager | Main coordinator, integration |
| **Phase 7** | 1 week | OpenGL shaders | Vertex/fragment shaders, lighting |
| **Phase 8** | 1 week | UI integration | Filter selection panel |
| **Phase 9** | 1 week | Sample filters | 3-5 demo filters with assets |
| **Phase 9.5** | 1 week | Expression-driven behaviors | Blendshape-responsive filters |
| **Phase 10** | 1 week | Performance optimization | Profiling, optimization |
| **Phase 11-12** | 2 weeks | Testing & refinement | Bug fixes, polish |

**Total Duration**: ~13 weeks (~3.25 months)

---

## 5. Testing Strategy

### 5.1 Unit Tests

Each component should have unit tests:

- `face_mesh_builder_test.cpp` - Mesh construction correctness
- `transform_calculator_test.cpp` - Pose estimation accuracy
- `model_loader_test.cpp` - OBJ parsing validation
- `texture_manager_test.cpp` - Texture loading/caching
- `filter_asset_test.cpp` - JSON parsing validation
- `ar_filter_manager_test.cpp` - Integration tests

### 5.2 Integration Tests

Test AR system with existing SegmeCam features:

- AR filters + beauty effects
- AR filters + background blur
- AR filters + virtual camera output
- AR filters + profile saving/loading

### 5.3 Performance Tests

Benchmark critical paths:

- Face mesh construction time
- Model rendering time
- Texture upload time
- Overall frame time with AR active
- Memory usage with multiple filters loaded

### 5.4 Visual Tests

Manual testing scenarios:

- Filter alignment accuracy
- Tracking smoothness
- Lighting quality
- Texture quality
- Edge cases (partial visibility, fast movement)

---

## 6. Success Criteria

### 6.1 Functional Requirements

- ✅ Load and display 3D models on face
- ✅ Accurate head pose tracking
- ✅ Smooth filter transitions
- ✅ Multiple anchor points supported
- ✅ Filter selection UI functional
- ✅ At least 5 sample filters included

### 6.2 Performance Requirements

- ✅ Maintain 30 FPS with AR filters active
- ✅ Filter rendering < 3ms per frame
- ✅ Filter switching < 100ms
- ✅ Memory usage < 100MB for assets
- ✅ No frame drops during head movement

### 6.3 Quality Requirements

- ✅ Accurate filter alignment (< 2 pixel jitter)
- ✅ Smooth tracking (no jerky movement)
- ✅ Realistic lighting and shadows
- ✅ High-quality textures (PNG with alpha)
- ✅ No visual artifacts or glitches

### 6.4 Usability Requirements

- ✅ Intuitive filter selection UI
- ✅ Clear filter thumbnails
- ✅ One-click filter application
- ✅ Quick filter removal
- ✅ Performance stats visible

---

## 7. Future Enhancements

### 7.1 Phase 13+ (Post-MVP)

**Animated Filters**:

- Support simple animations (bobbing, rotation)
- Time-based keyframe interpolation
- Physics simulation (floppy ears, dangling earrings)

**Advanced Effects**:

- Particle systems (sparkles, smoke)
- Screen-space effects (glow, blur)
- Dynamic textures (changing colors)

**User-Generated Filters**:

- Filter creation tool
- Import custom 3D models
- Community filter sharing

**AR Effects Library**:

- Expand to 20+ filters
- Categorization (fun, professional, seasonal)
- Filter ratings and favorites

**Mobile Export**:

- Export filters for mobile apps
- Cross-platform filter format
- Cloud filter repository

---

## 8. Risk Management

### 8.1 Identified Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Performance degradation | Medium | High | Early profiling, optimization phase |
| Complex math for transforms | Medium | Medium | Use proven algorithms, GLM library |
| 3D model compatibility | Low | Medium | Support OBJ initially, expand later |
| Tracking instability | Medium | High | Implement smoothing, test extensively |
| GPU compatibility | Low | High | Fallback to simpler rendering |
| Large asset sizes | Medium | Medium | Optimize models, compress textures |

### 8.2 Contingency Plans

**If performance targets not met**:

1. Reduce model complexity (fewer polygons)
2. Implement LOD system
3. Disable filters on slower hardware
4. Optimize shaders further

**If tracking too unstable**:

1. Increase smoothing factor
2. Use Kalman filtering
3. Implement prediction/extrapolation
4. Reduce update frequency

**If model loading issues**:

1. Fall back to minimal OBJ parser
2. Support only subset of OBJ features
3. Pre-process models to simplified format

---

## 9. Documentation Requirements

### 9.1 Developer Documentation

- **AR_FILTERS_API.md** - API reference for AR filter system
- **FILTER_CREATION_GUIDE.md** - How to create custom filters
- **3D_MODEL_GUIDELINES.md** - Model requirements and best practices
- Code comments for complex algorithms

### 9.2 User Documentation

- **User Guide** - How to use AR filters in SegmeCam
- **Filter Gallery** - Showcase of available filters
- **FAQ** - Common questions and troubleshooting

### 9.3 Technical Documentation

- **ARCHITECTURE.md** - System architecture diagrams
- **PERFORMANCE.md** - Performance analysis and benchmarks
- **TESTING.md** - Testing strategy and results

---

## 10. Conclusion

This implementation plan provides a comprehensive roadmap for adding native AR filters to SegmeCam. By leveraging existing MediaPipe landmarks and OpenGL infrastructure, we can create a performant, modular AR system that maintains the 30 FPS target while adding engaging face filter capabilities.

**Key Advantages**:

- ✅ Native C++/OpenGL implementation (no web dependencies)
- ✅ Builds on existing SegmeCam architecture
- ✅ Modular design (follows Phase 1-7 patterns)
- ✅ Performance-focused from the start
- ✅ Clear testing and optimization phases
- ✅ Apache 2.0 compatible dependencies

**Next Steps**:

1. Review and approve this plan
2. Set up development branch: `feature/ar-filters`
3. Begin Phase 1: Face mesh construction
4. Iterate with testing and feedback

---

**Document Version**: 1.0  
**Created**: October 1, 2025  
**Author**: SegmeCam Development Team  
**Status**: Draft - Awaiting Approval

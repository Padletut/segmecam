# Phase 3 Step 8: Full 3D Rendering Implementation

**Date**: October 3, 2025  
**Estimated Duration**: 2-3 hours  
**Goal**: Replace 2D circle visualization with actual OpenGL 3D geometry rendering

---

## Overview

Currently, 3D models display as colored circles with material colors. This step will implement proper OpenGL rendering to show actual 3D geometry with:
- Real mesh geometry (vertices, faces)
- Depth testing (z-buffer)
- Phong lighting (ambient + directional)
- Material properties (diffuse, specular, shininess)
- Transparency support (alpha blending)

---

## Implementation Phases

### Phase 1: Shader System (45 minutes)

**Goal**: Create GLSL shaders and shader program management

#### 1.1 Create Vertex Shader
**File**: `mediapipe/examples/desktop/segmecam/shaders/model_vertex.glsl`

```glsl
#version 330 core

layout(location = 0) in vec3 aPos;       // Vertex position
layout(location = 1) in vec3 aNormal;    // Vertex normal
layout(location = 2) in vec2 aTexCoord;  // Texture coordinate

uniform mat4 uModel;       // Model matrix
uniform mat4 uView;        // View matrix
uniform mat4 uProjection;  // Projection matrix
uniform mat3 uNormalMatrix; // Normal transformation matrix

out vec3 FragPos;    // Fragment position in world space
out vec3 Normal;     // Fragment normal in world space
out vec2 TexCoord;   // Texture coordinate

void main() {
    // Transform vertex position
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    
    // Transform normal (handle non-uniform scaling)
    Normal = normalize(uNormalMatrix * aNormal);
    
    // Pass texture coordinate
    TexCoord = aTexCoord;
    
    // Final position
    gl_Position = uProjection * uView * worldPos;
}
```

#### 1.2 Create Fragment Shader
**File**: `mediapipe/examples/desktop/segmecam/shaders/model_fragment.glsl`

```glsl
#version 330 core

in vec3 FragPos;   // Fragment position from vertex shader
in vec3 Normal;    // Normal from vertex shader
in vec2 TexCoord;  // Texture coordinate from vertex shader

uniform vec3 uCameraPos;        // Camera position
uniform vec3 uLightDir;         // Directional light direction
uniform vec3 uLightColor;       // Light color
uniform vec3 uAmbientColor;     // Ambient light color

// Material properties
uniform vec3 uMaterialAmbient;  // Ka
uniform vec3 uMaterialDiffuse;  // Kd
uniform vec3 uMaterialSpecular; // Ks
uniform float uMaterialShininess; // Ns
uniform float uMaterialOpacity;   // d (1.0 = opaque)

out vec4 FragColor;

void main() {
    // Normalize vectors
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-uLightDir); // Negate for direction TO light
    vec3 viewDir = normalize(uCameraPos - FragPos);
    
    // Ambient component
    vec3 ambient = uAmbientColor * uMaterialAmbient;
    
    // Diffuse component (Lambert)
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = uLightColor * (diff * uMaterialDiffuse);
    
    // Specular component (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), uMaterialShininess);
    vec3 specular = uLightColor * (spec * uMaterialSpecular);
    
    // Combine lighting
    vec3 result = ambient + diffuse + specular;
    
    // Apply opacity
    FragColor = vec4(result, uMaterialOpacity);
}
```

#### 1.3 Create Shader Program Manager
**File**: `mediapipe/examples/desktop/segmecam/include/render/shader_program.h`

```cpp
#ifndef SEGMECAM_SHADER_PROGRAM_H
#define SEGMECAM_SHADER_PROGRAM_H

#include <string>
#include <GL/gl.h>

namespace segmecam {
namespace render {

class ShaderProgram {
public:
    ShaderProgram();
    ~ShaderProgram();
    
    // Load and compile shaders from strings
    bool LoadFromStrings(const std::string& vertex_source,
                        const std::string& fragment_source);
    
    // Load and compile shaders from files
    bool LoadFromFiles(const std::string& vertex_path,
                      const std::string& fragment_path);
    
    // Use this shader program
    void Use() const;
    
    // Get program ID
    GLuint GetProgramID() const { return program_id_; }
    
    // Uniform setters
    void SetBool(const std::string& name, bool value) const;
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec3(const std::string& name, float x, float y, float z) const;
    void SetMat4(const std::string& name, const float* value) const;
    void SetMat3(const std::string& name, const float* value) const;
    
private:
    GLuint program_id_;
    
    // Helper functions
    bool CompileShader(const std::string& source, GLenum type, GLuint& shader_id);
    bool LinkProgram(GLuint vertex_shader, GLuint fragment_shader);
    std::string ReadFile(const std::string& path);
};

} // namespace render
} // namespace segmecam

#endif // SEGMECAM_SHADER_PROGRAM_H
```

**File**: `mediapipe/examples/desktop/segmecam/src/render/shader_program.cpp`

Implementation with error checking, compilation logs, etc.

---

### Phase 2: Rendering Pipeline Setup (45 minutes)

**Goal**: Set up matrices and OpenGL state for 3D rendering

#### 2.1 Add 3D Rendering State to RenderManager
**File**: `mediapipe/examples/desktop/segmecam/include/render/render_manager.h`

```cpp
// Add to RenderManager class:
private:
    std::unique_ptr<render::ShaderProgram> model_shader_;
    
    // 3D rendering state
    glm::mat4 projection_matrix_;
    glm::mat4 view_matrix_;
    
    // Lighting
    glm::vec3 light_direction_;
    glm::vec3 light_color_;
    glm::vec3 ambient_color_;
    
public:
    // Initialize 3D rendering
    bool Initialize3DRendering();
    
    // Update projection matrix (call on window resize)
    void UpdateProjection(int width, int height);
    
    // Render a 3D model
    void Render3DModel(const ar_filters::Model& model,
                      const glm::mat4& model_matrix,
                      const ar_filters::Material& material);
```

#### 2.2 Implement Matrix Calculations
**File**: `mediapipe/examples/desktop/segmecam/src/render/render_manager.cpp`

```cpp
bool RenderManager::Initialize3DRendering() {
    // Create shader program
    model_shader_ = std::make_unique<render::ShaderProgram>();
    
    if (!model_shader_->LoadFromFiles(
        "mediapipe/examples/desktop/segmecam/shaders/model_vertex.glsl",
        "mediapipe/examples/desktop/segmecam/shaders/model_fragment.glsl")) {
        LOG(ERROR) << "Failed to load 3D model shaders";
        return false;
    }
    
    // Set up lighting
    light_direction_ = glm::vec3(0.0f, -1.0f, -0.5f); // Slight downward angle
    light_color_ = glm::vec3(1.0f, 1.0f, 1.0f);       // White light
    ambient_color_ = glm::vec3(0.3f, 0.3f, 0.3f);     // 30% ambient
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    LOG(INFO) << "3D rendering initialized successfully";
    return true;
}

void RenderManager::UpdateProjection(int width, int height) {
    // Perspective projection
    float fov = glm::radians(45.0f);
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float near = 0.1f;
    float far = 100.0f;
    
    projection_matrix_ = glm::perspective(fov, aspect, near, far);
    
    // Simple view matrix (camera at origin looking down -Z)
    view_matrix_ = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 0.0f),  // Camera position
        glm::vec3(0.0f, 0.0f, -1.0f), // Look at point
        glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
    );
}
```

---

### Phase 3: 3D Rendering Function (45 minutes)

**Goal**: Implement actual mesh rendering with VAO/VBO/EBO

#### 3.1 Implement Render3DModel
**File**: `mediapipe/examples/desktop/segmecam/src/render/render_manager.cpp`

```cpp
void RenderManager::Render3DModel(
    const ar_filters::Model& model,
    const glm::mat4& model_matrix,
    const ar_filters::Material& material) {
    
    // Use shader program
    model_shader_->Use();
    
    // Set matrices
    model_shader_->SetMat4("uModel", glm::value_ptr(model_matrix));
    model_shader_->SetMat4("uView", glm::value_ptr(view_matrix_));
    model_shader_->SetMat4("uProjection", glm::value_ptr(projection_matrix_));
    
    // Calculate normal matrix (inverse transpose of model matrix)
    glm::mat3 normal_matrix = glm::mat3(glm::transpose(glm::inverse(model_matrix)));
    model_shader_->SetMat3("uNormalMatrix", glm::value_ptr(normal_matrix));
    
    // Set camera position (for specular calculations)
    glm::vec3 camera_pos = glm::vec3(0.0f, 0.0f, 0.0f);
    model_shader_->SetVec3("uCameraPos", camera_pos.x, camera_pos.y, camera_pos.z);
    
    // Set lighting
    model_shader_->SetVec3("uLightDir", 
        light_direction_.x, light_direction_.y, light_direction_.z);
    model_shader_->SetVec3("uLightColor",
        light_color_.x, light_color_.y, light_color_.z);
    model_shader_->SetVec3("uAmbientColor",
        ambient_color_.x, ambient_color_.y, ambient_color_.z);
    
    // Set material properties
    model_shader_->SetVec3("uMaterialAmbient",
        material.ambient[0], material.ambient[1], material.ambient[2]);
    model_shader_->SetVec3("uMaterialDiffuse",
        material.diffuse[0], material.diffuse[1], material.diffuse[2]);
    model_shader_->SetVec3("uMaterialSpecular",
        material.specular[0], material.specular[1], material.specular[2]);
    model_shader_->SetFloat("uMaterialShininess", material.shininess);
    model_shader_->SetFloat("uMaterialOpacity", material.transparency);
    
    // Render each mesh
    for (const auto& mesh : model.meshes) {
        // Bind VAO
        glBindVertexArray(mesh.vao);
        
        // Draw elements
        glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, 0);
        
        // Unbind
        glBindVertexArray(0);
    }
}
```

#### 3.2 Update Frame Processor to Use 3D Rendering
**File**: `mediapipe/examples/desktop/segmecam/src/application/frame_processor.cpp`

Replace circle rendering with:

```cpp
void FrameProcessor::RenderFilterPrimitives(...) {
    // ... existing code ...
    
    for (const auto& filter : state_.attachment_controller.GetAttachedFilters()) {
        if (!filter.enabled || !filter.visible) continue;
        
        if (filter.type == FilterObject::Type::MODEL_3D && filter.model != nullptr) {
            // 3D MODEL RENDERING PATH
            Render3DModelFilter(filter, frame_width, frame_height);
        } else {
            // 2D PRIMITIVE RENDERING PATH (existing circles)
            RenderPrimitiveFilter(filter, output_frame);
        }
    }
}

void FrameProcessor::Render3DModelFilter(
    const FilterObject& filter,
    int frame_width,
    int frame_height) {
    
    // Calculate model matrix from filter transform
    glm::mat4 model_matrix = glm::mat4(1.0f);
    
    // Position (convert from normalized coordinates to world space)
    model_matrix = glm::translate(model_matrix, glm::vec3(
        filter.position.x * 2.0f - 1.0f,  // Map [0,1] to [-1,1]
        -(filter.position.y * 2.0f - 1.0f), // Flip Y and map
        -2.0f  // Place in front of camera
    ));
    
    // Rotation (from quaternion or euler angles)
    model_matrix = glm::rotate(model_matrix, filter.rotation.z, glm::vec3(0, 0, 1));
    model_matrix = glm::rotate(model_matrix, filter.rotation.y, glm::vec3(0, 1, 0));
    model_matrix = glm::rotate(model_matrix, filter.rotation.x, glm::vec3(1, 0, 0));
    
    // Scale
    float scale = filter.local_scale * 0.1f; // Adjust scale factor
    model_matrix = glm::scale(model_matrix, glm::vec3(scale, scale, scale));
    
    // Get material (use first mesh's material for now)
    const std::string& material_name = filter.model->meshes[0].material_name;
    auto mat_it = filter.model->materials.find(material_name);
    
    if (mat_it != filter.model->materials.end()) {
        // Render with RenderManager
        state_.render_manager->Render3DModel(
            *filter.model,
            model_matrix,
            mat_it->second
        );
    }
}
```

---

### Phase 4: Integration & Testing (30 minutes)

**Goal**: Test, debug, and validate 3D rendering

#### 4.1 Update BUILD Files

Add shader files and dependencies:
```python
# mediapipe/examples/desktop/segmecam/BUILD

cc_library(
    name = "shader_program",
    srcs = ["src/render/shader_program.cpp"],
    hdrs = ["include/render/shader_program.h"],
    deps = [
        "@linux_gles//:gl",
        "@com_google_absl//absl/log",
    ],
)

filegroup(
    name = "shaders",
    srcs = [
        "shaders/model_vertex.glsl",
        "shaders/model_fragment.glsl",
    ],
)
```

Add GLM (OpenGL Mathematics) to WORKSPACE if not present.

#### 4.2 Testing Checklist

- [ ] Build succeeds without errors
- [ ] Shaders compile successfully
- [ ] simple_cube.obj renders as actual cube (not circle)
- [ ] Red material color visible
- [ ] Cube rotates/scales with face tracking
- [ ] Depth testing works (multiple models)
- [ ] No FPS drop (<10% impact)
- [ ] No memory leaks
- [ ] Transparency works (if testing glasses)

#### 4.3 Debug Strategies

**If nothing renders**:
- Check glGetError() after each GL call
- Verify VAO/VBO/EBO are valid
- Check shader compilation logs
- Verify uniform locations
- Check matrices (are they identity?)

**If geometry is wrong**:
- Print vertex data
- Check winding order (GL_CW vs GL_CCW)
- Verify normal calculations
- Check model matrix scaling

**If lighting is wrong**:
- Verify normals are normalized
- Check light direction (should point TO light)
- Print material values
- Test with ambient-only first

---

## Success Criteria

✅ **Functional**:
- Actual 3D geometry visible (not circles)
- Materials render with correct colors
- Basic lighting works (not flat)
- Face tracking integration works
- Multiple models render simultaneously

✅ **Quality**:
- Depth testing works correctly
- Transparency supported
- No visual glitches
- Smooth rendering

✅ **Performance**:
- >30 FPS with 3 models
- <10% FPS drop from baseline
- No memory leaks

---

## Files to Create/Modify

### New Files
1. `shaders/model_vertex.glsl` (~40 lines)
2. `shaders/model_fragment.glsl` (~50 lines)
3. `include/render/shader_program.h` (~60 lines)
4. `src/render/shader_program.cpp` (~200 lines)

### Modified Files
1. `include/render/render_manager.h` (+30 lines)
2. `src/render/render_manager.cpp` (+150 lines)
3. `src/application/frame_processor.cpp` (+100 lines, refactor)
4. `BUILD` (+20 lines for shader_program target)
5. `WORKSPACE` (add GLM if needed)

**Total Estimated Changes**: ~650 lines

---

## Dependencies

### Required Libraries
- **OpenGL 3.3+**: For modern shader pipeline
- **GLM**: For matrix mathematics (glm::mat4, glm::vec3, etc.)
- **Existing**: SDL2, OpenGL ES (already in project)

### Check GLM Availability
```bash
# Check if GLM is available
find /usr/include -name "glm.hpp" 2>/dev/null

# If not found, add to WORKSPACE:
http_archive(
    name = "glm",
    urls = ["https://github.com/g-truc/glm/archive/0.9.9.8.tar.gz"],
    strip_prefix = "glm-0.9.9.8",
    build_file = "//third_party:glm.BUILD",
)
```

---

## Timeline

| Phase | Task | Duration | Status |
|-------|------|----------|--------|
| 1 | Create GLSL shaders | 20 min | ⏳ |
| 1 | Implement ShaderProgram class | 25 min | ⏳ |
| 2 | Add 3D state to RenderManager | 20 min | ⏳ |
| 2 | Implement matrix calculations | 25 min | ⏳ |
| 3 | Implement Render3DModel | 30 min | ⏳ |
| 3 | Update FrameProcessor | 15 min | ⏳ |
| 4 | Update BUILD files | 10 min | ⏳ |
| 4 | Testing and debugging | 20 min | ⏳ |
| **Total** | | **2h 45m** | |

---

## Risk Mitigation

**Risk 1: OpenGL Context Issues**
- Mitigation: RenderManager already manages SDL/OpenGL context
- Test: Verify context is current before shader operations

**Risk 2: Shader Compilation Failures**
- Mitigation: Comprehensive error logging
- Test: Check compilation logs, start with simple shaders

**Risk 3: Matrix Math Errors**
- Mitigation: Use proven GLM library
- Test: Print matrices, verify with known values

**Risk 4: Performance Impact**
- Mitigation: Minimize state changes, batch rendering
- Test: Profile with 3+ models, optimize if needed

---

## Next Steps After Completion

1. **Texture Mapping** (Phase 4)
   - Load textures from MTL files
   - Sample in fragment shader
   - UV coordinate support

2. **Advanced Lighting**
   - Point lights
   - Spot lights
   - Multiple light sources

3. **Shadows**
   - Shadow mapping
   - Soft shadows

4. **Post-Processing**
   - Bloom
   - Anti-aliasing (MSAA)
   - Color grading

---

**Ready to implement! Let's start with Phase 1: Shader System** 🚀

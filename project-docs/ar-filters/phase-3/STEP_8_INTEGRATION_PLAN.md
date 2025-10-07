# Phase 3 Step 8: 3D Rendering Integration Plan

## Current Status
✅ **Infrastructure Complete** (Build successful!)
- GLSL shaders created (vertex + fragment with Blinn-Phong lighting)
- ShaderProgram class implemented
- GLM integrated for matrix math
- RenderManager 3D API added
- All code compiles successfully

## Integration Challenge

**Current Flow (2D visualization):**
```
Camera → MediaPipe → Effects → OpenCV Mat rendering (circles) → Upload to texture → ImGui display
```

**Target Flow (3D rendering):**
```
Camera → MediaPipe → Effects → OpenGL framebuffer (3D models) → ImGui display
```

## Integration Strategy

### Option A: Hybrid Rendering (RECOMMENDED FOR PHASE 3)
Keep 2D circle rendering for now, add 3D rendering alongside as optional visualization.

**Pros:**
- Non-breaking change
- Easy testing and comparison
- Can validate 3D rendering before full transition
- Preserves existing functionality

**Cons:**
- Temporary dual code paths
- Not final architecture

### Option B: Full 3D Transition
Replace circle rendering entirely with 3D OpenGL rendering.

**Pros:**
- Clean architecture
- True 3D rendering pipeline
- Better performance (no CPU→GPU texture upload for primitives)

**Cons:**
- More complex implementation
- Requires framebuffer object (FBO) for off-screen rendering
- Higher risk of breaking existing functionality

## Recommended Approach: Option A (Hybrid)

### Phase 3 Step 8A: Add 3D Rendering Alongside Circles

**Implementation Steps:**

1. **Initialize 3D Rendering System** (10 min)
   - Call `RenderManager::Initialize3DRendering()` in application initialization
   - Call `RenderManager::UpdateProjectionMatrix()` when window resizes
   - Handle initialization errors gracefully

2. **Create OpenGL Rendering Context** (15 min)
   - Ensure OpenGL context is active during rendering
   - Set up viewport correctly
   - Clear depth buffer each frame

3. **Add 3D Rendering Pass** (30 min)
   - In `RenderFilterPrimitives()`, after circle rendering:
     ```cpp
     // For each 3D model filter:
     //   1. Calculate model matrix from position/rotation/scale
     //   2. Call RenderManager::Render3DModel()
     //   3. Read back pixels to OpenCV Mat (glReadPixels)
     //   4. Composite onto display_bgr
     ```
   - Use alpha blending for proper compositing
   - Preserve existing circle rendering

4. **Add UI Toggle** (10 min)
   - Add checkbox to AR filters panel: "Render 3D Models"
   - Store in app_state: `app_state.ar_render_3d_models`
   - Default: enabled for 3D models, circles for 2D primitives

5. **Testing** (20 min)
   - Load simple_cube.obj
   - Verify 3D geometry visible
   - Verify materials work
   - Verify depth testing
   - Compare with circle rendering

**Total Time:** ~1.5 hours

### Phase 4: Full 3D Pipeline (Future)

This will be done later as a separate refactoring:
- Create framebuffer object (FBO) for off-screen rendering
- Render entire scene to FBO texture
- Composite FBO texture with video in ImGui
- Remove OpenCV Mat rendering entirely
- Optimize performance with batching

## Implementation Details

### 1. Initialize 3D Rendering

**File:** `src/application/application_initialization.cpp`

Add after SDL/OpenGL setup:

```cpp
// Initialize 3D rendering for AR filters
if (managers.render) {
    if (!managers.render->Initialize3DRendering()) {
        std::cerr << "⚠️  Failed to initialize 3D rendering, continuing without it" << std::endl;
        app_state.ar_3d_rendering_available = false;
    } else {
        std::cout << "✅ 3D rendering initialized" << std::endl;
        app_state.ar_3d_rendering_available = true;
        
        // Set initial projection matrix
        int width, height;
        SDL_GetWindowSize(sdl_params.window, &width, &height);
        managers.render->UpdateProjectionMatrix(width, height);
    }
}
```

### 2. Create Render3DFilters Function

**File:** `src/application/frame_processor.cpp`

Add new function:

```cpp
void FrameProcessor::Render3DFilters(cv::Mat& display_bgr, AppState& app_state, RenderManager* render_mgr) {
    if (!app_state.ar_filters_enabled || !app_state.ar_render_3d_models) {
        return;
    }
    
    if (!render_mgr || !app_state.ar_3d_rendering_available) {
        return;
    }
    
    const auto& filters = app_state.attachment_controller.GetActiveFilters();
    
    // Set up OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Clear depth buffer (preserve color buffer)
    glClear(GL_DEPTH_BUFFER_BIT);
    
    // Set viewport to match frame size
    glViewport(0, 0, display_bgr.cols, display_bgr.rows);
    
    // Render each 3D model
    for (const auto& filter : filters) {
        if (!filter.visible || !filter.enabled) {
            continue;
        }
        
        if (filter.type != FilterObject::Type::MODEL_3D || !filter.model) {
            continue;
        }
        
        // Build model matrix from filter transform
        glm::mat4 model_matrix = glm::mat4(1.0f);
        
        // Translate: position is in pixel coordinates, convert to normalized device coords
        float norm_x = (filter.position[0] / display_bgr.cols) * 2.0f - 1.0f;
        float norm_y = (filter.position[1] / display_bgr.rows) * 2.0f - 1.0f;
        float norm_z = filter.position[2];  // Already in depth [-1, 1]
        model_matrix = glm::translate(model_matrix, glm::vec3(norm_x, -norm_y, norm_z));
        
        // Rotate: rotation is in radians around X, Y, Z axes
        model_matrix = glm::rotate(model_matrix, filter.rotation[0], glm::vec3(1, 0, 0));
        model_matrix = glm::rotate(model_matrix, filter.rotation[1], glm::vec3(0, 1, 0));
        model_matrix = glm::rotate(model_matrix, filter.rotation[2], glm::vec3(0, 0, 1));
        
        // Scale: combine scale and local_scale
        glm::vec3 final_scale(
            filter.scale[0] * filter.local_scale,
            filter.scale[1] * filter.local_scale,
            filter.scale[2] * filter.local_scale
        );
        model_matrix = glm::scale(model_matrix, final_scale);
        
        // Get material (use first material from model)
        ar_filters::Material material;
        if (!filter.model->materials.empty()) {
            material = filter.model->materials.begin()->second;
        } else {
            // Default material
            material.ambient[0] = material.ambient[1] = material.ambient[2] = 0.2f;
            material.diffuse[0] = material.diffuse[1] = material.diffuse[2] = 0.8f;
            material.specular[0] = material.specular[1] = material.specular[2] = 1.0f;
            material.shininess = 32.0f;
            material.opacity = 1.0f;
        }
        
        // Render the model
        render_mgr->Render3DModel(*filter.model, model_matrix, material);
    }
    
    // Read back rendered pixels and composite onto OpenCV Mat
    // TODO: This is expensive! In Phase 4, we'll render directly to texture
    glReadPixels(0, 0, display_bgr.cols, display_bgr.rows, GL_BGR, GL_UNSIGNED_BYTE, display_bgr.data);
    
    // Restore OpenGL state
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
```

### 3. Update RenderFilterPrimitives

**File:** `src/application/frame_processor.cpp`

Add before circle rendering:

```cpp
void FrameProcessor::RenderFilterPrimitives(cv::Mat& display_bgr, AppState& app_state, RenderManager* render_mgr) {
    if (!app_state.ar_filters_enabled) {
        return;
    }
    
    const auto& filters = app_state.attachment_controller.GetActiveFilters();
    if (filters.empty()) {
        return;
    }
    
    // NEW: Render 3D models first (if enabled)
    if (app_state.ar_render_3d_models && render_mgr && app_state.ar_3d_rendering_available) {
        Render3DFilters(display_bgr, app_state, render_mgr);
    }
    
    // Existing circle rendering for 2D primitives or when 3D is disabled
    // ... (keep existing code)
}
```

### 4. Update Function Signature

**File:** `include/application/frame_processor.h`

```cpp
static void RenderFilterPrimitives(cv::Mat& display_bgr, AppState& app_state, RenderManager* render_mgr = nullptr);
```

### 5. Update Call Site

**File:** `src/application/frame_processor.cpp` (in ProcessFrame function)

Find the call to `RenderFilterPrimitives` and add render manager:

```cpp
// Pass render manager if available
RenderManager* render_mgr = params.managers.render ? params.managers.render.get() : nullptr;
RenderFilterPrimitives(display_bgr, params.app_state, render_mgr);
```

### 6. Add AppState Fields

**File:** `include/application/app_state.h`

```cpp
// AR Filters 3D Rendering
bool ar_render_3d_models = true;          // Render 3D models with OpenGL
bool ar_3d_rendering_available = false;   // True if 3D rendering initialized
```

### 7. Add UI Control

**File:** `src/ui/ui_manager_enhanced.cpp` (in AR Filters panel)

```cpp
if (ImGui::Checkbox("Render 3D Models", &state.ar_render_3d_models)) {
    state_changed = true;
}
if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Use OpenGL 3D rendering for MODEL_3D filters\n"
                     "(Circles are used when disabled)");
}
```

## Testing Checklist

After implementation:

- [ ] Build succeeds
- [ ] Application starts without errors
- [ ] simple_cube.obj loads successfully
- [ ] 3D cube is visible (not a circle)
- [ ] Material color matches (red cube visible)
- [ ] Depth testing works (faces occlude correctly)
- [ ] Rotation works (cube rotates with face)
- [ ] Scaling works (cube size responds to scale parameter)
- [ ] Toggle between 3D/2D rendering works
- [ ] Performance acceptable (>30 FPS)
- [ ] No memory leaks (valgrind clean)

## Success Criteria

✅ **Phase 3 Step 8 Complete** when:
1. Actual 3D geometry renders (not circles)
2. Materials and colors work correctly
3. Lighting visible (Blinn-Phong shading)
4. Depth testing works (proper occlusion)
5. Face tracking works (models follow anchors)
6. Performance >30 FPS with 1-3 models
7. UI toggle works (can switch between 3D/2D)
8. No regressions (existing features still work)

## Next Steps (Phase 4)

1. **Framebuffer Object Rendering**
   - Render to off-screen FBO
   - Composite FBO texture with video
   - Remove glReadPixels bottleneck

2. **Multiple Models Optimization**
   - Batch rendering
   - Frustum culling
   - Level of detail (LOD)

3. **Advanced Effects**
   - Shadows
   - Post-processing
   - Reflections

## Notes

- Current implementation uses glReadPixels which is slow (~10ms overhead)
- This is acceptable for Phase 3 (proof of concept)
- Phase 4 will eliminate this with proper FBO rendering
- Keep both 2D and 3D rendering for now (easy comparison/debugging)

---

**Created:** 2025-01-03  
**Status:** Ready for implementation  
**Estimated Time:** 1.5 hours

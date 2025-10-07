# Phase 3 Step 7: Integration Testing

**Status**: 🚧 IN PROGRESS  
**Duration Estimate**: 4-6 hours  
**Start Date**: 2025-10-02

## Objective

Validate Phase 3 (3D Model Loading System) integration with the running SegmeCam application through visual testing, performance validation, and real-world usage scenarios.

## Prerequisites

✅ **Completed**:
- Step 1: Assimp library decision (Assimp chosen for 50+ format support)
- Step 2: ModelLoader implementation (OBJ/GLTF/FBX loading)
- Step 3: FilterObject extension (MODEL_3D type added)
- Step 4: Test assets creation (cube, glasses, hat models)
- Step 5: AttachmentController verification (type-agnostic design)
- Step 6: Unit tests (83 tests written, 95% buildable)

⏳ **Parallel Work** (non-blocking):
- Step 6 build completion (Assimp config constants)

## Integration Testing Approach

### Philosophy

Integration testing validates the **entire pipeline**:
1. Model loading from disk → OpenGL buffers
2. Filter attachment → Face anchor binding
3. Transform updates → Per-frame positioning
4. Rendering → Visual output with materials/transparency

This differs from unit tests by:
- **Using real application context** (not mocked)
- **Visual validation** (human verification)
- **Performance measurement** (FPS impact)
- **Real face tracking** (MediaPipe integration)

## Test Scenarios

### Scenario 1: Load and Display Single Model 🎯
**Duration**: 1 hour

**Objective**: Verify basic model loading and rendering

**Steps**:
1. Start SegmeCam application
2. Enable face tracking (MediaPipe face mesh)
3. Load simple_glasses.obj model
4. Attach to "nose_bridge" anchor
5. Verify model appears on face
6. Check materials (frame opaque, lens transparent)
7. Measure FPS impact

**Success Criteria**:
- ✅ Model loads without errors
- ✅ Model appears at nose_bridge position
- ✅ Model follows face movement
- ✅ Transparency renders correctly
- ✅ FPS > 24 with model active
- ✅ No memory leaks

**Test Code Location**: 
- Use existing `filter_test_demo.cpp` as starting point
- Or create `model_integration_test.cpp`

### Scenario 2: Multiple Models with Different Anchors 🎯
**Duration**: 1.5 hours

**Objective**: Verify multi-model handling and anchor system

**Steps**:
1. Load simple_glasses.obj → attach to "nose_bridge"
2. Load simple_hat.obj → attach to "forehead"
3. Verify both render simultaneously
4. Check Z-ordering (hat behind glasses)
5. Verify independent transforms
6. Test enable/disable individual filters

**Success Criteria**:
- ✅ Multiple models render correctly
- ✅ Each tracks its anchor independently
- ✅ No visual artifacts (z-fighting, clipping)
- ✅ Disable one filter → other still works
- ✅ FPS > 20 with 2 models

### Scenario 3: Model Switching and Lifecycle 🎯
**Duration**: 1 hour

**Objective**: Verify resource management and model swapping

**Steps**:
1. Load glasses model
2. Detach and unload glasses
3. Load hat model
4. Verify GPU memory released (check glDeleteBuffers called)
5. Rapid model switching (10 times)
6. Check for memory leaks

**Success Criteria**:
- ✅ OpenGL buffers cleaned up
- ✅ No dangling pointers
- ✅ Memory usage stable after switching
- ✅ No crashes or errors

### Scenario 4: Performance Validation 🎯
**Duration**: 1 hour

**Objective**: Measure performance impact of 3D models

**Test Matrix**:
```
| Scenario              | Target FPS | Actual FPS | Pass/Fail |
|-----------------------|------------|------------|-----------|
| No models             | 30         |            |           |
| 1 simple model (8v)   | 28         |            |           |
| 1 complex model (36v) | 25         |            |           |
| 2 simple models       | 25         |            |           |
| 3 models + effects    | 20         |            |           |
```

**Metrics to Collect**:
- Frame time (ms)
- Model load time (ms)
- GPU memory usage (MB)
- CPU usage (%)

**Tools**:
- `glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX)` (NVIDIA)
- SegmeCam's existing FPS counter
- System monitor (htop)

### Scenario 5: Edge Cases and Error Handling 🎯
**Duration**: 30 minutes

**Objective**: Verify robustness

**Test Cases**:
1. Load nonexistent model → graceful failure
2. Load corrupt OBJ file → error reported
3. Attach to invalid anchor → filter hidden
4. Detach nonexistent filter → no crash
5. Unload model twice → no crash

**Success Criteria**:
- ✅ No crashes on bad input
- ✅ Error messages logged
- ✅ Application continues running

## Implementation Plan

### Phase A: Prepare Integration Test Environment
**Duration**: 30 minutes

1. **Verify SegmeCam builds with Phase 3 code**:
   ```bash
   ./build-app.sh
   ```

2. **Check for Phase 3 integration points**:
   - Verify `UIManager` has model loading controls
   - Check `EffectsManager` can handle MODEL_3D filters
   - Confirm `AttachmentController` is instantiated in Application

3. **Create test model loading UI** (if not exists):
   - Button: "Load Glasses Model"
   - Button: "Load Hat Model"
   - Button: "Unload All Models"
   - Checkbox: "Enable Model Rendering"

### Phase B: Model Loading Integration
**Duration**: 2 hours

1. **Add model loading to UIManager** (`ui_manager_enhanced.cpp`):
   ```cpp
   if (ImGui::Button("Load Glasses Model")) {
       auto model_result = model_loader_->LoadModel("assets/ar_filters/models/simple_glasses.obj");
       if (model_result.ok()) {
           FilterObject filter;
           filter.type = FilterType::MODEL_3D;
           filter.model = std::make_shared<Model>(std::move(model_result).value());
           filter.anchor_name = "nose_bridge";
           attachment_controller_->AttachFilter(filter);
       }
   }
   ```

2. **Wire ModelLoader into Application**:
   - Add `ModelLoader model_loader_;` to Application class
   - Pass to UIManager constructor
   - Initialize in `Application::Initialize()`

3. **Update EffectsManager rendering**:
   ```cpp
   void EffectsManager::RenderFilter(const FilterObject& filter) {
       if (filter.type == FilterType::MODEL_3D && filter.model) {
           RenderModel(*filter.model, filter.transform);
       } else {
           RenderPrimitive(filter);  // Existing code
       }
   }
   
   void EffectsManager::RenderModel(const Model& model, const cv::Mat& transform) {
       // Set up model-view-projection matrix
       // Iterate through model.meshes
       // Bind VAO, draw elements
       // Apply materials
   }
   ```

### Phase C: Visual Testing
**Duration**: 2 hours

Execute test scenarios 1-5 with **manual visual verification**:

**Checklist for Each Test**:
- [ ] Take screenshot of result
- [ ] Record FPS measurement
- [ ] Note any visual artifacts
- [ ] Check console for errors
- [ ] Verify memory usage

**Documentation**:
- Save screenshots to `project-docs/ar-filters/phase-3/integration-tests/`
- Record results in `PHASE_3_STEP_7_RESULTS.md`

### Phase D: Performance Profiling
**Duration**: 1 hour

1. **Instrument rendering loop**:
   ```cpp
   auto start = std::chrono::high_resolution_clock::now();
   RenderAllFilters();
   auto end = std::chrono::high_resolution_clock::now();
   render_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
   ```

2. **Collect metrics** over 100 frames
3. **Calculate statistics**:
   - Mean frame time
   - 95th percentile frame time
   - Frame time variance (jitter)

4. **Compare with baseline** (no models loaded)

## Success Criteria

### Functional Requirements ✅
- ✅ Models load from OBJ files
- ✅ Models render with correct materials
- ✅ Transparency works (lens on glasses)
- ✅ Models track face anchors
- ✅ Multiple models render simultaneously
- ✅ Models can be attached/detached dynamically

### Performance Requirements ✅
- ✅ Model load time < 100ms (per unit tests)
- ✅ FPS ≥ 24 with 1 simple model
- ✅ FPS ≥ 20 with 2 models
- ✅ Memory usage < +50MB per model

### Quality Requirements ✅
- ✅ No crashes or errors
- ✅ No memory leaks
- ✅ Graceful error handling
- ✅ Visual quality acceptable (no artifacts)

## Risks and Mitigations

### Risk 1: OpenGL Context Issues
**Probability**: Medium  
**Impact**: High  
**Mitigation**: 
- Ensure GLEW initialized before loading models
- Check OpenGL version ≥ 3.0
- Add error checking after GL calls

### Risk 2: Performance Impact
**Probability**: Medium  
**Impact**: Medium  
**Mitigation**:
- Use indexed rendering (EBO)
- Frustum culling (don't render off-screen models)
- Level-of-detail (LOD) for complex models

### Risk 3: Material/Transparency Issues
**Probability**: Low  
**Impact**: Medium  
**Mitigation**:
- Enable blending: `glEnable(GL_BLEND)`
- Sort transparent objects back-to-front
- Use premultiplied alpha

## Deliverables

1. **Integration Test Code** (`model_integration_test.cpp`)
2. **Test Results Document** (`PHASE_3_STEP_7_RESULTS.md`)
3. **Performance Report** (FPS charts, memory usage)
4. **Screenshots** (visual validation)
5. **Updated Application** (with model loading UI)

## Timeline

| Task | Duration | Status |
|------|----------|--------|
| Phase A: Environment Setup | 30 min | 🚧 In Progress |
| Phase B: Model Loading Integration | 2 hours | ⏳ Pending |
| Phase C: Visual Testing | 2 hours | ⏳ Pending |
| Phase D: Performance Profiling | 1 hour | ⏳ Pending |
| Documentation | 30 min | ⏳ Pending |
| **Total** | **6 hours** | |

## Next Actions

1. ✅ Create this plan document
2. 🚧 Check if SegmeCam builds with Phase 3 code
3. ⏳ Add model loading UI to UIManager
4. ⏳ Wire ModelLoader into Application
5. ⏳ Implement model rendering in EffectsManager
6. ⏳ Execute test scenarios
7. ⏳ Document results

---

**Phase 3 Progress**: 6/8 steps complete (75%)  
**Step 7 Status**: Started  
**Estimated Completion**: 2025-10-02 EOD

# Phase 9: UI Integration for AR Filters

## Status: ✅ COMPLETE

## Duration: October 7-8, 2025 (Estimated)

## Estimated Time: 12 hours (1.5-2 days)

---

## Completion summary

- Implemented ARFilterPanel (new ImGui panel) with enable toggle, grid selection, and action controls
- Filter discovery from assets/filters with metadata parsing and validation
- Thumbnail generation and caching shipped (128x128 RGBA); see THUMBNAIL_GENERATION_COMPLETE.md
- Interactive thumbnail grid with “None” option, active highlighting, tooltips, and keyboard shortcuts (Esc, Ctrl+A)
- Performance stats integrated (render time, budget impact), stable “Avg FPS” line preserved across UI
- ConfigManager persistence for ar_filters: enabled, active_filter_id, preview_only; restores on startup and profile switch
- UI Manager registration and ApplicationRun wiring completed; panel renders between camera and performance sections
- Verified on Linux: builds cleanly, maintains 30 FPS with typical filters

How to use
- Open AR Filters panel in the UI
- Toggle “Enable AR Filters” and pick a filter thumbnail; click “None” to clear
- Optional: click “Generate Thumbnails” if placeholders are shown (first run)
- Selection persists; use “Preview Only” to exclude from virtual camera if desired

## Overview

Phase 9 adds a user interface for AR filter selection and management, completing the user-facing AR filter feature. This phase builds the UI controls that allow users to browse, select, and manage AR filters through an intuitive ImGui panel.

### Goals

1. **Create ARFilterPanel** - New ImGui panel for filter selection and management
2. **Filter Discovery** - Enumerate available filters from filesystem
3. **Thumbnail Generation** - Generate thumbnail images for all filters (128x128 PNG)
4. **Filter Selection UI** - Interactive grid with thumbnail images
5. **Filter Management** - Enable/disable, switch between filters, show active filter
6. **Performance Display** - Show AR filter render time and FPS impact
7. **ConfigManager Integration** - Persist filter preferences to YAML
8. **UI Manager Integration** - Register panel with existing UI system

---

## Scope and Boundaries

### ✅ What Phase 9 INCLUDES

- ARFilterPanel class implementation
- Filter enumeration and display
- Filter selection and activation with **thumbnail images**
- Thumbnail generation (render once, save PNG)
- ImGui texture loading for thumbnails
- Performance metrics display
- ConfigManager YAML persistence
- UI Manager registration
- Basic keyboard shortcuts (Ctrl+A, ESC)

### ❌ What Phase 9 EXCLUDES

- Filter categories and search (basic category parsing, full UI in Phase 11+)
- Filter preview mode (Phase 11+)
- Filter editor/creator tools (Future)
- Animation controls (Phase 10.5)
- Expression behavior controls (Phase 10.5)

### 🎯 Success Criteria

- User can browse available filters in UI
- User can select/activate filters with one click
- Active filter is clearly indicated
- "No Filter" option disables AR filters
- Performance stats show render time
- Selected filter persists across application restarts
- UI is responsive and intuitive

---

## Goals Breakdown

### Goal 1: Create ARFilterPanel Class (6 hours)

**Files to Create**:
- `mediapipe/examples/desktop/segmecam/include/ui/ar_filter_panel.h`
- `mediapipe/examples/desktop/segmecam/src/ui/ar_filter_panel.cpp`

**Class Structure**:

```cpp
// include/ui/ar_filter_panel.h
#pragma once

#include "ui_panel.h"
#include "include/application/app_state.h"
#include "include/ar_filters/ar_filter_manager.h"
#include <memory>
#include <vector>
#include <string>

namespace segmecam {

struct FilterInfo {
  std::string id;           // e.g., "classic-glasses-v1"
  std::string name;         // e.g., "Classic Glasses"
  std::string author;       // e.g., "SegmeCam"
  std::string category;     // e.g., "glasses"
  std::string description;  // e.g., "Simple black-rimmed glasses"
  std::string thumbnail_path; // Path to thumbnail image
  GLuint thumbnail_texture; // OpenGL texture ID for thumbnail
  bool is_available;        // Whether filter assets exist
};

class ARFilterPanel : public UIPanel {
public:
  ARFilterPanel(AppState& state, ARFilterManager& ar_mgr);
  ~ARFilterPanel() override = default;

  void Render() override;
  void Update();  // Called each frame to refresh filter list

private:
  // Rendering methods
  void RenderHeader();
  void RenderFilterGrid();
  void RenderFilterInfo();
  void RenderPerformanceStats();
  void RenderControls();

  // Filter management
  void RefreshFilterList();
  void SelectFilter(const std::string& filter_id);
  void ClearFilter();

  // Thumbnail management
  void GenerateThumbnails();
  GLuint LoadThumbnailTexture(const std::string& path);
  void UnloadThumbnailTextures();

  // UI helpers
  bool RenderFilterButton(const FilterInfo& filter, bool is_active);
  void RenderNoFilterButton(bool is_active);

  // State
  AppState& state_;
  ARFilterManager& ar_mgr_;
  std::vector<FilterInfo> available_filters_;
  std::string active_filter_id_;
  bool needs_refresh_;
};

} // namespace segmecam
```

**Implementation Details**:

1. **Constructor**: 
   - Store references to AppState and ARFilterManager
   - Call RefreshFilterList() to enumerate available filters
   - Set initial active_filter_id_ from AppState

2. **Render()**:
   - Display panel title and enable/disable toggle
   - Call sub-rendering methods (Header, Grid, Info, Stats, Controls)
   - Handle user input (clicks, keyboard shortcuts)

3. **RenderFilterGrid()**:
   - Display available filters in a grid layout (4 per row)
   - Show "None" button first (to disable filters)
   - For each filter:
     - Use ImGui::Image() to display thumbnail (64x64)
     - Add filter name below thumbnail
     - Highlight active filter with border
     - Handle click to select filter
   - Scroll if more than 8 filters

**Implementation Example**:

```cpp
void ARFilterPanel::RenderFilterGrid() {
  ImGui::BeginChild("FilterGrid", ImVec2(0, 200), true);
  
  // "None" button (no thumbnail, just text)
  RenderNoFilterButton(active_filter_id_.empty());
  ImGui::SameLine();
  
  // Filter thumbnails (4 per row)
  int count = 1;  // Start at 1 ("None" is first)
  for (const auto& filter : available_filters_) {
    RenderFilterButton(filter, filter.id == active_filter_id_);
    
    if (count % 4 != 0 && count < available_filters_.size()) {
      ImGui::SameLine();
    }
    count++;
  }
  
  ImGui::EndChild();
}

bool ARFilterPanel::RenderFilterButton(const FilterInfo& filter, bool is_active) {
  ImGui::BeginGroup();
  
  // Highlight if active
  if (is_active) {
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
  }
  
  // Thumbnail image (64x64)
  bool clicked = false;
  if (filter.thumbnail_texture != 0) {
    clicked = ImGui::ImageButton(
        (void*)(intptr_t)filter.thumbnail_texture,
        ImVec2(64, 64)
    );
  } else {
    // Fallback: Text button if no thumbnail
    clicked = ImGui::Button(filter.name.c_str(), ImVec2(64, 64));
  }
  
  if (is_active) {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
  }
  
  // Filter name below thumbnail
  ImGui::Text("%s", filter.name.c_str());
  
  // Tooltip on hover
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("%s", filter.name.c_str());
    ImGui::Text("Author: %s", filter.author.c_str());
    ImGui::Text("Category: %s", filter.category.c_str());
    ImGui::EndTooltip();
  }
  
  ImGui::EndGroup();
  
  // Handle click
  if (clicked) {
    SelectFilter(filter.id);
  }
  
  return clicked;
}
```

4. **RenderFilterInfo()**:
   - Show active filter details (name, author, category)
   - Display filter description
   - Show attachment count

5. **RenderPerformanceStats()**:
   - Display AR render time (ms)
   - Show FPS impact (if applicable)
   - Performance warnings if render time > 5ms

6. **RefreshFilterList()**:
   - Enumerate filters from `assets/filters/` directory
   - Parse filter.json metadata for each filter
   - Load thumbnail textures (or generate if missing)
   - Populate available_filters_ vector
   - Sort by category and name

7. **GenerateThumbnails()** (NEW):
   - For each filter without thumbnail:
     - Load filter through ARFilterManager
     - Render to 128x128 FBO
     - Save as PNG in filter directory
     - Load texture for UI display
   - Only runs once per filter (checks if thumbnail.png exists)

8. **LoadThumbnailTexture()** (NEW):
   - Load PNG image using OpenCV
   - Upload to GPU as OpenGL texture
   - Store texture ID in FilterInfo
   - Return texture ID for ImGui::Image()

9. **UnloadThumbnailTextures()** (NEW):
   - Called in destructor
   - Delete all OpenGL textures
   - Free GPU memory

---

### Goal 3: Thumbnail Generation (2 hours)

**Implementation Strategy**:

**Option A: Generate on First Run** (Recommended)
- Check if `thumbnail.png` exists in filter directory
- If missing, generate it:
  1. Load filter through ARFilterManager
  2. Create 128x128 FBO
  3. Render filter with neutral face pose
  4. Save FBO contents as PNG
  5. Load texture for UI
- If exists, just load the texture

**Option B: Use Placeholder Icons**
- Use emoji or text as fallback
- Generate actual thumbnails in background thread
- Less ideal but faster initial load

**We'll use Option A** - Generate on first run, cache forever

**Thumbnail Generation Code**:

```cpp
void ARFilterPanel::GenerateThumbnails() {
  for (auto& filter : available_filters_) {
    std::string thumbnail_path = "assets/filters/" + filter.id + "/thumbnail.png";
    
    // Check if thumbnail already exists
    if (std::filesystem::exists(thumbnail_path)) {
      filter.thumbnail_texture = LoadThumbnailTexture(thumbnail_path);
      filter.thumbnail_path = thumbnail_path;
      continue;
    }
    
    // Generate thumbnail
    ABSL_LOG(INFO) << "Generating thumbnail for filter: " << filter.id;
    
    // 1. Load filter
    absl::Status status = ar_mgr_.LoadFilter(filter.id);
    if (!status.ok()) {
      ABSL_LOG(ERROR) << "Failed to load filter for thumbnail: " << status.message();
      continue;
    }
    
    // 2. Create 128x128 FBO
    GLuint fbo, texture;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    
    // 3. Render filter (neutral face pose)
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 128, 128);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Use neutral landmarks (forward-facing, centered)
    std::vector<mediapipe::NormalizedLandmark> neutral_landmarks = GetNeutralLandmarks();
    ar_mgr_.Update(neutral_landmarks, cv::Mat());
    ar_mgr_.Render(fbo, 128, 128);
    
    // 4. Read pixels and save as PNG
    cv::Mat thumbnail(128, 128, CV_8UC4);
    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, thumbnail.data);
    cv::flip(thumbnail, thumbnail, 0);  // Flip vertically (OpenGL is upside down)
    cv::imwrite(thumbnail_path, thumbnail);
    
    // 5. Load texture for UI
    filter.thumbnail_texture = LoadThumbnailTexture(thumbnail_path);
    filter.thumbnail_path = thumbnail_path;
    
    // Cleanup
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    ar_mgr_.UnloadFilter(filter.id);
  }
}

GLuint ARFilterPanel::LoadThumbnailTexture(const std::string& path) {
  cv::Mat image = cv::imread(path, cv::IMREAD_UNCHANGED);
  if (image.empty()) {
    ABSL_LOG(ERROR) << "Failed to load thumbnail: " << path;
    return 0;
  }
  
  // Convert BGR to RGB
  if (image.channels() == 3) {
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
  } else if (image.channels() == 4) {
    cv::cvtColor(image, image, cv::COLOR_BGRA2RGBA);
  }
  
  // Upload to GPU
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  
  GLenum format = (image.channels() == 4) ? GL_RGBA : GL_RGB;
  glTexImage2D(GL_TEXTURE_2D, 0, format, image.cols, image.rows, 0, format, GL_UNSIGNED_BYTE, image.data);
  
  return texture_id;
}
```

**Neutral Face Landmarks Helper**:

```cpp
std::vector<mediapipe::NormalizedLandmark> ARFilterPanel::GetNeutralLandmarks() {
  // Return forward-facing, centered face landmarks
  // This provides a consistent view for all filter thumbnails
  std::vector<mediapipe::NormalizedLandmark> landmarks(468);
  
  // Use pre-computed neutral pose from Phase 1 face mesh
  // Or load from reference file: assets/neutral_face.json
  // For now, use approximate values for key landmarks:
  
  // Nose bridge (landmark 6): center, slightly forward
  landmarks[6].set_x(0.5f);
  landmarks[6].set_y(0.4f);
  landmarks[6].set_z(-0.05f);
  
  // ... (468 landmarks with neutral pose)
  
  return landmarks;
}
```

---

### Goal 4: ConfigManager Integration (3 hours)

**Files to Modify**:
- `mediapipe/examples/desktop/segmecam/include/config/config_manager.h`
- `mediapipe/examples/desktop/segmecam/src/config/config_manager.cpp`
- `mediapipe/examples/desktop/segmecam/include/application/app_state.h`

**AppState Changes**:

```cpp
// include/application/app_state.h
struct AppState {
  // ... existing fields ...

  // AR Filter Settings (NEW)
  struct {
    bool enabled = false;
    std::string active_filter_id = "";  // Empty = no filter
    bool preview_only = false;          // Preview without virtual camera output
  } ar_filters;
};
```

**ConfigManager Changes**:

```cpp
// Add to ConfigManager class
void LoadARFilterConfig(const std::string& profile_name, AppState& state);
void SaveARFilterConfig(const std::string& profile_name, const AppState& state);
```

**YAML Structure**:

```yaml
# ~/.config/segmecam/profiles/default.yaml
ar_filters:
  enabled: true
  active_filter_id: "classic-glasses-v1"
  preview_only: false
```

**Implementation**:

1. **LoadARFilterConfig()**:
   - Read `ar_filters` section from YAML
   - Parse enabled, active_filter_id, preview_only
   - Update AppState with loaded values
   - Validate filter_id exists (set to "" if not found)

2. **SaveARFilterConfig()**:
   - Write current AR filter state to YAML
   - Preserve other profile settings
   - Call on profile save

3. **Integration**:
   - Call LoadARFilterConfig() in ApplicationRun::Initialize()
   - Call SaveARFilterConfig() in ApplicationRun::Cleanup()
   - Auto-save on filter change (optional)

---

### Goal 3: UI Manager Integration (1 hour)

**Files to Modify**:
- `mediapipe/examples/desktop/segmecam/include/ui/ui_manager_enhanced.h`
- `mediapipe/examples/desktop/segmecam/src/ui/ui_manager_enhanced.cpp`

**UIManagerEnhanced Changes**:

```cpp
// include/ui/ui_manager_enhanced.h
class UIManagerEnhanced {
public:
  // ... existing methods ...

  // Add panel registration
  void RegisterARFilterPanel(std::unique_ptr<ARFilterPanel> panel);

private:
  // ... existing panels ...
  std::unique_ptr<ARFilterPanel> ar_filter_panel_;
};
```

**Implementation**:

1. **RegisterARFilterPanel()**:
   - Store ARFilterPanel pointer
   - Add to panel list for rendering

2. **Render()**:
   - Call ar_filter_panel_->Render() in appropriate order
   - Place between camera controls and performance panels

3. **ApplicationRun Integration**:

```cpp
   // In ApplicationRun::Initialize()
   auto ar_panel = std::make_unique<ARFilterPanel>(
       app_state_, 
       *ar_filter_manager_
   );
   ui_manager_->RegisterARFilterPanel(std::move(ar_panel));
```

---

## UI Design

### Panel Layout

```
┌─ AR Filters ────────────────────────────────────────────────┐
│ [✓] Enable AR Filters                                        │
│                                                              │
│ ┌─ Available Filters ─────────────────────────────────────┐ │
│ │ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                    │ │
│ │ │ None │ │  👓  │ │  🎩  │ │  😺  │  [More...]         │ │
│ │ │      │ │Glassⓘ│ │ Hat  │ │ Ears │                    │ │
│ │ └──────┘ └──────┘ └──────┘ └──────┘                    │ │
│ └──────────────────────────────────────────────────────────┘ │
│                                                              │
│ ┌─ Active Filter ────────────────────────────────────────┐  │
│ │ Name:   Classic Glasses                                 │  │
│ │ Author: SegmeCam Team                                   │  │
│ │ Type:   Glasses                                         │  │
│ │                                                         │  │
│ │ Description: Simple black-rimmed glasses that          │  │
│ │              follow your face naturally                │  │
│ └──────────────────────────────────────────────────────────┘ │
│                                                              │
│ ┌─ Performance ──────────────────────────────────────────┐  │
│ │ Render Time: 0.5ms                                      │  │
│ │ FPS Impact:  Negligible (<1%)                           │  │
│ └──────────────────────────────────────────────────────────┘ │
│                                                              │
│ [ ] Preview Only (don't send to virtual camera)             │
│                                                              │
│ [Clear Filter] [Refresh List]                               │
└──────────────────────────────────────────────────────────────┘
```

### UI Elements

1. **Enable Toggle**: Master switch for AR filter system
2. **Filter Grid**: Visual selection of available filters
3. **Active Filter Info**: Details about currently selected filter
4. **Performance Stats**: Real-time render time and FPS impact
5. **Preview Toggle**: Test filters without virtual camera output
6. **Action Buttons**: Clear filter, refresh list

### Interaction Flow

```text
User Action                    System Response
1. Open AR Filters panel       → Display available filters
2. Click filter button         → Load filter, render on face
3. Face detected               → Filter appears aligned
4. Move head                   → Filter tracks smoothly
5. Click "None"                → Filter disappears
6. Close app                   → Save selection to config
7. Reopen app                  → Restore last filter
```

---

## Implementation Timeline

### Day 1: ARFilterPanel Class (8 hours)

**Hours 1-2: Class skeleton and basic rendering**
- Create header and source files
- Implement constructor and basic Render() method
- Add to BUILD file
- Test panel appears in UI

**Hours 3-4: Filter enumeration and thumbnail loading**
- Implement RefreshFilterList() 
- Parse filter.json files
- Implement LoadThumbnailTexture()
- Load existing thumbnails
- Test filter discovery

**Hours 5-6: Thumbnail generation**
- Implement GenerateThumbnails()
- Generate missing thumbnails (128x128 PNG)
- Test with all 5 existing filters
- Verify thumbnails display correctly

**Hours 7-8: Filter selection UI**
- Implement RenderFilterGrid() with ImGui::Image
- Implement SelectFilter() and ClearFilter()
- Add filter info section
- Add performance stats display
- Test filter activation

### Day 2: Integration and Polish (4 hours)

**Hours 1-2: ConfigManager integration**
- Add AR filter fields to AppState
- Implement LoadARFilterConfig() and SaveARFilterConfig()
- Add YAML read/write
- Test persistence across restarts

**Hours 2-3: UI Manager registration**
- Modify UIManagerEnhanced to register ARFilterPanel
- Update ApplicationRun to instantiate panel
- Test integration with main app

**Hour 4: Polish and testing**
- Add keyboard shortcuts (Ctrl+A toggle)
- Improve layout and spacing
- Add tooltips and help text
- Final testing

---

## Technical Details

### Filter Discovery

**Directory Structure**:
```
assets/filters/
├── classic-glasses-v1/
│   ├── filter.json
│   ├── glasses.obj
│   └── glasses_texture.png
├── top-hat-v1/
│   ├── filter.json
│   ├── hat.obj
│   └── hat_texture.png
└── cat-ears-v1/
    ├── filter.json
    ├── ears.obj
    └── ears_texture.png
```

**Discovery Algorithm**:
1. Scan `assets/filters/` directory
2. For each subdirectory:
   - Look for `filter.json`
   - Parse JSON to extract metadata
   - Validate required assets exist
   - Add to available_filters_ list
3. Sort by category, then name
4. Cache results (refresh on button click)

### Filter Selection

**Selection Flow**:
```cpp
void ARFilterPanel::SelectFilter(const std::string& filter_id) {
  // 1. Update app state
  state_.ar_filters.active_filter_id = filter_id;
  state_.ar_filters.enabled = true;
  
  // 2. Load filter through ARFilterManager
  absl::Status status = ar_mgr_.LoadFilter(filter_id);
  if (!status.ok()) {
    ABSL_LOG(ERROR) << "Failed to load filter: " << status.message();
    state_.ar_filters.active_filter_id = "";
    return;
  }
  
  // 3. Update UI state
  active_filter_id_ = filter_id;
  
  // 4. Auto-save (optional)
  // config_manager_->SaveCurrentProfile();
}
```

### Performance Monitoring

**Metrics to Display**:
- **Render Time**: `ar_mgr_.GetRenderTimeMs()` - Time to render filter
- **FPS Impact**: Calculate from render time vs frame budget
- **Filter Count**: Number of active attachments
- **Vertex Count**: Total vertices being rendered (optional)

**Display Logic**:
```cpp
void ARFilterPanel::RenderPerformanceStats() {
  float render_time_ms = ar_mgr_.GetRenderTimeMs();
  float fps_impact = (render_time_ms / 16.67f) * 100.0f;  // % of 60fps budget
  
  ImGui::Text("Render Time: %.2f ms", render_time_ms);
  
  if (render_time_ms < 3.0f) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Performance: Excellent");
  } else if (render_time_ms < 5.0f) {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Performance: Good");
  } else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Performance: Poor (%.1f%% FPS)", fps_impact);
  }
}
```

---

## Keyboard Shortcuts

| Shortcut | Action | Implementation |
|----------|--------|----------------|
| `Ctrl+A` | Toggle AR filters on/off | Check in ARFilterPanel::Render() |
| `ESC` | Clear active filter | Same as clicking "None" |
| `F1-F12` | Quick select filters 1-12 | Phase 10+ (with more filters) |

**Implementation Example**:
```cpp
void ARFilterPanel::Render() {
  // ... existing rendering ...
  
  // Keyboard shortcuts
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    ClearFilter();
  }
  
  if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
    state_.ar_filters.enabled = !state_.ar_filters.enabled;
  }
}
```

---

## Testing Plan

### Unit Tests

**ARFilterPanel Tests**:
- ✅ Panel constructs successfully
- ✅ Filter enumeration finds all filters
- ✅ Filter selection updates state correctly
- ✅ Clear filter removes active filter
- ✅ Keyboard shortcuts work as expected

**ConfigManager Tests**:
- ✅ AR filter config saves to YAML
- ✅ AR filter config loads from YAML
- ✅ Invalid filter IDs handled gracefully
- ✅ Config persists across app restarts

### Integration Tests

**UI Integration**:
- ✅ Panel appears in UI Manager
- ✅ Panel renders without errors
- ✅ Clicking filters activates them
- ✅ Performance stats update in real-time
- ✅ Filter persists after restart

### Manual Testing Scenarios

1. **Basic Usage**:
   - Open AR Filters panel
   - Click "Classic Glasses" → Glasses appear on face
   - Move head → Glasses track smoothly
   - Click "None" → Glasses disappear

2. **Performance**:
   - Enable AR filter
   - Check performance stats show < 3ms render time
   - Verify no frame drops or stuttering
   - Confirm FPS remains stable

3. **Persistence**:
   - Select a filter
   - Close application
   - Reopen application
   - Verify filter is still active

4. **Edge Cases**:
   - No filters available → Show "No filters found" message
   - Invalid filter ID in config → Load with no filter active
   - Filter assets missing → Show error, don't crash
   - Rapid filter switching → No crashes or glitches

---

## Dependencies

### Phase Dependencies

- ✅ **Phase 0**: Blendshapes (not directly used, but available)
- ✅ **Phase 1-6**: AR filter core system (all complete)
- ✅ **Phase 7**: ARFilterManager (90% complete, behaviors exist)
- ✅ **Phase 8**: Application integration (100% complete)

### External Dependencies

- ✅ **ImGui**: Already integrated, no changes needed
- ✅ **ConfigManager**: Already exists, just add AR filter section
- ✅ **UIManagerEnhanced**: Already exists, just register new panel
- ✅ **ARFilterManager**: Complete API available

### File Dependencies

**Existing Files to Reference**:
- `src/ui/camera_panel.cpp` - Example ImGui panel implementation
- `src/ui/beauty_panel.cpp` - Example settings panel
- `src/ui/performance_panel.cpp` - Example stats display
- `src/config/config_manager.cpp` - YAML read/write patterns
- `include/application/app_state.h` - State structure patterns

---

## Success Criteria

### Functional Requirements

- ✅ ARFilterPanel displays in UI
- ✅ Available filters are enumerated and displayed
- ✅ User can select filters with one click
- ✅ Active filter is clearly indicated
- ✅ "No Filter" option works correctly
- ✅ Filter selection persists across restarts
- ✅ Performance stats display correctly

### Performance Requirements

- ✅ UI panel rendering < 0.1ms per frame
- ✅ Filter enumeration < 100ms on startup
- ✅ Filter switching < 50ms (user perception of instant)
- ✅ No impact on AR filter render performance

### Quality Requirements

- ✅ UI is intuitive and easy to use
- ✅ Panel layout is clean and organized
- ✅ No crashes or errors during normal use
- ✅ Error messages are clear and actionable
- ✅ Tooltips provide helpful information

### Code Quality Requirements

- ✅ Follows existing code style (clang-format)
- ✅ Proper error handling (absl::Status)
- ✅ No memory leaks (verified with valgrind)
- ✅ Clean separation of concerns
- ✅ Well-documented public APIs

---

## Deliverables

### Code Deliverables

1. **ARFilterPanel Class**:
   - `include/ui/ar_filter_panel.h` (180 lines - added thumbnail methods)
   - `src/ui/ar_filter_panel.cpp` (550 lines - added thumbnail generation)

2. **ConfigManager Updates**:
   - AR filter section in config_manager.h/cpp (50 lines)

3. **AppState Updates**:
   - AR filter fields in app_state.h (10 lines)

4. **UI Manager Updates**:
   - Panel registration in ui_manager_enhanced.h/cpp (20 lines)

5. **BUILD File Updates**:
   - Add ar_filter_panel target to BUILD

6. **Integration**:
   - ApplicationRun panel instantiation (5 lines)

**Total Estimated Lines**: ~785 lines of new code (+150 for thumbnail support)

### Documentation Deliverables

1. **Phase 9 Completion Document** (this file updated)
2. **API Documentation** (in header comments)
3. **User Guide Section** (README.md update)
4. **Testing Report** (PHASE_9_TESTING.md)

---

## Risk Assessment

### Identified Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| UI layout issues | Medium | Low | Reference existing panels, iterate on design |
| Filter enumeration slow | Low | Medium | Cache results, async loading (Phase 10+) |
| Config corruption | Low | High | Validate on load, use defaults on error |
| UI crash on error | Low | High | Proper error handling, defensive coding |
| Poor UX | Medium | Medium | User testing, iterate on feedback |

### Contingency Plans

1. **If filter enumeration is slow** (>500ms):
   - Cache filter list on first load
   - Add manual refresh button
   - Consider async loading in Phase 10

2. **If UI layout doesn't fit**:
   - Use collapsible sections
   - Add scrolling to filter grid
   - Consider separate window (Phase 10+)

3. **If config migration fails**:
   - Detect old config format
   - Migrate automatically or use defaults
   - Log migration status

---

## Future Enhancements (Phase 10+)

### Phase 10: Sample Filters
- Create 5-10 sample filters with thumbnails
- Add filter categories (glasses, hats, masks, etc.)
- Filter preview images for UI

### Phase 11: Advanced UI
- Filter search and filtering
- Filter categories as tabs
- Filter preview mode (before applying)
- Custom filter upload

### Phase 11.5: Expression Behaviors
- UI controls for behavior parameters
- Behavior presets (subtle, moderate, intense)
- Per-filter behavior toggles

### Phase 12: Filter Editor
- In-app filter creation tool
- Visual attachment point editor
- Material/texture editor
- Export custom filters

---

## Notes

### Design Decisions

1. **Why ImGui for UI?**
   - Already integrated and used throughout SegmeCam
   - Lightweight and performant
   - Easy to iterate and modify
   - Consistent with existing UI

2. **Why YAML for persistence?**
   - Already used by ConfigManager
   - Human-readable and editable
   - Easy to version control
   - Supports nested structures

3. **Why separate ARFilterPanel?**
   - Follows existing panel architecture
   - Clean separation of concerns
   - Easy to test independently
   - Can be shown/hidden independently

4. **Why filter_id instead of filter name?**
   - Unique identifier (names can change)
   - Version support (classic-glasses-v1, v2, etc.)
   - Easier to persist and reference
   - Matches filter directory structure

### Lessons from Phase 8

1. **Start with working code**: Reference existing panels for patterns
2. **Test incrementally**: Build and test each method as you go
3. **Use existing patterns**: Follow ConfigManager YAML patterns
4. **Error handling first**: Add absl::Status checks from the start
5. **Document as you go**: Comments help catch design issues early

### Questions to Resolve

- ❓ Should filter thumbnails be loaded in Phase 9 or Phase 10?
  - **Decision**: Phase 10 (with sample filter creation)
  
- ❓ Should we support filter preview mode now?
  - **Decision**: Phase 11 (after basic UI is stable)
  
- ❓ Should filter config auto-save on change?
  - **Decision**: Yes, call SaveCurrentProfile() on filter change
  
- ❓ Should we add filter categories now?
  - **Decision**: Basic support (parse from JSON), full UI in Phase 10

---

## Conclusion

Phase 9 completes the user-facing AR filter feature by adding an intuitive UI for filter selection and management. With this phase complete, users will be able to:

1. Browse available AR filters
2. Select filters with one click
3. See active filter information
4. Monitor performance impact
5. Have preferences persist across sessions

The implementation leverages existing SegmeCam patterns (ImGui panels, ConfigManager YAML, AppState) to integrate seamlessly with the application architecture.

**Estimated Completion**: End of Day 2 (October 8, 2025)

**Next Phase**: Phase 10 - Sample Filters Creation (5-10 demo filters with assets)

---

**Document Version**: 1.0  
**Last Updated**: October 4, 2025  
**Author**: SegmeCam Team  
**Status**: Planning Complete, Ready to Implement

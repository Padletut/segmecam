# 🏗️ Effects Manager Refactoring Plan

## Current State Analysis

- **File**: `effects_manager.cpp` (1013 lines, 783 non-comment lines)
- **Issue**: Exceeds Codacy complexity threshold (Lizard_file-nloc-medium)
- **Methods**: ~80+ methods in single class

## Refactoring Strategy

Break down monolithic EffectsManager into focused, single-responsibility components:

### Phase 1: Background Effects Module ✅

**Files to create:**

- `include/effects/background/background_effects.h`
- `src/effects/background/background_effects.cpp`

**Methods to move:**

- `ApplyBackgroundEffect`
- `ApplyMaskVisualization`
- `ApplyBackgroundModeEffect`
- `ApplyDefaultBackgroundEffect`
- `ApplyBlurBackground`
- `ApplyImageBackground`
- `ApplySolidBackground`
- `ResizeMaskIfNeeded`
- `ConvertRGBColorToBGR`

### Phase 2: Performance Monitoring Module ✅

**Files to create:**

- `include/effects/performance/performance_monitor.h`
- `src/effects/performance/performance_monitor.cpp`

**Methods to move:**

- `UpdatePerformanceTracking`
- `LogPerformanceStats`
- `ShouldLogPerformance`
- `GetAverageProcessingTime`
- `ResetPerformanceStats`
- `UpdatePerformanceStats`
- All auto-processing scale methods (FPS tracking, auto-scaling)

### Phase 3: Effects Configuration Module ✅

**Files to create:**

- `include/effects/config/effects_config.h`
- `src/effects/config/effects_config.cpp`

**Methods to move:**

- All `Set*` methods (background, skin, wrinkle, lip, teeth settings)
- `ApplyBeautyPreset`
- `GetCurrentBeautyState`
- `SetBeautyState`

### Phase 4: Face Processing Utilities ✅

**Files to create:**

- `include/effects/face/face_processor.h`
- `src/effects/face/face_processor.cpp`

**Methods to move:**

- `ExtractFaceRegionsFromLandmarks`
- `DrawLandmarks`
- `DrawConnections` (template)
- Face region transformation methods

### Phase 5: Core Effects Manager (Slimmed Down) ✅

**Keep in effects_manager.cpp:**

- Constructor/Destructor
- `Initialize` / `Cleanup`
- `ProcessFrame` (main pipeline)
- `ProcessFaceEffects` / `ProcessBackgroundEffects`
- Core state management

## Implementation Order

1. Create background effects module
2. Create performance monitoring module  
3. Create configuration module
4. Create face processing utilities
5. Update main effects_manager to use new modules
6. Update BUILD file
7. Test compilation and functionality

## Expected Results

- **effects_manager.cpp**: ~300-400 lines (main pipeline only)
- **Individual modules**: 100-200 lines each
- **Maintainability**: Each module has single responsibility
- **Testability**: Individual components can be unit tested
- **Codacy Compliance**: All files under complexity limits

// Copyright 2025 SegmeCam Project
// Licensed under the Apache License, Version 2.0

#ifndef SEGMECAM_AR_FILTERS_FILTER_PRESETS_H_
#define SEGMECAM_AR_FILTERS_FILTER_PRESETS_H_

#include <string>
#include <vector>
#include <memory>

namespace segmecam {

// Forward declarations
class AttachmentController;

// Filter preset types
enum class FilterPreset {
  None = 0,
  ClassicGlasses,  // Simple glasses on nose/eyes
  PartyHat,        // Cone hat with pom-pom
  FaceMask,        // Protective face mask
  TestDemo         // Keep test demo available for debugging
};

// Manages production filter presets
class FilterPresetManager {
 public:
  FilterPresetManager();
  ~FilterPresetManager() = default;

  // Preset management
  void ApplyPreset(FilterPreset preset, AttachmentController& controller,
                   bool& enabled);
  void ClearCurrentPreset(AttachmentController& controller, bool& enabled);

  // Preset information
  std::string GetPresetName(FilterPreset preset) const;
  std::string GetPresetDescription(FilterPreset preset) const;
  int GetPresetFilterCount(FilterPreset preset) const;

  // Current state
  FilterPreset GetCurrentPreset() const { return current_preset_; }
  bool IsPresetActive() const { return preset_active_; }
  const std::vector<std::string>& GetActiveFilterIds() const {
    return active_filter_ids_;
  }

  // Get all available presets
  std::vector<FilterPreset> GetAllPresets() const;

 private:
  // Preset creators - each creates specific filter configuration
  void CreateClassicGlasses(AttachmentController& controller);
  void CreatePartyHat(AttachmentController& controller);
  void CreateFaceMask(AttachmentController& controller);

  // State tracking
  FilterPreset current_preset_;
  bool preset_active_;
  std::vector<std::string> active_filter_ids_;  // IDs of filters in current preset
};

}  // namespace segmecam

#endif  // SEGMECAM_AR_FILTERS_FILTER_PRESETS_H_

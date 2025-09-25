#pragma once

#include <string>

namespace segmecam {

// Base class for all UI panels
class UIPanel {
public:
    UIPanel(const std::string& name) : panel_name_(name) {}
    virtual ~UIPanel() = default;
    
    virtual void Render() = 0;
    
    void SetVisible(bool visible) { visible_ = visible; }
    bool IsVisible() const { return visible_; }
    const std::string& GetName() const { return panel_name_; }

protected:
    std::string panel_name_;
    bool visible_ = true;
};

} // namespace segmecam

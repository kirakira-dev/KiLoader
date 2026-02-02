#pragma once

#include <string>
#include <vector>
#include "ftxui/component/component.hpp"

namespace kiloader {
namespace gui {

class App;

// Pseudocode view
class PseudoView {
public:
    PseudoView(App& app);
    
    // Get FTXUI component
    ftxui::Component getComponent();
    
    // Render the view
    ftxui::Element render();
    
    // Set function to display
    void setFunction(uint64_t addr);
    
    // Get current content
    const std::string& getContent() const { return content_; }
    
private:
    App& app_;
    
    uint64_t current_function_ = 0;
    std::string content_;
    std::vector<std::string> lines_;
    
    int scroll_offset_ = 0;
    int selected_line_ = 0;
    
    void refresh();
    ftxui::Element renderLine(int line_num, const std::string& line);
};

} // namespace gui
} // namespace kiloader

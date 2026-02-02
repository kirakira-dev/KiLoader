#pragma once

#include <string>
#include <vector>
#include <functional>
#include "ftxui/component/component.hpp"

namespace kiloader {
namespace gui {

class App;

// Function list view
class FunctionView {
public:
    FunctionView(App& app);
    
    // Get FTXUI component
    ftxui::Component getComponent();
    
    // Render the view
    ftxui::Element render();
    
    // Refresh function list from analyzer
    void refresh();
    
    // Filter functions
    void setFilter(const std::string& filter);
    
    // Get selected function address
    uint64_t getSelectedAddress() const;
    
private:
    App& app_;
    
    std::vector<std::pair<uint64_t, std::string>> functions_;  // addr, name
    std::vector<size_t> filtered_indices_;
    std::string filter_;
    
    int selected_ = 0;
    int scroll_offset_ = 0;
    
    void applyFilter();
};

} // namespace gui
} // namespace kiloader

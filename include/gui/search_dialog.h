#pragma once

#include <string>
#include <vector>
#include <functional>
#include "ftxui/component/component.hpp"

namespace kiloader {
namespace gui {

class App;

// Search type
enum class SearchType {
    Strings,
    Assembly,
    RawHex,
    Pseudocode
};

// Search result
struct SearchResult {
    uint64_t address;
    std::string context;
    std::string match;
};

// Search dialog
class SearchDialog {
public:
    SearchDialog(App& app);
    
    // Get FTXUI component
    ftxui::Component getComponent();
    
    // Render the dialog
    ftxui::Element render();
    
    // Show/hide
    void show(SearchType type);
    void hide();
    bool isVisible() const { return visible_; }
    
    // Perform search
    void search();
    
    // Get results
    const std::vector<SearchResult>& getResults() const { return results_; }
    
    // Navigate to result
    void goToResult(int index);
    
private:
    App& app_;
    
    bool visible_ = false;
    SearchType search_type_ = SearchType::Strings;
    std::string query_;
    std::vector<SearchResult> results_;
    
    int selected_result_ = 0;
    int scroll_offset_ = 0;
    
    void searchStrings();
    void searchAssembly();
    void searchRawHex();
    void searchPseudocode();
};

} // namespace gui
} // namespace kiloader

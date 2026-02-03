#pragma once

#include <ncurses.h>
#include <string>
#include <vector>

namespace kiloader {
namespace gui {

class App;

// Search types
enum class SearchType {
    Strings,
    Assembly,
    RawHex,
    Pseudocode
};

class SearchDialog {
public:
    SearchDialog(App& app);
    ~SearchDialog();
    
    void show(SearchType type);
    void hide();
    bool isVisible() const { return visible_; }
    
    void draw();
    void handleKey(int ch);
    
private:
    void performSearch();
    
    App& app_;
    
    bool visible_ = false;
    SearchType type_ = SearchType::Strings;
    std::string query_;
    std::vector<std::string> results_;
    int selected_ = 0;
    int scroll_offset_ = 0;
    
    WINDOW* win_ = nullptr;
};

} // namespace gui
} // namespace kiloader

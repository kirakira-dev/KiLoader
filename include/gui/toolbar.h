#pragma once

#include <string>
#include <functional>
#include <vector>
#include "ftxui/component/component.hpp"

namespace kiloader {
namespace gui {

class App;

// Menu item
struct MenuItem {
    std::string label;
    std::function<void()> action;
    std::string shortcut;
    bool enabled = true;
};

// Menu
struct Menu {
    std::string label;
    std::vector<MenuItem> items;
};

// Toolbar with menus
class Toolbar {
public:
    Toolbar(App& app);
    
    // Get FTXUI component
    ftxui::Component getComponent();
    
    // Render the toolbar element
    ftxui::Element render();
    
    // Show file browser dialog
    void showLoadDialog();
    void showLoadProgressDialog();
    
private:
    void setupMenus();
    
    App& app_;
    std::vector<Menu> menus_;
    
    int selected_menu_ = 0;
    bool menu_open_ = false;
    int selected_item_ = 0;
    
    // File input
    std::string file_input_;
    bool show_file_dialog_ = false;
    bool show_progress_dialog_ = false;
    int selected_progress_ = 0;
    std::vector<std::string> progress_list_;
};

} // namespace gui
} // namespace kiloader

#pragma once

#include <ncurses.h>
#include <menu.h>
#include <string>
#include <vector>
#include <functional>

namespace kiloader {
namespace gui {

class App;

struct MenuItem {
    std::string label;
    std::function<void()> action;
};

struct Menu {
    std::string label;
    std::vector<MenuItem> items;
};

class Toolbar {
public:
    Toolbar(App& app);
    ~Toolbar();
    
    void draw(WINDOW* win);
    void handleKey(int ch);
    
    bool isMenuOpen() const { return menu_open_; }
    
private:
    void setupMenus();
    void showDropdown(int menu_idx);
    void hideDropdown();
    void executeItem();
    
    App& app_;
    std::vector<Menu> menus_;
    
    int selected_menu_ = 0;
    int selected_item_ = 0;
    bool menu_open_ = false;
    
    WINDOW* dropdown_win_ = nullptr;
};

} // namespace gui
} // namespace kiloader

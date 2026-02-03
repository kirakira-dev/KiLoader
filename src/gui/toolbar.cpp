#include "gui/toolbar.h"
#include "gui/app.h"

namespace kiloader {
namespace gui {

Toolbar::Toolbar(App& app) : app_(app) {
    setupMenus();
}

Toolbar::~Toolbar() {
    hideDropdown();
}

void Toolbar::setupMenus() {
    Menu load_menu;
    load_menu.label = "Load";
    load_menu.items = {
        {"Load NSO File...", [this]() { 
            app_.focusCommandCenter();
            app_.appendOutput("Type: load <path>");
        }},
        {"Load Progress...", [this]() { 
            app_.focusCommandCenter();
            app_.appendOutput("Type: load progress <build_id>");
        }},
        {"Save Progress", [this]() { app_.saveProgress(); }},
        {"Exit", [this]() { app_.quit(); }},
    };
    menus_.push_back(load_menu);
    
    Menu window_menu;
    window_menu.label = "Window";
    window_menu.items = {
        {"Toggle Functions", [this]() { 
            app_.getWindowState().show_functions = !app_.getWindowState().show_functions; 
        }},
        {"Toggle Pseudocode", [this]() { 
            app_.getWindowState().show_pseudo = !app_.getWindowState().show_pseudo;
            if (app_.getWindowState().show_pseudo) app_.getWindowState().show_disasm = false;
        }},
        {"Toggle Disassembly", [this]() { 
            app_.getWindowState().show_disasm = !app_.getWindowState().show_disasm;
            if (app_.getWindowState().show_disasm) app_.getWindowState().show_pseudo = false;
        }},
    };
    menus_.push_back(window_menu);
    
    Menu tools_menu;
    tools_menu.label = "Tools";
    tools_menu.items = {
        {"Search Strings", [this]() {
            app_.focusCommandCenter();
            app_.appendOutput("Type: strings <pattern>");
        }},
        {"Go to Address", [this]() {
            app_.focusCommandCenter();
            app_.appendOutput("Type: goto <address>");
        }},
        {"Disassemble", [this]() {
            app_.focusCommandCenter();
            app_.appendOutput("Type: disasm <address>");
        }},
    };
    menus_.push_back(tools_menu);
    
    Menu ui_menu;
    ui_menu.label = "UI";
    ui_menu.items = {
        {"Toggle Line Numbers", [this]() { 
            app_.getSettings().show_line_numbers = !app_.getSettings().show_line_numbers; 
        }},
    };
    menus_.push_back(ui_menu);
}

void Toolbar::draw(WINDOW* win) {
    int width = getmaxx(win);
    
    // Draw menu bar background
    wattron(win, A_REVERSE);
    mvwhline(win, 0, 0, ' ', width);
    
    // Draw title
    wattron(win, A_BOLD);
    mvwprintw(win, 0, 1, "KILOADER");
    wattroff(win, A_BOLD);
    
    // Draw menu items
    int x = 11;
    for (size_t i = 0; i < menus_.size(); i++) {
        if (menu_open_ && selected_menu_ == static_cast<int>(i)) {
            wattroff(win, A_REVERSE);
            wattron(win, A_BOLD);
        }
        mvwprintw(win, 0, x, " %s ", menus_[i].label.c_str());
        if (menu_open_ && selected_menu_ == static_cast<int>(i)) {
            wattroff(win, A_BOLD);
            wattron(win, A_REVERSE);
        }
        x += menus_[i].label.length() + 3;
    }
    wattroff(win, A_REVERSE);
    
    // Note: dropdown is drawn separately via drawDropdown() to ensure it's on top
}

void Toolbar::drawDropdown() {
    if (!menu_open_) return;
    showDropdown(selected_menu_);
}

void Toolbar::showDropdown(int menu_idx) {
    if (menu_idx < 0 || menu_idx >= static_cast<int>(menus_.size())) return;
    
    auto& menu = menus_[menu_idx];
    
    // Calculate position
    int x = 11;
    for (int i = 0; i < menu_idx; i++) {
        x += menus_[i].label.length() + 3;
    }
    
    // Calculate width (longest item + padding)
    int max_width = 0;
    for (auto& item : menu.items) {
        if (item.label.length() > static_cast<size_t>(max_width)) {
            max_width = item.label.length();
        }
    }
    max_width += 4;
    
    int height = menu.items.size() + 2;
    
    // Create dropdown window
    if (dropdown_win_) {
        delwin(dropdown_win_);
    }
    dropdown_win_ = newwin(height, max_width, 1, x);
    
    // Draw dropdown
    wattron(dropdown_win_, A_REVERSE);
    box(dropdown_win_, 0, 0);
    
    for (size_t i = 0; i < menu.items.size(); i++) {
        if (static_cast<int>(i) == selected_item_) {
            wattroff(dropdown_win_, A_REVERSE);
            wattron(dropdown_win_, A_BOLD);
        }
        mvwprintw(dropdown_win_, 1 + i, 1, " %-*s", max_width - 3, menu.items[i].label.c_str());
        if (static_cast<int>(i) == selected_item_) {
            wattroff(dropdown_win_, A_BOLD);
            wattron(dropdown_win_, A_REVERSE);
        }
    }
    wattroff(dropdown_win_, A_REVERSE);
    
    wrefresh(dropdown_win_);
}

void Toolbar::hideDropdown() {
    if (dropdown_win_) {
        werase(dropdown_win_);
        wrefresh(dropdown_win_);
        delwin(dropdown_win_);
        dropdown_win_ = nullptr;
    }
    menu_open_ = false;
}

void Toolbar::handleKey(int ch) {
    if (ch == KEY_F(1)) {
        if (menu_open_ && selected_menu_ == 0) {
            hideDropdown();
        } else {
            selected_menu_ = 0;
            selected_item_ = 0;
            menu_open_ = true;
        }
        return;
    }
    if (ch == KEY_F(2)) {
        if (menu_open_ && selected_menu_ == 1) {
            hideDropdown();
        } else {
            selected_menu_ = 1;
            selected_item_ = 0;
            menu_open_ = true;
        }
        return;
    }
    if (ch == KEY_F(3)) {
        if (menu_open_ && selected_menu_ == 2) {
            hideDropdown();
        } else {
            selected_menu_ = 2;
            selected_item_ = 0;
            menu_open_ = true;
        }
        return;
    }
    if (ch == KEY_F(4)) {
        if (menu_open_ && selected_menu_ == 3) {
            hideDropdown();
        } else {
            selected_menu_ = 3;
            selected_item_ = 0;
            menu_open_ = true;
        }
        return;
    }
    
    if (!menu_open_) return;
    
    if (ch == KEY_DOWN) {
        selected_item_ = std::min(selected_item_ + 1, 
            static_cast<int>(menus_[selected_menu_].items.size()) - 1);
    } else if (ch == KEY_UP) {
        selected_item_ = std::max(selected_item_ - 1, 0);
    } else if (ch == KEY_LEFT) {
        selected_menu_ = (selected_menu_ - 1 + menus_.size()) % menus_.size();
        selected_item_ = 0;
    } else if (ch == KEY_RIGHT) {
        selected_menu_ = (selected_menu_ + 1) % menus_.size();
        selected_item_ = 0;
    } else if (ch == '\n' || ch == KEY_ENTER) {
        executeItem();
        hideDropdown();
    } else if (ch == 27) {  // ESC
        hideDropdown();
    }
}

void Toolbar::executeItem() {
    if (selected_menu_ >= 0 && selected_menu_ < static_cast<int>(menus_.size())) {
        auto& menu = menus_[selected_menu_];
        if (selected_item_ >= 0 && selected_item_ < static_cast<int>(menu.items.size())) {
            if (menu.items[selected_item_].action) {
                menu.items[selected_item_].action();
            }
        }
    }
}

} // namespace gui
} // namespace kiloader

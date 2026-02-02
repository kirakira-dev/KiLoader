#include "gui/toolbar.h"
#include "gui/app.h"
#include "gui/search_dialog.h"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace kiloader {
namespace gui {

using namespace ftxui;

Toolbar::Toolbar(App& app) : app_(app) {
    setupMenus();
}

void Toolbar::setupMenus() {
    // Load menu
    Menu load_menu;
    load_menu.label = "Load";
    load_menu.items = {
        {"Load NSO File...", [this]() { showLoadDialog(); }, ""},
        {"Load Progress...", [this]() { showLoadProgressDialog(); }, ""},
        {"Save Progress", [this]() { app_.saveProgress(); }, "Ctrl+S"},
        {"", nullptr, ""},  // Separator
        {"Exit", [this]() { app_.quit(); }, "Ctrl+Q"},
    };
    menus_.push_back(load_menu);
    
    // Window menu
    Menu window_menu;
    window_menu.label = "Window";
    window_menu.items = {
        {"Toggle Functions", [this]() { 
            app_.getWindowState().show_functions = !app_.getWindowState().show_functions; 
        }, ""},
        {"Toggle Pseudocode", [this]() { 
            app_.getWindowState().show_pseudo = !app_.getWindowState().show_pseudo; 
        }, ""},
        {"Toggle Disassembly", [this]() { 
            app_.getWindowState().show_disasm = !app_.getWindowState().show_disasm; 
        }, ""},
    };
    menus_.push_back(window_menu);
    
    // Tools menu
    Menu tools_menu;
    tools_menu.label = "Tools";
    tools_menu.items = {
        {"Search Strings", [this]() { 
            // Get search dialog and show it
        }, "Ctrl+S"},
        {"Search Assembly", [this]() { }, "Ctrl+A"},
        {"Search Raw Hex", [this]() { }, "Ctrl+H"},
        {"Search Pseudocode", [this]() { }, "Ctrl+P"},
    };
    menus_.push_back(tools_menu);
    
    // UI menu
    Menu ui_menu;
    ui_menu.label = "UI";
    ui_menu.items = {
        {"Toggle Dark Theme", [this]() { 
            app_.getSettings().dark_theme = !app_.getSettings().dark_theme; 
        }, ""},
        {"Toggle Line Numbers", [this]() { 
            app_.getSettings().show_line_numbers = !app_.getSettings().show_line_numbers; 
        }, ""},
    };
    menus_.push_back(ui_menu);
}

Component Toolbar::getComponent() {
    auto component = Container::Horizontal({});
    
    // Create menu buttons
    for (size_t i = 0; i < menus_.size(); i++) {
        auto button = Button(menus_[i].label, [this, i]() {
            if (menu_open_ && selected_menu_ == static_cast<int>(i)) {
                menu_open_ = false;
            } else {
                selected_menu_ = static_cast<int>(i);
                menu_open_ = true;
                selected_item_ = 0;
            }
        });
        component->Add(button);
    }
    
    // Handle file input dialog
    auto file_input = Input(&file_input_, "Enter path...");
    
    return CatchEvent(component, [this, file_input](Event event) {
        if (show_file_dialog_) {
            if (event == Event::Return) {
                if (!file_input_.empty()) {
                    app_.loadNsoFile(file_input_);
                }
                show_file_dialog_ = false;
                file_input_.clear();
                return true;
            }
            if (event == Event::Escape) {
                show_file_dialog_ = false;
                file_input_.clear();
                return true;
            }
        }
        
        if (show_progress_dialog_) {
            if (event == Event::Return) {
                if (selected_progress_ < static_cast<int>(progress_list_.size())) {
                    app_.loadProgress(progress_list_[selected_progress_]);
                }
                show_progress_dialog_ = false;
                return true;
            }
            if (event == Event::Escape) {
                show_progress_dialog_ = false;
                return true;
            }
            if (event == Event::ArrowUp && selected_progress_ > 0) {
                selected_progress_--;
                return true;
            }
            if (event == Event::ArrowDown && selected_progress_ < static_cast<int>(progress_list_.size()) - 1) {
                selected_progress_++;
                return true;
            }
        }
        
        if (menu_open_) {
            if (event == Event::ArrowDown) {
                selected_item_ = std::min(selected_item_ + 1, 
                    static_cast<int>(menus_[selected_menu_].items.size()) - 1);
                return true;
            }
            if (event == Event::ArrowUp) {
                selected_item_ = std::max(selected_item_ - 1, 0);
                return true;
            }
            if (event == Event::ArrowLeft) {
                selected_menu_ = (selected_menu_ - 1 + menus_.size()) % menus_.size();
                selected_item_ = 0;
                return true;
            }
            if (event == Event::ArrowRight) {
                selected_menu_ = (selected_menu_ + 1) % menus_.size();
                selected_item_ = 0;
                return true;
            }
            if (event == Event::Return) {
                auto& item = menus_[selected_menu_].items[selected_item_];
                if (item.action) {
                    item.action();
                }
                menu_open_ = false;
                return true;
            }
            if (event == Event::Escape) {
                menu_open_ = false;
                return true;
            }
        }
        
        return false;
    });
}

Element Toolbar::render() {
    Elements menu_buttons;
    
    for (size_t i = 0; i < menus_.size(); i++) {
        Element btn = text(" " + menus_[i].label + " ");
        if (menu_open_ && selected_menu_ == static_cast<int>(i)) {
            btn = btn | inverted;
        }
        menu_buttons.push_back(btn | border);
    }
    
    Element toolbar = hbox(menu_buttons);
    
    // Render dropdown if open
    if (menu_open_) {
        Elements items;
        for (size_t i = 0; i < menus_[selected_menu_].items.size(); i++) {
            auto& item = menus_[selected_menu_].items[i];
            if (item.label.empty()) {
                items.push_back(separator());
            } else {
                Element line = hbox({
                    text(" " + item.label) | flex,
                    text(item.shortcut + " "),
                });
                if (static_cast<int>(i) == selected_item_) {
                    line = line | inverted;
                }
                items.push_back(line);
            }
        }
        
        // Position dropdown under selected menu
        int offset = 0;
        for (int i = 0; i < selected_menu_; i++) {
            offset += menus_[i].label.length() + 4;  // " label " + borders
        }
        
        Element dropdown = vbox(items) | border;
        toolbar = vbox({
            toolbar,
            hbox({
                text(std::string(offset, ' ')),
                dropdown,
            }),
        });
    }
    
    // File input dialog overlay
    if (show_file_dialog_) {
        toolbar = vbox({
            toolbar,
            hbox({
                text(" Path: "),
                text(file_input_) | flex | inverted,
            }) | border,
        });
    }
    
    // Progress selection dialog
    if (show_progress_dialog_) {
        Elements items;
        for (size_t i = 0; i < progress_list_.size(); i++) {
            Element line = text(" " + progress_list_[i] + " ");
            if (static_cast<int>(i) == selected_progress_) {
                line = line | inverted;
            }
            items.push_back(line);
        }
        if (items.empty()) {
            items.push_back(text(" No saved progress found "));
        }
        toolbar = vbox({
            toolbar,
            vbox({
                text(" Select Progress: ") | bold,
                separator(),
                vbox(items),
            }) | border,
        });
    }
    
    return toolbar;
}

void Toolbar::showLoadDialog() {
    show_file_dialog_ = true;
    file_input_.clear();
    menu_open_ = false;
}

void Toolbar::showLoadProgressDialog() {
    ProgressManager mgr;
    progress_list_ = mgr.listProgress();
    selected_progress_ = 0;
    show_progress_dialog_ = true;
    menu_open_ = false;
}

} // namespace gui
} // namespace kiloader

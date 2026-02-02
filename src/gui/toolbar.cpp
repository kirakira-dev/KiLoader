#include "gui/toolbar.h"
#include "gui/app.h"
#include "gui/search_dialog.h"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

#include <algorithm>

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
        {"Save Progress", [this]() { app_.saveProgress(); }, ""},
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
            app_.focusCommandCenter();
            app_.appendOutput("Type: strings <pattern>");
        }, "/"},
        {"Go to Address", [this]() {
            app_.focusCommandCenter();
            app_.appendOutput("Type: goto <address>");
        }, ":goto"},
        {"Disassemble", [this]() {
            app_.focusCommandCenter();
            app_.appendOutput("Type: disasm <address> [count]");
        }, ":disasm"},
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
    // Use FTXUI's built-in button with animated style for proper mouse handling
    std::vector<Component> menu_buttons;
    
    for (size_t i = 0; i < menus_.size(); i++) {
        size_t idx = i;
        auto btn = Button(menus_[i].label, [this, idx]() {
            if (menu_open_ && selected_menu_ == static_cast<int>(idx)) {
                menu_open_ = false;
            } else {
                selected_menu_ = static_cast<int>(idx);
                menu_open_ = true;
                selected_item_ = 0;
            }
        }, ButtonOption::Animated(Color::Cyan));
        menu_buttons.push_back(btn);
    }
    
    // Title label
    auto title = Renderer([] {
        return text(" KILOADER ") | bold | color(Color::Cyan);
    });
    
    // Combine title + buttons
    std::vector<Component> bar_components;
    bar_components.push_back(title);
    for (auto& btn : menu_buttons) {
        bar_components.push_back(btn);
    }
    
    auto menu_bar = Container::Horizontal(bar_components);
    
    // Wrap everything with event handler and custom rendering for dropdowns/dialogs
    auto component = CatchEvent(menu_bar, [this](Event event) {
        // File dialog has priority
        if (show_file_dialog_) {
            if (event.is_character()) {
                file_input_ += event.character();
                return true;
            }
            if (event.input().size() == 1) {
                char c = event.input()[0];
                if (c >= 32 && c < 127) {
                    file_input_ += c;
                    return true;
                }
            }
            if (event == Event::Backspace && !file_input_.empty()) {
                file_input_.pop_back();
                return true;
            }
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
            return true;
        }
        
        // Progress dialog
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
            return true;
        }
        
        // Dropdown navigation
        if (menu_open_) {
            if (event == Event::ArrowDown) {
                do {
                    selected_item_ = std::min(selected_item_ + 1, 
                        static_cast<int>(menus_[selected_menu_].items.size()) - 1);
                } while (selected_item_ < static_cast<int>(menus_[selected_menu_].items.size()) - 1 &&
                         menus_[selected_menu_].items[selected_item_].label.empty());
                return true;
            }
            if (event == Event::ArrowUp) {
                do {
                    selected_item_ = std::max(selected_item_ - 1, 0);
                } while (selected_item_ > 0 && 
                         menus_[selected_menu_].items[selected_item_].label.empty());
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
        
        // F-keys
        if (event == Event::F1) { selected_menu_ = 0; menu_open_ = true; selected_item_ = 0; return true; }
        if (event == Event::F2) { selected_menu_ = 1; menu_open_ = true; selected_item_ = 0; return true; }
        if (event == Event::F3) { selected_menu_ = 2; menu_open_ = true; selected_item_ = 0; return true; }
        if (event == Event::F4) { selected_menu_ = 3; menu_open_ = true; selected_item_ = 0; return true; }
        
        return false;
    });
    
    return component;
}

Element Toolbar::render() {
    // Render dropdown overlay if menu is open
    Elements extra;
    
    if (menu_open_ && selected_menu_ >= 0 && selected_menu_ < static_cast<int>(menus_.size())) {
        Elements items;
        for (size_t i = 0; i < menus_[selected_menu_].items.size(); i++) {
            auto& item = menus_[selected_menu_].items[i];
            if (item.label.empty()) {
                items.push_back(separatorLight());
            } else {
                Element line = hbox({
                    text(" " + item.label) | flex,
                    text(item.shortcut.empty() ? "  " : " " + item.shortcut + " ") | dim,
                });
                if (static_cast<int>(i) == selected_item_) {
                    line = line | inverted;
                }
                items.push_back(line);
            }
        }
        
        // Position dropdown
        int offset = 11;  // KILOADER label
        for (int i = 0; i < selected_menu_; i++) {
            offset += menus_[i].label.length() + 4;
        }
        
        extra.push_back(hbox({
            text(std::string(offset, ' ')),
            vbox(items) | border | bgcolor(Color::Black),
        }));
    }
    
    // File dialog
    if (show_file_dialog_) {
        extra.push_back(separator());
        extra.push_back(hbox({
            text(" Path: ") | bold,
            text(file_input_) | flex | bgcolor(Color::GrayDark),
            text("_") | blink,
        }));
        extra.push_back(text(" Enter: load | Escape: cancel ") | dim | center);
    }
    
    // Progress dialog  
    if (show_progress_dialog_) {
        extra.push_back(separator());
        extra.push_back(text(" Select saved progress: ") | bold);
        Elements items;
        if (progress_list_.empty()) {
            items.push_back(text(" No saved progress ") | dim);
        } else {
            for (size_t i = 0; i < progress_list_.size(); i++) {
                Element line = text(" " + progress_list_[i] + " ");
                if (static_cast<int>(i) == selected_progress_) {
                    line = line | inverted;
                }
                items.push_back(line);
            }
        }
        extra.push_back(vbox(items) | border);
        extra.push_back(text(" Up/Down | Enter | Escape ") | dim | center);
    }
    
    if (extra.empty()) {
        return text("");  // No overlay needed
    }
    return vbox(extra);
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

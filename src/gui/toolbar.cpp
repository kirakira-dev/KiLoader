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
    // Create button options with custom style
    auto button_style = ButtonOption::Animated();
    button_style.transform = [](const EntryState& s) {
        Element el = text(" " + s.label + " ");
        if (s.focused) {
            el = el | inverted;
        }
        return el;
    };
    
    // Create menu bar buttons
    std::vector<Component> buttons;
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
        }, button_style);
        buttons.push_back(btn);
    }
    
    auto menu_bar = Container::Horizontal(buttons);
    
    // Wrap to handle keyboard navigation and dialogs
    return CatchEvent(menu_bar, [this](Event event) {
        // Handle file input dialog - MUST be first to capture all input
        if (show_file_dialog_) {
            // Handle all printable characters
            if (event.is_character()) {
                file_input_ += event.character();
                return true;
            }
            // Special handling for characters that might be interpreted as events
            if (event.input().size() == 1) {
                char c = event.input()[0];
                if (c >= 32 && c < 127) {  // Printable ASCII
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
            return true;  // Consume ALL events when dialog is open
        }
        
        // Handle progress dialog
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
        
        // Handle open dropdown menu
        if (menu_open_) {
            if (event == Event::ArrowDown) {
                // Skip separators
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
            
            // Handle mouse clicks on dropdown items
            if (event.is_mouse() && event.mouse().button == Mouse::Left &&
                event.mouse().motion == Mouse::Pressed) {
                int y = event.mouse().y;
                // Dropdown items start at y=1 (after the menu bar line)
                // Each item is 1 line
                int item_idx = y - 1;
                if (item_idx >= 0 && item_idx < static_cast<int>(menus_[selected_menu_].items.size())) {
                    auto& item = menus_[selected_menu_].items[item_idx];
                    if (!item.label.empty() && item.action) {
                        selected_item_ = item_idx;
                        item.action();
                        menu_open_ = false;
                        return true;
                    }
                }
                // Click elsewhere closes menu
                menu_open_ = false;
                return true;
            }
        }
        
        // F1-F4 to open specific menus
        if (event == Event::F1) {
            selected_menu_ = 0;
            menu_open_ = true;
            selected_item_ = 0;
            return true;
        }
        if (event == Event::F2) {
            selected_menu_ = 1;
            menu_open_ = true;
            selected_item_ = 0;
            return true;
        }
        if (event == Event::F3) {
            selected_menu_ = 2;
            menu_open_ = true;
            selected_item_ = 0;
            return true;
        }
        if (event == Event::F4) {
            selected_menu_ = 3;
            menu_open_ = true;
            selected_item_ = 0;
            return true;
        }
        
        return false;
    });
}

Element Toolbar::render() {
    Elements menu_buttons;
    
    for (size_t i = 0; i < menus_.size(); i++) {
        Element btn = text(" " + menus_[i].label + " ");
        if (menu_open_ && selected_menu_ == static_cast<int>(i)) {
            btn = btn | inverted | bold;
        }
        menu_buttons.push_back(btn | border);
    }
    
    Element toolbar = hbox({
        text(" KILOADER ") | bold | color(Color::Cyan) | border,
        hbox(menu_buttons),
        filler(),
    });
    
    // Render dropdown if open
    if (menu_open_) {
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
        
        // Calculate dropdown position - after "KILOADER" box and previous menu buttons
        int offset = 12;  // " KILOADER " + borders
        for (int i = 0; i < selected_menu_; i++) {
            offset += menus_[i].label.length() + 4;  // " label " + borders
        }
        
        Element dropdown = vbox(items) | border | bgcolor(Color::Black);
        
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
            separator(),
            hbox({
                text(" Path: ") | bold,
                text(file_input_) | flex | bgcolor(Color::GrayDark),
                text("_") | blink,
            }),
            text(" Enter: load | Escape: cancel ") | dim | center,
        });
    }
    
    // Progress selection dialog
    if (show_progress_dialog_) {
        Elements items;
        if (progress_list_.empty()) {
            items.push_back(text(" No saved progress found ") | dim);
        } else {
            for (size_t i = 0; i < progress_list_.size(); i++) {
                Element line = text(" " + progress_list_[i] + " ");
                if (static_cast<int>(i) == selected_progress_) {
                    line = line | inverted;
                }
                items.push_back(line);
            }
        }
        toolbar = vbox({
            toolbar,
            separator(),
            text(" Select saved progress: ") | bold,
            vbox(items) | border,
            text(" Up/Down: navigate | Enter: load | Escape: cancel ") | dim | center,
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

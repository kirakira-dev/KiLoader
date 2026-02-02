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
    // Create buttons for each menu with simple border style
    std::vector<Component> menu_buttons;
    
    for (size_t i = 0; i < menus_.size(); i++) {
        size_t idx = i;
        
        // Simple button style with border
        auto option = ButtonOption::Simple();
        option.transform = [this, idx](const EntryState& s) {
            Element el = text(" " + menus_[idx].label + " ");
            if (menu_open_ && selected_menu_ == static_cast<int>(idx)) {
                el = el | inverted;
            } else if (s.focused) {
                el = el | bold;
            }
            return el | border;
        };
        
        auto btn = Button(menus_[i].label, [this, idx]() {
            if (menu_open_ && selected_menu_ == static_cast<int>(idx)) {
                menu_open_ = false;
            } else {
                selected_menu_ = static_cast<int>(idx);
                menu_open_ = true;
                selected_item_ = 0;
            }
        }, option);
        menu_buttons.push_back(btn);
    }
    
    // Title label with border
    auto title = Renderer([] {
        return text(" KILOADER ") | bold | border;
    });
    
    // Combine title + buttons
    std::vector<Component> bar_components;
    bar_components.push_back(title);
    for (auto& btn : menu_buttons) {
        bar_components.push_back(btn);
    }
    
    auto menu_bar = Container::Horizontal(bar_components);
    
    // Wrap with renderer - menu bar only, dropdowns handled separately as overlay
    auto with_overlay = Renderer(menu_bar, [this, menu_bar]() {
        // Just render the menu bar - dropdowns are overlays handled by App
        return menu_bar->Render();
    });
    
    // Event handler
    return CatchEvent(with_overlay, [this](Event event) {
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
            // Mouse click on dropdown items
            if (event.is_mouse() && event.mouse().button == Mouse::Left &&
                event.mouse().motion == Mouse::Pressed) {
                int y = event.mouse().y;
                // Menu bar is line 0-2 (3 lines with border), dropdown starts at line 3
                // Inside dropdown: border top = line 3, first item = line 4, etc.
                int item_y = y - 4;  // Offset for menu bar + dropdown border
                if (item_y >= 0 && item_y < static_cast<int>(menus_[selected_menu_].items.size())) {
                    auto& item = menus_[selected_menu_].items[item_y];
                    if (!item.label.empty() && item.action) {
                        item.action();
                        menu_open_ = false;
                        return true;
                    }
                }
                // Click elsewhere closes menu
                menu_open_ = false;
                return true;
            }
            
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
}

Element Toolbar::render() {
    // Render overlay elements (dropdown menu, dialogs)
    // These will be positioned using dbox in App
    
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
        
        // Calculate horizontal offset
        int offset = 12;  // " KILOADER " + border
        for (int i = 0; i < selected_menu_; i++) {
            offset += menus_[i].label.length() + 4;
        }
        
        // Dropdown box
        Element dropdown = vbox(items) | border | bgcolor(Color::Black) | clear_under;
        
        // Position: offset from left, 3 lines down (menu bar height)
        return vbox({
            text(""),  // Line 1: top border of menu bar
            text(""),  // Line 2: menu bar content  
            text(""),  // Line 3: bottom border of menu bar
            hbox({
                text(std::string(offset, ' ')),
                dropdown,
                filler(),
            }),
            filler(),
        });
    }
    
    // File input dialog - centered overlay
    if (show_file_dialog_) {
        Element dialog = vbox({
            text(" Load NSO File ") | bold | center,
            separator(),
            hbox({
                text(" Path: ") | bold,
                text(file_input_) | bgcolor(Color::GrayDark),
                text("_") | blink,
            }) | size(WIDTH, EQUAL, 50),
            separator(),
            text(" Enter: load | Escape: cancel ") | dim | center,
        }) | border | bgcolor(Color::Black) | clear_under;
        
        // Center using filler pattern
        return vbox({
            filler(),
            hbox({
                filler(),
                dialog,
                filler(),
            }),
            filler(),
        });
    }
    
    // Progress selection dialog - centered overlay
    if (show_progress_dialog_) {
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
        Element dialog = vbox({
            text(" Load Progress ") | bold | center,
            separator(),
            vbox(items),
            separator(),
            text(" Up/Down | Enter | Escape ") | dim | center,
        }) | border | bgcolor(Color::Black) | clear_under;
        
        // Center using filler pattern
        return vbox({
            filler(),
            hbox({
                filler(),
                dialog,
                filler(),
            }),
            filler(),
        });
    }
    
    return text("");  // No overlay
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

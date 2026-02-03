#include "gui/function_view.h"
#include "gui/app.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace kiloader {
namespace gui {

FunctionView::FunctionView(App& app) : app_(app) {
}

void FunctionView::refresh() {
    functions_.clear();
    
    auto& funcs = app_.getAnalyzer().getFunctionFinder().getFunctions();
    for (const auto& [addr, func] : funcs) {
        functions_.push_back({addr, func.name});
    }
    
    // Sort by address
    std::sort(functions_.begin(), functions_.end());
    
    selected_ = 0;
    scroll_offset_ = 0;
}

void FunctionView::draw(WINDOW* win) {
    int height, width;
    getmaxyx(win, height, width);
    
    int visible_height = height - 2;  // Account for border
    int visible_width = width - 4;
    
    // Ensure scroll offset is valid
    if (selected_ < scroll_offset_) {
        scroll_offset_ = selected_;
    } else if (selected_ >= scroll_offset_ + visible_height) {
        scroll_offset_ = selected_ - visible_height + 1;
    }
    
    // Draw functions
    for (int i = 0; i < visible_height && scroll_offset_ + i < static_cast<int>(functions_.size()); i++) {
        int idx = scroll_offset_ + i;
        auto& [addr, name] = functions_[idx];
        
        std::stringstream ss;
        ss << std::hex << std::setw(10) << std::setfill('0') << addr;
        std::string addr_str = ss.str();
        
        std::string display = name;
        if (display.length() > static_cast<size_t>(visible_width - 12)) {
            display = display.substr(0, visible_width - 15) + "...";
        }
        
        if (idx == selected_) {
            wattron(win, A_REVERSE);
        }
        
        mvwprintw(win, 1 + i, 2, "%-*s", visible_width, display.c_str());
        
        if (idx == selected_) {
            wattroff(win, A_REVERSE);
        }
    }
    
    // Draw scroll indicator
    if (!functions_.empty()) {
        int scroll_height = std::max(1, visible_height * visible_height / static_cast<int>(functions_.size()));
        int scroll_pos = functions_.empty() ? 0 : 
            scroll_offset_ * (visible_height - scroll_height) / std::max(1, static_cast<int>(functions_.size()) - visible_height);
        
        for (int i = 0; i < visible_height; i++) {
            if (i >= scroll_pos && i < scroll_pos + scroll_height) {
                mvwaddch(win, 1 + i, width - 2, ACS_CKBOARD);
            } else {
                mvwaddch(win, 1 + i, width - 2, ACS_VLINE);
            }
        }
    }
    
    // Show count
    std::stringstream ss;
    ss << " " << functions_.size() << " funcs ";
    mvwprintw(win, height - 1, width - ss.str().length() - 2, "%s", ss.str().c_str());
}

void FunctionView::handleKey(int ch) {
    if (functions_.empty()) return;
    
    if (ch == KEY_DOWN || ch == 'j') {
        if (selected_ < static_cast<int>(functions_.size()) - 1) {
            selected_++;
            app_.setSelectedFunction(functions_[selected_].first);
        }
    } else if (ch == KEY_UP || ch == 'k') {
        if (selected_ > 0) {
            selected_--;
            app_.setSelectedFunction(functions_[selected_].first);
        }
    } else if (ch == KEY_PPAGE) {  // Page Up
        int height = 20;  // Approximate visible height
        selected_ = std::max(0, selected_ - height);
        app_.setSelectedFunction(functions_[selected_].first);
    } else if (ch == KEY_NPAGE) {  // Page Down
        int height = 20;
        selected_ = std::min(static_cast<int>(functions_.size()) - 1, selected_ + height);
        app_.setSelectedFunction(functions_[selected_].first);
    } else if (ch == KEY_HOME || ch == 'g') {
        selected_ = 0;
        scroll_offset_ = 0;
        if (!functions_.empty()) {
            app_.setSelectedFunction(functions_[selected_].first);
        }
    } else if (ch == KEY_END || ch == 'G') {
        selected_ = functions_.size() - 1;
        if (!functions_.empty()) {
            app_.setSelectedFunction(functions_[selected_].first);
        }
    } else if (ch == '\n' || ch == KEY_ENTER) {
        if (!functions_.empty()) {
            app_.setSelectedFunction(functions_[selected_].first);
        }
    }
}

void FunctionView::handleMouse(MEVENT& event) {
    if (event.bstate & BUTTON1_CLICKED || event.bstate & BUTTON1_PRESSED) {
        int clicked_idx = scroll_offset_ + event.y - 2;  // Account for border and offset
        if (clicked_idx >= 0 && clicked_idx < static_cast<int>(functions_.size())) {
            selected_ = clicked_idx;
            app_.setSelectedFunction(functions_[selected_].first);
        }
    }
#ifdef BUTTON4_PRESSED
    else if (event.bstate & BUTTON4_PRESSED) {  // Scroll up
        if (selected_ > 0) {
            selected_--;
            app_.setSelectedFunction(functions_[selected_].first);
        }
    }
#endif
#ifdef BUTTON5_PRESSED
    else if (event.bstate & BUTTON5_PRESSED) {  // Scroll down
        if (selected_ < static_cast<int>(functions_.size()) - 1) {
            selected_++;
            app_.setSelectedFunction(functions_[selected_].first);
        }
    }
#endif
}

} // namespace gui
} // namespace kiloader

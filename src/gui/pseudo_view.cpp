#include "gui/pseudo_view.h"
#include "gui/app.h"

#include <sstream>

namespace kiloader {
namespace gui {

PseudoView::PseudoView(App& app) : app_(app) {
    lines_.push_back("// Select a function to view pseudocode");
}

void PseudoView::setFunction(uint64_t addr) {
    if (addr == current_addr_) return;
    current_addr_ = addr;
    scroll_offset_ = 0;
    generatePseudocode();
}

void PseudoView::generatePseudocode() {
    lines_.clear();
    
    if (current_addr_ == 0) {
        lines_.push_back("// Select a function to view pseudocode");
        return;
    }
    
    std::string pseudo = app_.getAnalyzer().getPseudocodeAt(current_addr_);
    
    std::istringstream stream(pseudo);
    std::string line;
    while (std::getline(stream, line)) {
        lines_.push_back(line);
    }
    
    if (lines_.empty()) {
        lines_.push_back("// No pseudocode available");
    }
}

void PseudoView::draw(WINDOW* win) {
    int height, width;
    getmaxyx(win, height, width);
    
    int visible_height = height - 2;
    int visible_width = width - 3;
    
    bool show_line_nums = app_.getSettings().show_line_numbers;
    int line_num_width = show_line_nums ? 5 : 0;
    
    for (int i = 0; i < visible_height && scroll_offset_ + i < static_cast<int>(lines_.size()); i++) {
        int line_idx = scroll_offset_ + i;
        std::string& line = lines_[line_idx];
        
        // Line number
        if (show_line_nums) {
            wattron(win, COLOR_PAIR(3));
            mvwprintw(win, 1 + i, 1, "%4d ", line_idx + 1);
            wattroff(win, COLOR_PAIR(3));
        }
        
        // Line content
        std::string display = line;
        if (display.length() > static_cast<size_t>(visible_width - line_num_width)) {
            display = display.substr(0, visible_width - line_num_width - 3) + "...";
        }
        
        // Simple syntax highlighting
        if (line.find("//") != std::string::npos || line.find("/*") != std::string::npos) {
            wattron(win, COLOR_PAIR(2));
        } else if (line.find("return") != std::string::npos || 
                   line.find("if ") != std::string::npos ||
                   line.find("else") != std::string::npos ||
                   line.find("while") != std::string::npos ||
                   line.find("for ") != std::string::npos) {
            wattron(win, A_BOLD);
        }
        
        mvwprintw(win, 1 + i, 1 + line_num_width, "%s", display.c_str());
        
        wattroff(win, COLOR_PAIR(2));
        wattroff(win, A_BOLD);
    }
    
    // Scroll indicator
    if (!lines_.empty() && lines_.size() > static_cast<size_t>(visible_height)) {
        int scroll_height = std::max(1, visible_height * visible_height / static_cast<int>(lines_.size()));
        int scroll_pos = scroll_offset_ * (visible_height - scroll_height) / 
                         std::max(1, static_cast<int>(lines_.size()) - visible_height);
        
        for (int i = 0; i < visible_height; i++) {
            if (i >= scroll_pos && i < scroll_pos + scroll_height) {
                mvwaddch(win, 1 + i, width - 2, ACS_CKBOARD);
            }
        }
    }
}

void PseudoView::handleKey(int ch) {
    int height = 20;  // Approximate
    
    if (ch == KEY_DOWN || ch == 'j') {
        if (scroll_offset_ < static_cast<int>(lines_.size()) - height) {
            scroll_offset_++;
        }
    } else if (ch == KEY_UP || ch == 'k') {
        if (scroll_offset_ > 0) {
            scroll_offset_--;
        }
    } else if (ch == KEY_PPAGE) {
        scroll_offset_ = std::max(0, scroll_offset_ - height);
    } else if (ch == KEY_NPAGE) {
        scroll_offset_ = std::min(static_cast<int>(lines_.size()) - height, scroll_offset_ + height);
        if (scroll_offset_ < 0) scroll_offset_ = 0;
    } else if (ch == KEY_HOME || ch == 'g') {
        scroll_offset_ = 0;
    } else if (ch == KEY_END || ch == 'G') {
        scroll_offset_ = std::max(0, static_cast<int>(lines_.size()) - height);
    }
}

} // namespace gui
} // namespace kiloader

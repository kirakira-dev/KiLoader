#include "gui/search_dialog.h"
#include "gui/app.h"

#include <sstream>
#include <iomanip>

namespace kiloader {
namespace gui {

SearchDialog::SearchDialog(App& app) : app_(app) {
}

SearchDialog::~SearchDialog() {
    hide();
}

void SearchDialog::show(SearchType type) {
    type_ = type;
    visible_ = true;
    query_.clear();
    results_.clear();
    selected_ = 0;
    scroll_offset_ = 0;
    
    // Create centered window
    int height = 20;
    int width = 60;
    int starty = (app_.getHeight() - height) / 2;
    int startx = (app_.getWidth() - width) / 2;
    
    win_ = newwin(height, width, starty, startx);
    keypad(win_, TRUE);
}

void SearchDialog::hide() {
    visible_ = false;
    if (win_) {
        werase(win_);
        wrefresh(win_);
        delwin(win_);
        win_ = nullptr;
    }
}

void SearchDialog::draw() {
    if (!visible_ || !win_) return;
    
    werase(win_);
    box(win_, 0, 0);
    
    // Title
    const char* title = "Search";
    switch (type_) {
        case SearchType::Strings: title = " Search Strings "; break;
        case SearchType::Assembly: title = " Search Assembly "; break;
        case SearchType::RawHex: title = " Search Hex "; break;
        case SearchType::Pseudocode: title = " Search Pseudocode "; break;
    }
    wattron(win_, A_BOLD);
    mvwprintw(win_, 0, 2, "%s", title);
    wattroff(win_, A_BOLD);
    
    // Query input
    mvwprintw(win_, 2, 2, "Query: ");
    wattron(win_, A_REVERSE);
    mvwprintw(win_, 2, 9, "%-48s", query_.c_str());
    wattroff(win_, A_REVERSE);
    
    mvwhline(win_, 3, 1, ACS_HLINE, 58);
    
    // Results
    int visible_height = 14;
    for (int i = 0; i < visible_height && scroll_offset_ + i < static_cast<int>(results_.size()); i++) {
        int idx = scroll_offset_ + i;
        
        if (idx == selected_) {
            wattron(win_, A_REVERSE);
        }
        
        std::string line = results_[idx];
        if (line.length() > 56) {
            line = line.substr(0, 53) + "...";
        }
        mvwprintw(win_, 4 + i, 2, "%-56s", line.c_str());
        
        if (idx == selected_) {
            wattroff(win_, A_REVERSE);
        }
    }
    
    // Footer
    mvwhline(win_, 18, 1, ACS_HLINE, 58);
    mvwprintw(win_, 19, 2, " Enter: search | Up/Down: select | Esc: close ");
    
    wrefresh(win_);
}

void SearchDialog::handleKey(int ch) {
    if (!visible_) return;
    
    if (ch == 27) {  // ESC
        hide();
    } else if (ch == '\n' || ch == KEY_ENTER) {
        if (results_.empty()) {
            performSearch();
        } else if (selected_ >= 0 && selected_ < static_cast<int>(results_.size())) {
            // Navigate to selected result
            // Parse address from result line
            std::string line = results_[selected_];
            size_t pos = line.find("0x");
            if (pos != std::string::npos) {
                std::string addr_str = line.substr(pos + 2, 16);
                size_t end = addr_str.find_first_not_of("0123456789abcdefABCDEF");
                if (end != std::string::npos) {
                    addr_str = addr_str.substr(0, end);
                }
                uint64_t addr = std::stoull(addr_str, nullptr, 16);
                app_.setSelectedFunction(addr);
                hide();
            }
        }
    } else if (ch == KEY_DOWN) {
        if (selected_ < static_cast<int>(results_.size()) - 1) {
            selected_++;
            if (selected_ >= scroll_offset_ + 14) {
                scroll_offset_++;
            }
        }
    } else if (ch == KEY_UP) {
        if (selected_ > 0) {
            selected_--;
            if (selected_ < scroll_offset_) {
                scroll_offset_--;
            }
        }
    } else if (ch == KEY_BACKSPACE || ch == 127) {
        if (!query_.empty()) {
            query_.pop_back();
            results_.clear();
        }
    } else if (ch >= 32 && ch < 127) {
        query_ += static_cast<char>(ch);
        results_.clear();
    }
}

void SearchDialog::performSearch() {
    results_.clear();
    selected_ = 0;
    scroll_offset_ = 0;
    
    if (query_.empty()) return;
    
    if (type_ == SearchType::Strings) {
        auto matches = app_.getAnalyzer().searchStrings(query_);
        for (const auto& entry : matches) {
            std::stringstream ss;
            ss << "0x" << std::hex << std::setw(10) << std::setfill('0') << entry.address;
            ss << ": " << entry.value.substr(0, 40);
            results_.push_back(ss.str());
            
            if (results_.size() >= 100) break;
        }
    }
    
    if (results_.empty()) {
        results_.push_back("No results found");
    }
}

} // namespace gui
} // namespace kiloader

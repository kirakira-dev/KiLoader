#include "gui/pseudo_view.h"
#include "gui/app.h"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

#include <sstream>

namespace kiloader {
namespace gui {

using namespace ftxui;

PseudoView::PseudoView(App& app) : app_(app) {}

void PseudoView::setFunction(uint64_t addr) {
    if (addr == current_function_) return;
    
    current_function_ = addr;
    refresh();
}

void PseudoView::refresh() {
    lines_.clear();
    scroll_offset_ = 0;
    selected_line_ = 0;
    
    if (current_function_ == 0) {
        content_ = "// Select a function to view pseudocode";
        lines_.push_back(content_);
        return;
    }
    
    content_ = app_.getAnalyzer().getPseudocodeAt(current_function_);
    
    // Split into lines
    std::istringstream stream(content_);
    std::string line;
    while (std::getline(stream, line)) {
        lines_.push_back(line);
    }
    
    if (lines_.empty()) {
        lines_.push_back("// No pseudocode available");
    }
}

Component PseudoView::getComponent() {
    return CatchEvent(Renderer([this]{ return render(); }), [this](Event event) {
        if (lines_.empty()) return false;
        
        int visible_count = 30;
        
        if (event == Event::ArrowDown) {
            if (selected_line_ < static_cast<int>(lines_.size()) - 1) {
                selected_line_++;
                if (selected_line_ >= scroll_offset_ + visible_count) {
                    scroll_offset_ = selected_line_ - visible_count + 1;
                }
            }
            return true;
        }
        
        if (event == Event::ArrowUp) {
            if (selected_line_ > 0) {
                selected_line_--;
                if (selected_line_ < scroll_offset_) {
                    scroll_offset_ = selected_line_;
                }
            }
            return true;
        }
        
        if (event == Event::PageDown) {
            selected_line_ = std::min(selected_line_ + visible_count, 
                                     static_cast<int>(lines_.size()) - 1);
            scroll_offset_ = std::max(0, selected_line_ - visible_count + 1);
            return true;
        }
        
        if (event == Event::PageUp) {
            selected_line_ = std::max(selected_line_ - visible_count, 0);
            scroll_offset_ = selected_line_;
            return true;
        }
        
        return false;
    });
}

Element PseudoView::render() {
    if (lines_.empty()) {
        return text(" No content ") | dim | center;
    }
    
    Elements items;
    bool show_line_numbers = app_.getSettings().show_line_numbers;
    
    int visible_count = 35;
    int end = std::min(scroll_offset_ + visible_count, 
                       static_cast<int>(lines_.size()));
    
    // Calculate line number width
    int line_num_width = std::to_string(lines_.size()).length();
    
    for (int i = scroll_offset_; i < end; i++) {
        items.push_back(renderLine(i + 1, lines_[i]));
    }
    
    return vbox(items) | frame | flex;
}

Element PseudoView::renderLine(int line_num, const std::string& line) {
    Elements parts;
    bool show_line_numbers = app_.getSettings().show_line_numbers;
    
    if (show_line_numbers) {
        int width = std::to_string(lines_.size()).length();
        std::string num_str = std::to_string(line_num);
        while (num_str.length() < static_cast<size_t>(width)) {
            num_str = " " + num_str;
        }
        parts.push_back(text(num_str + " ") | dim);
        parts.push_back(text("| ") | dim);
    }
    
    // Simple syntax highlighting
    std::string text_line = line;
    
    // Keywords
    Element line_elem;
    
    // Check for comments
    if (line.find("//") != std::string::npos) {
        size_t pos = line.find("//");
        parts.push_back(text(line.substr(0, pos)));
        parts.push_back(text(line.substr(pos)) | color(Color::Green));
    }
    // Check for function header
    else if (line.find("function") != std::string::npos || 
             line.find("void ") == 0 ||
             line.find("int ") == 0 ||
             line.find("uint") == 0) {
        parts.push_back(text(line) | color(Color::Yellow));
    }
    // Check for return
    else if (line.find("return") != std::string::npos) {
        parts.push_back(text(line) | color(Color::Magenta));
    }
    // Check for control flow
    else if (line.find("if ") != std::string::npos ||
             line.find("else") != std::string::npos ||
             line.find("while") != std::string::npos ||
             line.find("for ") != std::string::npos) {
        parts.push_back(text(line) | color(Color::Cyan));
    }
    else {
        parts.push_back(text(line));
    }
    
    Element result = hbox(parts);
    
    if (line_num - 1 == selected_line_) {
        result = result | inverted;
    }
    
    return result;
}

} // namespace gui
} // namespace kiloader

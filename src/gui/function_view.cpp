#include "gui/function_view.h"
#include "gui/app.h"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace kiloader {
namespace gui {

using namespace ftxui;

FunctionView::FunctionView(App& app) : app_(app) {}

void FunctionView::refresh() {
    functions_.clear();
    filtered_indices_.clear();
    
    auto& funcs = app_.getAnalyzer().getFunctionFinder().getFunctions();
    
    for (const auto& [addr, func] : funcs) {
        functions_.push_back({addr, func.name});
    }
    
    applyFilter();
    selected_ = 0;
    scroll_offset_ = 0;
}

void FunctionView::setFilter(const std::string& filter) {
    filter_ = filter;
    applyFilter();
    selected_ = 0;
    scroll_offset_ = 0;
}

void FunctionView::applyFilter() {
    filtered_indices_.clear();
    
    if (filter_.empty()) {
        for (size_t i = 0; i < functions_.size(); i++) {
            filtered_indices_.push_back(i);
        }
    } else {
        std::string lower_filter = filter_;
        std::transform(lower_filter.begin(), lower_filter.end(), 
                      lower_filter.begin(), ::tolower);
        
        for (size_t i = 0; i < functions_.size(); i++) {
            std::string lower_name = functions_[i].second;
            std::transform(lower_name.begin(), lower_name.end(), 
                          lower_name.begin(), ::tolower);
            
            if (lower_name.find(lower_filter) != std::string::npos) {
                filtered_indices_.push_back(i);
            }
        }
    }
}

uint64_t FunctionView::getSelectedAddress() const {
    if (filtered_indices_.empty()) return 0;
    if (selected_ >= static_cast<int>(filtered_indices_.size())) return 0;
    return functions_[filtered_indices_[selected_]].first;
}

Component FunctionView::getComponent() {
    return CatchEvent(Renderer([this]{ return render(); }), [this](Event event) {
        if (filtered_indices_.empty()) return false;
        
        int visible_count = 20;  // Approximate visible lines
        
        if (event == Event::ArrowDown) {
            if (selected_ < static_cast<int>(filtered_indices_.size()) - 1) {
                selected_++;
                // Scroll if needed
                if (selected_ >= scroll_offset_ + visible_count) {
                    scroll_offset_ = selected_ - visible_count + 1;
                }
                app_.setSelectedFunction(getSelectedAddress());
            }
            return true;
        }
        
        if (event == Event::ArrowUp) {
            if (selected_ > 0) {
                selected_--;
                if (selected_ < scroll_offset_) {
                    scroll_offset_ = selected_;
                }
                app_.setSelectedFunction(getSelectedAddress());
            }
            return true;
        }
        
        if (event == Event::PageDown) {
            selected_ = std::min(selected_ + visible_count, 
                                static_cast<int>(filtered_indices_.size()) - 1);
            scroll_offset_ = std::max(0, selected_ - visible_count + 1);
            app_.setSelectedFunction(getSelectedAddress());
            return true;
        }
        
        if (event == Event::PageUp) {
            selected_ = std::max(selected_ - visible_count, 0);
            scroll_offset_ = selected_;
            app_.setSelectedFunction(getSelectedAddress());
            return true;
        }
        
        if (event == Event::Home) {
            selected_ = 0;
            scroll_offset_ = 0;
            app_.setSelectedFunction(getSelectedAddress());
            return true;
        }
        
        if (event == Event::End) {
            selected_ = static_cast<int>(filtered_indices_.size()) - 1;
            scroll_offset_ = std::max(0, selected_ - visible_count + 1);
            app_.setSelectedFunction(getSelectedAddress());
            return true;
        }
        
        if (event == Event::Return) {
            app_.setSelectedFunction(getSelectedAddress());
            return true;
        }
        
        return false;
    });
}

Element FunctionView::render() {
    if (functions_.empty()) {
        return text(" No functions loaded ") | dim | center;
    }
    
    Elements items;
    
    int visible_count = 25;
    int end = std::min(scroll_offset_ + visible_count, 
                       static_cast<int>(filtered_indices_.size()));
    
    for (int i = scroll_offset_; i < end; i++) {
        size_t idx = filtered_indices_[i];
        auto& [addr, name] = functions_[idx];
        
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(10) << addr;
        
        Element line = hbox({
            text(ss.str()) | dim,
            text(" "),
            text(name) | flex,
        });
        
        if (i == selected_) {
            line = line | inverted | focus;
        }
        
        items.push_back(line);
    }
    
    // Scrollbar indicator
    if (filtered_indices_.size() > static_cast<size_t>(visible_count)) {
        int total = filtered_indices_.size();
        int pos = scroll_offset_ * 100 / total;
        items.push_back(separator());
        items.push_back(text(" [" + std::to_string(selected_ + 1) + "/" + 
                            std::to_string(total) + "] ") | dim);
    }
    
    return vbox(items) | frame | flex;
}

} // namespace gui
} // namespace kiloader

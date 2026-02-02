#include "gui/search_dialog.h"
#include "gui/app.h"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

#include <sstream>
#include <iomanip>
#include <algorithm>

namespace kiloader {
namespace gui {

using namespace ftxui;

SearchDialog::SearchDialog(App& app) : app_(app) {}

void SearchDialog::show(SearchType type) {
    search_type_ = type;
    visible_ = true;
    query_.clear();
    results_.clear();
    selected_result_ = 0;
    scroll_offset_ = 0;
}

void SearchDialog::hide() {
    visible_ = false;
}

void SearchDialog::search() {
    results_.clear();
    selected_result_ = 0;
    scroll_offset_ = 0;
    
    if (query_.empty()) return;
    
    switch (search_type_) {
        case SearchType::Strings:
            searchStrings();
            break;
        case SearchType::Assembly:
            searchAssembly();
            break;
        case SearchType::RawHex:
            searchRawHex();
            break;
        case SearchType::Pseudocode:
            searchPseudocode();
            break;
    }
    
    app_.setStatus("Found " + std::to_string(results_.size()) + " results");
}

void SearchDialog::searchStrings() {
    auto matches = app_.getAnalyzer().searchStrings(query_);
    
    for (const auto& entry : matches) {
        SearchResult result;
        result.address = entry.address;
        result.match = entry.value;
        
        // Truncate long strings
        if (result.match.length() > 60) {
            result.match = result.match.substr(0, 57) + "...";
        }
        
        // Escape special characters
        for (char& c : result.match) {
            if (c == '\n') c = ' ';
            if (c == '\r') c = ' ';
            if (c == '\t') c = ' ';
        }
        
        result.context = "string";
        results_.push_back(result);
        
        if (results_.size() >= 1000) break;  // Limit results
    }
}

void SearchDialog::searchAssembly() {
    // Search through all function instructions
    auto& funcs = app_.getAnalyzer().getFunctionFinder().getFunctions();
    
    std::string lower_query = query_;
    std::transform(lower_query.begin(), lower_query.end(), 
                  lower_query.begin(), ::tolower);
    
    for (const auto& [addr, func] : funcs) {
        for (const auto& insn : func.instructions) {
            std::string insn_str = insn.mnemonic + " " + insn.operands;
            std::string lower_insn = insn_str;
            std::transform(lower_insn.begin(), lower_insn.end(), 
                          lower_insn.begin(), ::tolower);
            
            if (lower_insn.find(lower_query) != std::string::npos) {
                SearchResult result;
                result.address = insn.address;
                result.match = insn_str;
                result.context = func.name;
                results_.push_back(result);
                
                if (results_.size() >= 1000) return;
            }
        }
    }
}

void SearchDialog::searchRawHex() {
    // Convert hex string to bytes
    std::vector<uint8_t> search_bytes;
    std::string hex = query_;
    
    // Remove spaces
    hex.erase(std::remove(hex.begin(), hex.end(), ' '), hex.end());
    
    if (hex.length() % 2 != 0) {
        app_.setStatus("Invalid hex string (must be even length)");
        return;
    }
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        try {
            uint8_t byte = std::stoi(hex.substr(i, 2), nullptr, 16);
            search_bytes.push_back(byte);
        } catch (...) {
            app_.setStatus("Invalid hex character");
            return;
        }
    }
    
    if (search_bytes.empty()) return;
    
    // Search in text segment
    auto& text = app_.getAnalyzer().getNso().getTextSegment();
    uint64_t base = app_.getAnalyzer().getNso().getBaseAddress() + text.mem_offset;
    
    for (size_t i = 0; i + search_bytes.size() <= text.size; i++) {
        bool match = true;
        for (size_t j = 0; j < search_bytes.size(); j++) {
            if (text.data[i + j] != search_bytes[j]) {
                match = false;
                break;
            }
        }
        
        if (match) {
            SearchResult result;
            result.address = base + i;
            result.match = query_;
            result.context = ".text";
            results_.push_back(result);
            
            if (results_.size() >= 1000) break;
        }
    }
}

void SearchDialog::searchPseudocode() {
    // Search through generated pseudocode of all functions
    auto& funcs = app_.getAnalyzer().getFunctionFinder().getFunctions();
    
    std::string lower_query = query_;
    std::transform(lower_query.begin(), lower_query.end(), 
                  lower_query.begin(), ::tolower);
    
    for (const auto& [addr, func] : funcs) {
        std::string pseudo = app_.getAnalyzer().getPseudocodeAt(addr);
        std::string lower_pseudo = pseudo;
        std::transform(lower_pseudo.begin(), lower_pseudo.end(), 
                      lower_pseudo.begin(), ::tolower);
        
        if (lower_pseudo.find(lower_query) != std::string::npos) {
            SearchResult result;
            result.address = addr;
            result.match = func.name;
            result.context = "pseudocode match";
            results_.push_back(result);
            
            if (results_.size() >= 500) return;  // Lower limit for pseudocode search
        }
    }
}

void SearchDialog::goToResult(int index) {
    if (index < 0 || index >= static_cast<int>(results_.size())) return;
    
    auto& result = results_[index];
    app_.setSelectedFunction(result.address);
    app_.setStatus("Jumped to 0x" + 
        ([](uint64_t a) {
            std::stringstream ss;
            ss << std::hex << a;
            return ss.str();
        })(result.address));
}

Component SearchDialog::getComponent() {
    return CatchEvent(Renderer([this]{ return render(); }), [this](Event event) {
        if (!visible_) return false;
        
        // Text input
        if (event.is_character()) {
            query_ += event.character();
            return true;
        }
        
        if (event == Event::Backspace && !query_.empty()) {
            query_.pop_back();
            return true;
        }
        
        if (event == Event::Return) {
            if (results_.empty()) {
                search();
            } else {
                goToResult(selected_result_);
                hide();
            }
            return true;
        }
        
        if (event == Event::Escape) {
            hide();
            return true;
        }
        
        // Navigate results
        if (event == Event::ArrowDown && !results_.empty()) {
            selected_result_ = std::min(selected_result_ + 1, 
                                       static_cast<int>(results_.size()) - 1);
            if (selected_result_ >= scroll_offset_ + 10) {
                scroll_offset_ = selected_result_ - 9;
            }
            return true;
        }
        
        if (event == Event::ArrowUp && !results_.empty()) {
            selected_result_ = std::max(selected_result_ - 1, 0);
            if (selected_result_ < scroll_offset_) {
                scroll_offset_ = selected_result_;
            }
            return true;
        }
        
        // Tab to search
        if (event == Event::Tab) {
            search();
            return true;
        }
        
        return true;  // Consume all events when dialog is open
    });
}

Element SearchDialog::render() {
    if (!visible_) return text("");
    
    std::string title;
    switch (search_type_) {
        case SearchType::Strings: title = "Search Strings"; break;
        case SearchType::Assembly: title = "Search Assembly"; break;
        case SearchType::RawHex: title = "Search Raw Hex"; break;
        case SearchType::Pseudocode: title = "Search Pseudocode"; break;
    }
    
    Elements content;
    content.push_back(text(" " + title + " ") | bold | center);
    content.push_back(separator());
    
    // Input field
    content.push_back(
        hbox({
            text(" Query: "),
            text(query_ + "_") | inverted | flex,
        })
    );
    content.push_back(separator());
    
    // Results
    if (results_.empty() && !query_.empty()) {
        content.push_back(text(" Press Tab or Enter to search ") | dim | center);
    } else if (!results_.empty()) {
        content.push_back(text(" Results: " + std::to_string(results_.size()) + " ") | dim);
        content.push_back(separator());
        
        int end = std::min(scroll_offset_ + 10, static_cast<int>(results_.size()));
        for (int i = scroll_offset_; i < end; i++) {
            auto& result = results_[i];
            
            std::stringstream addr_ss;
            addr_ss << std::hex << std::setfill('0') << std::setw(10) << result.address;
            
            Element line = hbox({
                text(addr_ss.str()) | color(Color::Blue),
                text(" "),
                text(result.match) | flex,
            });
            
            if (i == selected_result_) {
                line = line | inverted;
            }
            
            content.push_back(line);
        }
    } else {
        content.push_back(text(" Enter search query ") | dim | center);
    }
    
    content.push_back(separator());
    content.push_back(text(" Enter: Search/Go | Esc: Close | Arrows: Navigate ") | dim);
    
    return vbox(content) | border | size(WIDTH, EQUAL, 60) | size(HEIGHT, LESS_THAN, 20);
}

} // namespace gui
} // namespace kiloader

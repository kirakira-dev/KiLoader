#include "gui/disasm_view.h"
#include "gui/app.h"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

#include <sstream>
#include <iomanip>

namespace kiloader {
namespace gui {

using namespace ftxui;

DisasmView::DisasmView(App& app) : app_(app) {}

void DisasmView::setFunction(uint64_t addr) {
    if (addr == 0) return;
    
    auto* func = app_.getAnalyzer().getFunctionAt(addr);
    if (func) {
        current_address_ = addr;
        instructions_ = func->instructions;
    } else {
        setAddress(addr, 50);
    }
    
    scroll_offset_ = 0;
    selected_line_ = 0;
}

void DisasmView::setAddress(uint64_t addr, size_t count) {
    current_address_ = addr;
    instructions_ = app_.getAnalyzer().disassembleAt(addr, count);
    scroll_offset_ = 0;
    selected_line_ = 0;
}

void DisasmView::refresh() {
    if (current_address_ != 0) {
        setAddress(current_address_, 50);
    }
}

Component DisasmView::getComponent() {
    return CatchEvent(Renderer([this]{ return render(); }), [this](Event event) {
        if (instructions_.empty()) return false;
        
        int visible_count = 30;
        
        if (event == Event::ArrowDown) {
            if (selected_line_ < static_cast<int>(instructions_.size()) - 1) {
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
                                     static_cast<int>(instructions_.size()) - 1);
            scroll_offset_ = std::max(0, selected_line_ - visible_count + 1);
            return true;
        }
        
        if (event == Event::PageUp) {
            selected_line_ = std::max(selected_line_ - visible_count, 0);
            scroll_offset_ = selected_line_;
            return true;
        }
        
        // Follow branch on Enter
        if (event == Event::Return) {
            if (selected_line_ < static_cast<int>(instructions_.size())) {
                auto& insn = instructions_[selected_line_];
                if (insn.branch_target != 0) {
                    setAddress(insn.branch_target, 50);
                    app_.setStatus("Jumped to 0x" + 
                        ([](uint64_t a) {
                            std::stringstream ss;
                            ss << std::hex << a;
                            return ss.str();
                        })(insn.branch_target));
                }
            }
            return true;
        }
        
        return false;
    });
}

Element DisasmView::render() {
    if (instructions_.empty()) {
        return text(" Select a function to view disassembly ") | dim | center;
    }
    
    Elements items;
    
    int visible_count = 35;
    int end = std::min(scroll_offset_ + visible_count, 
                       static_cast<int>(instructions_.size()));
    
    for (int i = scroll_offset_; i < end; i++) {
        items.push_back(renderInstruction(instructions_[i], i == selected_line_));
    }
    
    return vbox(items) | frame | flex;
}

Element DisasmView::renderInstruction(const Instruction& insn, bool selected) {
    Elements parts;
    
    // Address
    std::stringstream addr_ss;
    addr_ss << std::hex << std::setfill('0') << std::setw(10) << insn.address;
    parts.push_back(text(addr_ss.str()) | color(Color::Blue));
    parts.push_back(text("  "));
    
    // Bytes
    std::string bytes_str;
    for (size_t i = 0; i < insn.bytes.size() && i < 4; i++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X", insn.bytes[i]);
        bytes_str += buf;
    }
    while (bytes_str.length() < 8) bytes_str += " ";
    parts.push_back(text(bytes_str) | dim);
    parts.push_back(text("  "));
    
    // Mnemonic
    std::string mnemonic = insn.mnemonic;
    while (mnemonic.length() < 8) mnemonic += " ";
    
    Color mnemonic_color = Color::White;
    if (insn.is_call) {
        mnemonic_color = Color::Green;
    } else if (insn.is_branch) {
        mnemonic_color = Color::Yellow;
    } else if (insn.is_return) {
        mnemonic_color = Color::Red;
    } else if (insn.is_load || insn.is_store) {
        mnemonic_color = Color::Cyan;
    }
    
    parts.push_back(text(mnemonic) | color(mnemonic_color));
    parts.push_back(text(" "));
    
    // Operands
    parts.push_back(text(insn.operands));
    
    // Branch target indicator
    if (insn.branch_target != 0) {
        std::stringstream target_ss;
        target_ss << " -> 0x" << std::hex << insn.branch_target;
        parts.push_back(text(target_ss.str()) | dim);
    }
    
    Element result = hbox(parts);
    
    if (selected) {
        result = result | inverted;
    }
    
    return result;
}

} // namespace gui
} // namespace kiloader

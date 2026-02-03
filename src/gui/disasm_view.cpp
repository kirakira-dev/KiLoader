#include "gui/disasm_view.h"
#include "gui/app.h"

#include <sstream>
#include <iomanip>

namespace kiloader {
namespace gui {

DisasmView::DisasmView(App& app) : app_(app) {
}

void DisasmView::setFunction(uint64_t addr) {
    if (addr == current_addr_) return;
    current_addr_ = addr;
    scroll_offset_ = 0;
    selected_ = 0;
    disassemble();
}

void DisasmView::disassemble() {
    lines_.clear();
    
    if (current_addr_ == 0) return;
    
    // Get function info
    auto* func = app_.getAnalyzer().getFunctionAt(current_addr_);
    size_t count = 100;
    if (func && func->size > 0) {
        count = func->size / 4 + 1;
    }
    
    auto insns = app_.getAnalyzer().disassembleAt(current_addr_, count);
    
    for (const auto& insn : insns) {
        DisasmLine line;
        line.address = insn.address;
        line.mnemonic = insn.mnemonic;
        line.operands = insn.operands;
        
        // Format bytes
        std::stringstream ss;
        for (size_t i = 0; i < insn.bytes.size() && i < 8; i++) {
            ss << std::hex << std::setfill('0') << std::setw(2) 
               << static_cast<int>(insn.bytes[i]);
        }
        line.bytes = ss.str();
        
        lines_.push_back(line);
    }
}

void DisasmView::draw(WINDOW* win) {
    int height, width;
    getmaxyx(win, height, width);
    
    int visible_height = height - 2;
    
    // Ensure scroll offset is valid
    if (selected_ < scroll_offset_) {
        scroll_offset_ = selected_;
    } else if (selected_ >= scroll_offset_ + visible_height) {
        scroll_offset_ = selected_ - visible_height + 1;
    }
    
    for (int i = 0; i < visible_height && scroll_offset_ + i < static_cast<int>(lines_.size()); i++) {
        int idx = scroll_offset_ + i;
        auto& line = lines_[idx];
        
        // Address
        std::stringstream ss;
        ss << std::hex << std::setw(10) << std::setfill('0') << line.address;
        
        if (idx == selected_) {
            wattron(win, A_REVERSE);
        }
        
        // Address in cyan
        wattron(win, COLOR_PAIR(1));
        mvwprintw(win, 1 + i, 1, "%s", ss.str().c_str());
        wattroff(win, COLOR_PAIR(1));
        
        // Mnemonic
        wattron(win, A_BOLD);
        mvwprintw(win, 1 + i, 13, "%-8s", line.mnemonic.c_str());
        wattroff(win, A_BOLD);
        
        // Operands
        std::string ops = line.operands;
        int max_ops = width - 25;
        if (ops.length() > static_cast<size_t>(max_ops)) {
            ops = ops.substr(0, max_ops - 3) + "...";
        }
        mvwprintw(win, 1 + i, 22, "%s", ops.c_str());
        
        if (idx == selected_) {
            wattroff(win, A_REVERSE);
        }
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

void DisasmView::handleKey(int ch) {
    if (lines_.empty()) return;
    
    int height = 20;  // Approximate
    
    if (ch == KEY_DOWN || ch == 'j') {
        if (selected_ < static_cast<int>(lines_.size()) - 1) {
            selected_++;
        }
    } else if (ch == KEY_UP || ch == 'k') {
        if (selected_ > 0) {
            selected_--;
        }
    } else if (ch == KEY_PPAGE) {
        selected_ = std::max(0, selected_ - height);
    } else if (ch == KEY_NPAGE) {
        selected_ = std::min(static_cast<int>(lines_.size()) - 1, selected_ + height);
    } else if (ch == KEY_HOME || ch == 'g') {
        selected_ = 0;
        scroll_offset_ = 0;
    } else if (ch == KEY_END || ch == 'G') {
        selected_ = lines_.size() - 1;
    } else if (ch == '\n' || ch == KEY_ENTER) {
        // Follow branch/call
        if (selected_ >= 0 && selected_ < static_cast<int>(lines_.size())) {
            auto& line = lines_[selected_];
            // Check if operand looks like an address
            if (line.operands.find("0x") != std::string::npos || 
                line.operands.find("#") != std::string::npos) {
                // Try to parse address from operands
                std::string ops = line.operands;
                size_t pos = ops.find("0x");
                if (pos != std::string::npos) {
                    std::string addr_str = ops.substr(pos + 2);
                    size_t end = addr_str.find_first_not_of("0123456789abcdefABCDEF");
                    if (end != std::string::npos) {
                        addr_str = addr_str.substr(0, end);
                    }
                    uint64_t addr = std::stoull(addr_str, nullptr, 16);
                    app_.setSelectedFunction(addr);
                }
            }
        }
    }
}

} // namespace gui
} // namespace kiloader

#pragma once

#include <string>
#include <vector>
#include "ftxui/component/component.hpp"
#include "disassembler.h"

namespace kiloader {
namespace gui {

class App;

// Disassembly view
class DisasmView {
public:
    DisasmView(App& app);
    
    // Get FTXUI component
    ftxui::Component getComponent();
    
    // Render the view
    ftxui::Element render();
    
    // Set function/address to display
    void setFunction(uint64_t addr);
    void setAddress(uint64_t addr, size_t count = 100);
    
private:
    App& app_;
    
    uint64_t current_address_ = 0;
    std::vector<Instruction> instructions_;
    
    int scroll_offset_ = 0;
    int selected_line_ = 0;
    
    void refresh();
    ftxui::Element renderInstruction(const Instruction& insn, bool selected);
};

} // namespace gui
} // namespace kiloader

#pragma once

#include <ncurses.h>
#include <string>
#include <vector>
#include <cstdint>

namespace kiloader {
namespace gui {

class App;

struct DisasmLine {
    uint64_t address;
    std::string bytes;
    std::string mnemonic;
    std::string operands;
};

class DisasmView {
public:
    DisasmView(App& app);
    ~DisasmView() = default;
    
    void draw(WINDOW* win);
    void handleKey(int ch);
    void setFunction(uint64_t addr);
    
private:
    void disassemble();
    
    App& app_;
    
    uint64_t current_addr_ = 0;
    std::vector<DisasmLine> lines_;
    int scroll_offset_ = 0;
    int selected_ = 0;
};

} // namespace gui
} // namespace kiloader

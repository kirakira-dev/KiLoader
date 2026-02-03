#pragma once

#include <ncurses.h>
#include <string>
#include <vector>
#include <cstdint>

namespace kiloader {
namespace gui {

class App;

class PseudoView {
public:
    PseudoView(App& app);
    ~PseudoView() = default;
    
    void draw(WINDOW* win);
    void handleKey(int ch);
    void setFunction(uint64_t addr);
    
private:
    void generatePseudocode();
    
    App& app_;
    
    uint64_t current_addr_ = 0;
    std::vector<std::string> lines_;
    int scroll_offset_ = 0;
};

} // namespace gui
} // namespace kiloader

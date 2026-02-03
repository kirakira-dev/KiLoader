#pragma once

#include <ncurses.h>
#include <string>
#include <vector>

namespace kiloader {
namespace gui {

class App;

class FunctionView {
public:
    FunctionView(App& app);
    ~FunctionView() = default;
    
    void draw(WINDOW* win);
    void handleKey(int ch);
    void handleMouse(MEVENT& event);
    void refresh();
    
private:
    App& app_;
    
    std::vector<std::pair<uint64_t, std::string>> functions_;
    int selected_ = 0;
    int scroll_offset_ = 0;
    std::string filter_;
};

} // namespace gui
} // namespace kiloader

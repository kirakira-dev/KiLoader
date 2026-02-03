#include "gui/app.h"
#include "gui/toolbar.h"
#include "gui/function_view.h"
#include "gui/pseudo_view.h"
#include "gui/disasm_view.h"
#include "gui/search_dialog.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace kiloader {
namespace gui {

App::App() {
    initNcurses();
    createWindows();
    
    toolbar_ = std::make_unique<Toolbar>(*this);
    function_view_ = std::make_unique<FunctionView>(*this);
    pseudo_view_ = std::make_unique<PseudoView>(*this);
    disasm_view_ = std::make_unique<DisasmView>(*this);
    search_dialog_ = std::make_unique<SearchDialog>(*this);
    
    command_output_.push_back("KILOADER - Type ':' for command mode, F1-F4 for menus");
    command_output_.push_back("");
}

App::~App() {
    destroyWindows();
    cleanupNcurses();
}

void App::initNcurses() {
    initscr();
    start_color();
    use_default_colors();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
    curs_set(0);  // Hide cursor
    
    // Define color pairs
    init_pair(1, COLOR_CYAN, -1);     // Title
    init_pair(2, COLOR_GREEN, -1);    // Highlighted
    init_pair(3, COLOR_YELLOW, -1);   // Commands
    init_pair(4, COLOR_WHITE, COLOR_BLUE);  // Menu bar
    init_pair(5, COLOR_BLACK, COLOR_WHITE); // Selected
    init_pair(6, COLOR_RED, -1);      // Error
    
    getmaxyx(stdscr, height_, width_);
}

void App::cleanupNcurses() {
    endwin();
}

void App::createWindows() {
    // Menu bar at top (1 line)
    menu_win_ = newwin(1, width_, 0, 0);
    keypad(menu_win_, TRUE);
    
    // Calculate layout
    int func_width = 35;
    int content_height = height_ - 1 - 10 - 1;  // menu, cmd, status
    int cmd_height = 10;
    
    // Function list (left panel)
    func_win_ = newwin(content_height, func_width, 1, 0);
    keypad(func_win_, TRUE);
    
    // Content area (right panel)
    content_win_ = newwin(content_height, width_ - func_width, 1, func_width);
    keypad(content_win_, TRUE);
    
    // Command center
    cmd_win_ = newwin(cmd_height, width_, 1 + content_height, 0);
    keypad(cmd_win_, TRUE);
    
    // Status bar
    status_win_ = newwin(1, width_, height_ - 1, 0);
}

void App::destroyWindows() {
    if (menu_win_) delwin(menu_win_);
    if (func_win_) delwin(func_win_);
    if (content_win_) delwin(content_win_);
    if (cmd_win_) delwin(cmd_win_);
    if (status_win_) delwin(status_win_);
}

void App::handleResize() {
    getmaxyx(stdscr, height_, width_);
    destroyWindows();
    createWindows();
    refreshAll();
}

void App::run() {
    refreshAll();
    
    while (running_) {
        int ch = getch();
        
        if (ch == KEY_RESIZE) {
            handleResize();
            continue;
        }
        
        handleInput(ch);
        refreshAll();
    }
}

void App::handleInput(int ch) {
    // Handle command center input
    if (command_focused_) {
        if (ch == 27) {  // ESC
            command_focused_ = false;
            curs_set(0);
        } else if (ch == '\n' || ch == KEY_ENTER) {
            if (!command_input_.empty()) {
                executeCommand(command_input_);
                command_history_.push_back(command_input_);
                command_input_.clear();
                history_index_ = -1;
            }
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (!command_input_.empty()) {
                command_input_.pop_back();
            }
        } else if (ch == KEY_UP && !command_history_.empty()) {
            if (history_index_ < 0) {
                history_index_ = command_history_.size() - 1;
            } else if (history_index_ > 0) {
                history_index_--;
            }
            command_input_ = command_history_[history_index_];
        } else if (ch == KEY_DOWN && !command_history_.empty()) {
            if (history_index_ >= 0) {
                history_index_++;
                if (history_index_ >= static_cast<int>(command_history_.size())) {
                    history_index_ = -1;
                    command_input_.clear();
                } else {
                    command_input_ = command_history_[history_index_];
                }
            }
        } else if (ch >= 32 && ch < 127) {
            command_input_ += static_cast<char>(ch);
        } else if (ch == '\t') {
            command_focused_ = false;
            curs_set(0);
            focus_ = 1;
        }
        return;
    }
    
    // Global keys
    if (ch == ':') {
        command_focused_ = true;
        command_input_.clear();
        curs_set(1);
        return;
    }
    
    if (ch == 'q' || ch == 'Q') {
        running_ = false;
        return;
    }
    
    // F-keys for menus
    if (ch == KEY_F(1) || ch == KEY_F(2) || ch == KEY_F(3) || ch == KEY_F(4)) {
        toolbar_->handleKey(ch);
        return;
    }
    
    // Tab to switch focus
    if (ch == '\t') {
        focus_ = (focus_ + 1) % 4;
        if (focus_ == 3) {
            command_focused_ = true;
            curs_set(1);
        }
        return;
    }
    
    // Mouse
    if (ch == KEY_MOUSE) {
        MEVENT event;
        if (getmouse(&event) == OK) {
            // Determine which window was clicked
            if (event.y == 0) {
                // Menu bar
                focus_ = 0;
            } else if (event.y < height_ - 11) {
                if (event.x < 35) {
                    focus_ = 1;  // Functions
                } else {
                    focus_ = 2;  // Content
                }
            } else if (event.y < height_ - 1) {
                focus_ = 3;  // Command
                command_focused_ = true;
                curs_set(1);
            }
            
            // Forward to appropriate component
            if (focus_ == 1) {
                function_view_->handleMouse(event);
            }
        }
        return;
    }
    
    // Forward to focused component
    switch (focus_) {
        case 0:
            toolbar_->handleKey(ch);
            break;
        case 1:
            function_view_->handleKey(ch);
            break;
        case 2:
            if (window_state_.show_pseudo) {
                pseudo_view_->handleKey(ch);
            } else if (window_state_.show_disasm) {
                disasm_view_->handleKey(ch);
            }
            break;
    }
}

void App::refreshAll() {
    // Clear and redraw all windows
    werase(menu_win_);
    werase(func_win_);
    werase(content_win_);
    werase(cmd_win_);
    werase(status_win_);
    
    // Draw menu bar
    toolbar_->draw(menu_win_);
    
    // Draw function list
    box(func_win_, 0, 0);
    mvwprintw(func_win_, 0, 2, " Functions ");
    function_view_->draw(func_win_);
    
    // Draw content
    box(content_win_, 0, 0);
    if (window_state_.show_pseudo) {
        mvwprintw(content_win_, 0, 2, " Pseudocode ");
        pseudo_view_->draw(content_win_);
    } else if (window_state_.show_disasm) {
        mvwprintw(content_win_, 0, 2, " Disassembly ");
        disasm_view_->draw(content_win_);
    }
    
    // Draw command center
    drawCommandCenter();
    
    // Draw status bar
    drawStatusBar();
    
    // Refresh all windows
    wrefresh(menu_win_);
    wrefresh(func_win_);
    wrefresh(content_win_);
    wrefresh(cmd_win_);
    wrefresh(status_win_);
    
    // Position cursor if in command mode
    if (command_focused_) {
        int cmd_y = height_ - 10;
        wmove(cmd_win_, 8, 3 + command_input_.length());
        wrefresh(cmd_win_);
    }
}

void App::drawCommandCenter() {
    box(cmd_win_, 0, 0);
    mvwprintw(cmd_win_, 0, 2, " Command Center ");
    
    int win_height, win_width;
    getmaxyx(cmd_win_, win_height, win_width);
    
    // Draw output (last few lines)
    int visible_lines = win_height - 3;
    int start = std::max(0, static_cast<int>(command_output_.size()) - visible_lines);
    
    for (int i = 0; i < visible_lines && start + i < static_cast<int>(command_output_.size()); i++) {
        std::string line = command_output_[start + i];
        if (line.length() > static_cast<size_t>(win_width - 4)) {
            line = line.substr(0, win_width - 7) + "...";
        }
        mvwprintw(cmd_win_, 1 + i, 2, "%s", line.c_str());
    }
    
    // Draw input line
    wattron(cmd_win_, A_BOLD);
    mvwprintw(cmd_win_, win_height - 2, 1, "> ");
    wattroff(cmd_win_, A_BOLD);
    
    if (command_focused_) {
        wattron(cmd_win_, A_REVERSE);
    }
    mvwprintw(cmd_win_, win_height - 2, 3, "%-*s", win_width - 5, command_input_.c_str());
    if (command_focused_) {
        wattroff(cmd_win_, A_REVERSE);
    }
}

void App::drawStatusBar() {
    wattron(status_win_, A_REVERSE);
    mvwhline(status_win_, 0, 0, ' ', width_);
    mvwprintw(status_win_, 0, 1, " %s ", status_.c_str());
    mvwprintw(status_win_, 0, width_ - 40, " F1-F4:Menus | Tab:Focus | q:Quit ");
    wattroff(status_win_, A_REVERSE);
}

bool App::loadNsoFile(const std::string& path) {
    appendOutput("> load " + path);
    setStatus("Loading " + path + "...");
    refreshAll();
    
    if (!analyzer_.loadNso(path)) {
        appendOutput("ERROR: Failed to load NSO file");
        setStatus("Failed to load NSO file");
        return false;
    }
    
    file_loaded_ = true;
    appendOutput("NSO loaded successfully");
    appendOutput("Build ID: " + analyzer_.getNso().getBuildId());
    appendOutput("Analyzing...");
    setStatus("Analyzing...");
    refreshAll();
    
    analyzer_.analyze();
    analyzed_ = true;
    
    auto& funcs = analyzer_.getFunctionFinder().getFunctions();
    auto& strs = analyzer_.getStringTable().getStrings();
    
    appendOutput("Found " + std::to_string(funcs.size()) + " functions");
    appendOutput("Found " + std::to_string(strs.size()) + " strings");
    appendOutput("Ready.");
    
    function_view_->refresh();
    setStatus("Loaded: " + std::to_string(funcs.size()) + " functions");
    
    return true;
}

bool App::loadProgress(const std::string& build_id) {
    appendOutput("> load progress " + build_id);
    setStatus("Loading progress...");
    
    if (!progress_mgr_.loadProgress(analyzer_, build_id)) {
        appendOutput("ERROR: " + progress_mgr_.getError());
        setStatus("Failed to load progress");
        return false;
    }
    
    file_loaded_ = true;
    analyzed_ = true;
    function_view_->refresh();
    appendOutput("Progress loaded");
    setStatus("Loaded progress for " + build_id);
    
    return true;
}

bool App::saveProgress() {
    if (!file_loaded_ || !analyzed_) {
        appendOutput("Nothing to save");
        return false;
    }
    
    appendOutput("> save");
    if (!progress_mgr_.saveProgress(analyzer_)) {
        appendOutput("ERROR: " + progress_mgr_.getError());
        return false;
    }
    
    appendOutput("Progress saved");
    setStatus("Progress saved");
    return true;
}

void App::setSelectedFunction(uint64_t addr) {
    selected_function_ = addr;
    pseudo_view_->setFunction(addr);
    disasm_view_->setFunction(addr);
}

void App::setStatus(const std::string& msg) {
    status_ = msg;
}

void App::appendOutput(const std::string& text) {
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        command_output_.push_back(line);
    }
    while (command_output_.size() > 500) {
        command_output_.erase(command_output_.begin());
    }
}

void App::executeCommand(const std::string& cmd) {
    if (cmd.empty()) return;
    
    appendOutput("> " + cmd);
    
    std::istringstream iss(cmd);
    std::string command;
    iss >> command;
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);
    
    if (command == "help" || command == "h" || command == "?") {
        appendOutput("Commands:");
        appendOutput("  load <path>        Load NSO file");
        appendOutput("  save               Save progress");
        appendOutput("  goto <addr>        Go to address");
        appendOutput("  info               Show file info");
        appendOutput("  clear              Clear output");
        appendOutput("  quit               Exit");
        return;
    }
    
    if (command == "clear" || command == "cls") {
        command_output_.clear();
        return;
    }
    
    if (command == "quit" || command == "exit" || command == "q") {
        running_ = false;
        return;
    }
    
    if (command == "load") {
        std::string path;
        std::getline(iss >> std::ws, path);
        if (path.empty()) {
            appendOutput("Usage: load <path>");
            return;
        }
        loadNsoFile(path);
        return;
    }
    
    if (command == "save") {
        saveProgress();
        return;
    }
    
    if (command == "info") {
        if (!file_loaded_) {
            appendOutput("No file loaded");
            return;
        }
        appendOutput("Build ID: " + analyzer_.getNso().getBuildId());
        appendOutput("Functions: " + std::to_string(analyzer_.getFunctionFinder().getFunctions().size()));
        appendOutput("Strings: " + std::to_string(analyzer_.getStringTable().getStrings().size()));
        return;
    }
    
    if (command == "goto" || command == "go" || command == "g") {
        std::string addr_str;
        iss >> addr_str;
        if (addr_str.empty()) {
            appendOutput("Usage: goto <address>");
            return;
        }
        
        uint64_t addr = 0;
        if (addr_str.substr(0, 2) == "0x") {
            std::stringstream ss;
            ss << std::hex << addr_str.substr(2);
            ss >> addr;
        } else {
            addr = std::stoull(addr_str);
        }
        
        setSelectedFunction(addr);
        std::stringstream ss;
        ss << "Jumped to 0x" << std::hex << addr;
        appendOutput(ss.str());
        return;
    }
    
    appendOutput("Unknown command. Type 'help' for commands.");
}

} // namespace gui
} // namespace kiloader

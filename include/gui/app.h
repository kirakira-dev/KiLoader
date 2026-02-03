#pragma once

#include <ncurses.h>
#include <menu.h>
#include <panel.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "analyzer.h"
#include "progress_manager.h"

namespace kiloader {
namespace gui {

// Forward declarations
class Toolbar;
class FunctionView;
class PseudoView;
class DisasmView;
class SearchDialog;

// UI Settings
struct UISettings {
    bool dark_theme = true;
    bool show_line_numbers = true;
};

// Window visibility
struct WindowState {
    bool show_functions = true;
    bool show_pseudo = true;
    bool show_disasm = false;
};

// Main application class
class App {
public:
    App();
    ~App();
    
    // Run the application (blocking)
    void run();
    
    // Load file
    bool loadNsoFile(const std::string& path);
    bool loadProgress(const std::string& build_id);
    
    // Save progress
    bool saveProgress();
    
    // Get analyzer
    Analyzer& getAnalyzer() { return analyzer_; }
    
    // Get settings
    UISettings& getSettings() { return settings_; }
    WindowState& getWindowState() { return window_state_; }
    
    // Set selected function
    void setSelectedFunction(uint64_t addr);
    uint64_t getSelectedFunction() const { return selected_function_; }
    
    // Status message
    void setStatus(const std::string& msg);
    std::string getStatus() const { return status_; }
    
    // Command center
    void executeCommand(const std::string& cmd);
    void appendOutput(const std::string& text);
    bool isCommandCenterFocused() const { return command_focused_; }
    void focusCommandCenter() { command_focused_ = true; }
    
    // Quit
    void quit() { running_ = false; }
    
    // Get windows
    WINDOW* getMainWin() { return main_win_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    
    // Refresh all
    void refreshAll();
    
private:
    void initNcurses();
    void cleanupNcurses();
    void createWindows();
    void destroyWindows();
    void handleResize();
    void drawLayout();
    void drawStatusBar();
    void drawCommandCenter();
    void handleInput(int ch);
    
    Analyzer analyzer_;
    ProgressManager progress_mgr_;
    
    UISettings settings_;
    WindowState window_state_;
    
    uint64_t selected_function_ = 0;
    std::string status_ = "Ready - Press F1 for Load menu";
    bool running_ = true;
    bool file_loaded_ = false;
    bool analyzed_ = false;
    
    // Command center
    std::string command_input_;
    std::vector<std::string> command_output_;
    std::vector<std::string> command_history_;
    int history_index_ = -1;
    bool command_focused_ = false;
    int output_scroll_ = 0;
    
    // Windows
    WINDOW* main_win_ = nullptr;
    WINDOW* menu_win_ = nullptr;
    WINDOW* func_win_ = nullptr;
    WINDOW* content_win_ = nullptr;
    WINDOW* cmd_win_ = nullptr;
    WINDOW* status_win_ = nullptr;
    
    int width_ = 0;
    int height_ = 0;
    
    // Components
    std::unique_ptr<Toolbar> toolbar_;
    std::unique_ptr<FunctionView> function_view_;
    std::unique_ptr<PseudoView> pseudo_view_;
    std::unique_ptr<DisasmView> disasm_view_;
    std::unique_ptr<SearchDialog> search_dialog_;
    
    // Focus state: 0=menu, 1=functions, 2=content, 3=command
    int focus_ = 1;
};

} // namespace gui
} // namespace kiloader

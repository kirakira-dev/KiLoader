#pragma once

#include <memory>
#include <string>
#include <functional>
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
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
    int font_size = 1;  // 0=small, 1=normal, 2=large
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
    void quit();
    
private:
    ftxui::Component createMainComponent();
    ftxui::Component createLayout();
    ftxui::Element renderCommandCenter();
    
    ftxui::ScreenInteractive screen_;
    Analyzer analyzer_;
    ProgressManager progress_mgr_;
    
    UISettings settings_;
    WindowState window_state_;
    
    uint64_t selected_function_ = 0;
    std::string status_ = "Ready";
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
    
    // Components
    std::shared_ptr<Toolbar> toolbar_;
    std::shared_ptr<FunctionView> function_view_;
    std::shared_ptr<PseudoView> pseudo_view_;
    std::shared_ptr<DisasmView> disasm_view_;
    std::shared_ptr<SearchDialog> search_dialog_;
};

} // namespace gui
} // namespace kiloader

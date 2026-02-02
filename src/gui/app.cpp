#include "gui/app.h"
#include "gui/toolbar.h"
#include "gui/function_view.h"
#include "gui/pseudo_view.h"
#include "gui/disasm_view.h"
#include "gui/search_dialog.h"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace kiloader {
namespace gui {

using namespace ftxui;

App::App() : screen_(ScreenInteractive::Fullscreen()) {
    toolbar_ = std::make_shared<Toolbar>(*this);
    function_view_ = std::make_shared<FunctionView>(*this);
    pseudo_view_ = std::make_shared<PseudoView>(*this);
    disasm_view_ = std::make_shared<DisasmView>(*this);
    search_dialog_ = std::make_shared<SearchDialog>(*this);
    
    // Welcome message
    command_output_.push_back("KILOADER Command Center - Type 'help' for commands");
    command_output_.push_back("");
}

App::~App() = default;

void App::run() {
    auto component = createMainComponent();
    screen_.Loop(component);
}

void App::appendOutput(const std::string& text) {
    // Split by newlines
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        command_output_.push_back(line);
    }
    // Keep last 500 lines
    while (command_output_.size() > 500) {
        command_output_.erase(command_output_.begin());
    }
    // Scroll to bottom
    output_scroll_ = std::max(0, static_cast<int>(command_output_.size()) - 8);
}

void App::executeCommand(const std::string& cmd) {
    if (cmd.empty()) return;
    
    // Add to history
    command_history_.push_back(cmd);
    history_index_ = -1;
    
    // Echo command
    appendOutput("> " + cmd);
    
    // Parse command
    std::istringstream iss(cmd);
    std::string command;
    iss >> command;
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);
    
    // Help
    if (command == "help" || command == "h" || command == "?") {
        appendOutput("Commands:");
        appendOutput("  load <path>        Load NSO file");
        appendOutput("  analyze            Run full analysis");
        appendOutput("  save               Save analysis progress");
        appendOutput("  disasm <addr> [n]  Disassemble at address");
        appendOutput("  func <addr>        Show function info");
        appendOutput("  goto <addr>        Go to address/function");
        appendOutput("  strings <pattern>  Search strings");
        appendOutput("  info               Show loaded file info");
        appendOutput("  clear              Clear output");
        appendOutput("  quit               Exit application");
        return;
    }
    
    // Clear
    if (command == "clear" || command == "cls") {
        command_output_.clear();
        output_scroll_ = 0;
        return;
    }
    
    // Quit
    if (command == "quit" || command == "exit" || command == "q") {
        quit();
        return;
    }
    
    // Load
    if (command == "load") {
        std::string path;
        std::getline(iss >> std::ws, path);
        if (path.empty()) {
            appendOutput("Usage: load <path>");
            return;
        }
        if (loadNsoFile(path)) {
            appendOutput("Loaded successfully");
        }
        return;
    }
    
    // Analyze
    if (command == "analyze") {
        if (!file_loaded_) {
            appendOutput("No file loaded");
            return;
        }
        appendOutput("Analyzing...");
        analyzer_.analyze();
        analyzed_ = true;
        function_view_->refresh();
        appendOutput("Analysis complete: " + 
            std::to_string(analyzer_.getFunctionFinder().getFunctions().size()) + " functions");
        return;
    }
    
    // Save
    if (command == "save") {
        if (saveProgress()) {
            appendOutput("Progress saved");
        }
        return;
    }
    
    // Info
    if (command == "info") {
        if (!file_loaded_) {
            appendOutput("No file loaded");
            return;
        }
        appendOutput("Build ID: " + analyzer_.getNso().getBuildId());
        appendOutput("Text size: 0x" + 
            ([](size_t s) { std::stringstream ss; ss << std::hex << s; return ss.str(); })
            (analyzer_.getNso().getTextSegment().size));
        appendOutput("Functions: " + std::to_string(analyzer_.getFunctionFinder().getFunctions().size()));
        appendOutput("Strings: " + std::to_string(analyzer_.getStringTable().getStrings().size()));
        return;
    }
    
    // Goto
    if (command == "goto" || command == "go" || command == "g") {
        std::string addr_str;
        iss >> addr_str;
        if (addr_str.empty()) {
            appendOutput("Usage: goto <address>");
            return;
        }
        
        uint64_t addr = 0;
        // Parse FUN_ prefix
        std::string upper = addr_str;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        if (upper.substr(0, 4) == "FUN_" || upper.substr(0, 4) == "SUB_") {
            std::stringstream ss;
            ss << std::hex << addr_str.substr(4);
            ss >> addr;
        } else if (addr_str.substr(0, 2) == "0x" || addr_str.substr(0, 2) == "0X") {
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
    
    // Disasm
    if (command == "disasm" || command == "d") {
        std::string addr_str;
        size_t count = 10;
        iss >> addr_str >> count;
        
        if (addr_str.empty()) {
            appendOutput("Usage: disasm <address> [count]");
            return;
        }
        
        uint64_t addr = 0;
        if (addr_str.substr(0, 2) == "0x" || addr_str.substr(0, 2) == "0X") {
            std::stringstream ss;
            ss << std::hex << addr_str.substr(2);
            ss >> addr;
        } else {
            addr = std::stoull(addr_str);
        }
        
        auto insns = analyzer_.disassembleAt(addr, count);
        for (const auto& insn : insns) {
            std::stringstream ss;
            ss << std::hex << std::setfill('0') << std::setw(10) << insn.address;
            ss << "  " << insn.mnemonic << " " << insn.operands;
            appendOutput(ss.str());
        }
        return;
    }
    
    // Func
    if (command == "func" || command == "f") {
        std::string addr_str;
        iss >> addr_str;
        
        if (addr_str.empty()) {
            appendOutput("Usage: func <address>");
            return;
        }
        
        uint64_t addr = 0;
        if (addr_str.substr(0, 2) == "0x" || addr_str.substr(0, 2) == "0X") {
            std::stringstream ss;
            ss << std::hex << addr_str.substr(2);
            ss >> addr;
        } else {
            addr = std::stoull(addr_str);
        }
        
        auto* func = analyzer_.getFunctionAt(addr);
        if (func) {
            appendOutput("Function: " + func->name);
            std::stringstream ss;
            ss << "Address: 0x" << std::hex << func->address;
            appendOutput(ss.str());
            appendOutput("Size: " + std::to_string(func->size) + " bytes");
            appendOutput("Leaf: " + std::string(func->is_leaf ? "yes" : "no"));
        } else {
            appendOutput("No function at address");
        }
        return;
    }
    
    // Strings
    if (command == "strings" || command == "s") {
        std::string pattern;
        std::getline(iss >> std::ws, pattern);
        
        if (pattern.empty()) {
            appendOutput("Usage: strings <pattern>");
            return;
        }
        
        auto results = analyzer_.searchStrings(pattern);
        appendOutput("Found " + std::to_string(results.size()) + " strings:");
        int shown = 0;
        for (const auto& entry : results) {
            std::stringstream ss;
            ss << "  0x" << std::hex << entry.address << ": ";
            std::string val = entry.value;
            if (val.length() > 50) val = val.substr(0, 47) + "...";
            for (char& c : val) {
                if (c == '\n' || c == '\r' || c == '\t') c = ' ';
            }
            ss << val;
            appendOutput(ss.str());
            if (++shown >= 20) {
                appendOutput("  ... and " + std::to_string(results.size() - 20) + " more");
                break;
            }
        }
        return;
    }
    
    appendOutput("Unknown command: " + command + ". Type 'help' for commands.");
}

bool App::loadNsoFile(const std::string& path) {
    setStatus("Loading " + path + "...");
    
    if (!analyzer_.loadNso(path)) {
        setStatus("Failed to load NSO file");
        return false;
    }
    
    file_loaded_ = true;
    setStatus("Analyzing...");
    
    analyzer_.analyze();
    analyzed_ = true;
    
    function_view_->refresh();
    
    auto& funcs = analyzer_.getFunctionFinder().getFunctions();
    setStatus("Loaded: " + std::to_string(funcs.size()) + " functions");
    
    return true;
}

bool App::loadProgress(const std::string& build_id) {
    setStatus("Loading progress for " + build_id + "...");
    
    if (!progress_mgr_.loadProgress(analyzer_, build_id)) {
        setStatus("Failed to load progress: " + progress_mgr_.getError());
        return false;
    }
    
    file_loaded_ = true;
    analyzed_ = true;
    function_view_->refresh();
    setStatus("Loaded progress for " + build_id);
    
    return true;
}

bool App::saveProgress() {
    if (!file_loaded_ || !analyzed_) {
        setStatus("Nothing to save");
        return false;
    }
    
    setStatus("Saving progress...");
    
    if (!progress_mgr_.saveProgress(analyzer_)) {
        setStatus("Failed to save: " + progress_mgr_.getError());
        return false;
    }
    
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

void App::quit() {
    running_ = false;
    screen_.Exit();
}

Element App::renderCommandCenter() {
    Elements output_lines;
    
    // Show output (last N lines based on scroll)
    int visible_lines = 6;
    int start = std::max(0, std::min(output_scroll_, 
                 static_cast<int>(command_output_.size()) - visible_lines));
    int end = std::min(start + visible_lines, static_cast<int>(command_output_.size()));
    
    for (int i = start; i < end; i++) {
        Element line = text(command_output_[i]);
        if (command_output_[i].substr(0, 2) == "> ") {
            line = line | bold;
        }
        output_lines.push_back(line);
    }
    
    // Pad if needed
    while (output_lines.size() < static_cast<size_t>(visible_lines)) {
        output_lines.push_back(text(""));
    }
    
    // Input line
    Element input_line = hbox({
        text("> ") | bold | color(Color::Green),
        text(command_input_) | flex,
        text("_") | (command_focused_ ? blink : dim),
    });
    
    if (command_focused_) {
        input_line = input_line | inverted;
    }
    
    return vbox({
        text(" Command Center ") | bold | center,
        separator(),
        vbox(output_lines) | frame,
        separator(),
        input_line,
    }) | border | size(HEIGHT, EQUAL, 12);
}

Component App::createMainComponent() {
    auto toolbar_component = toolbar_->getComponent();
    auto func_component = function_view_->getComponent();
    auto pseudo_component = pseudo_view_->getComponent();
    auto disasm_component = disasm_view_->getComponent();
    auto search_component = search_dialog_->getComponent();
    
    // Container for all focusable components
    auto container = Container::Vertical({
        toolbar_component,
        Container::Horizontal({
            func_component,
            Container::Tab({
                pseudo_component,
                disasm_component,
            }, nullptr),
        }),
    });
    
    // Main renderer
    auto renderer = Renderer(container, [&] {
        Elements main_content;
        
        // Toolbar
        main_content.push_back(toolbar_->render());
        main_content.push_back(separator());
        
        // Main area
        Elements panels;
        
        // Functions panel
        if (window_state_.show_functions) {
            panels.push_back(
                vbox({
                    text(" Functions ") | bold | center,
                    separator(),
                    function_view_->render() | flex,
                }) | border | size(WIDTH, EQUAL, 35)
            );
        }
        
        // Right panel (pseudo or disasm)
        Elements right_content;
        
        if (window_state_.show_pseudo) {
            right_content.push_back(
                vbox({
                    text(" Pseudocode ") | bold | center,
                    separator(),
                    pseudo_view_->render() | flex,
                }) | border | flex
            );
        }
        
        if (window_state_.show_disasm) {
            right_content.push_back(
                vbox({
                    text(" Disassembly ") | bold | center,
                    separator(),
                    disasm_view_->render() | flex,
                }) | border | flex
            );
        }
        
        if (!right_content.empty()) {
            if (right_content.size() == 1) {
                panels.push_back(right_content[0]);
            } else {
                panels.push_back(vbox(right_content) | flex);
            }
        }
        
        if (!panels.empty()) {
            main_content.push_back(hbox(panels) | flex);
        }
        
        // Command Center at the bottom
        main_content.push_back(renderCommandCenter());
        
        // Status bar
        main_content.push_back(
            hbox({
                text(" " + status_ + " ") | flex,
                separator(),
                text(" F1-F4:Menus | : Command | Ctrl+Q:Quit "),
            })
        );
        
        Element main = vbox(main_content);
        
        // Overlay search dialog if visible
        if (search_dialog_->isVisible()) {
            main = dbox({
                main,
                search_dialog_->render() | clear_under | center,
            });
        }
        
        return main;
    });
    
    // Handle global keyboard events
    return CatchEvent(renderer, [&](Event event) {
        // IMPORTANT: If toolbar has an active dialog, let it handle ALL events first
        if (toolbar_->hasActiveDialog()) {
            // Forward to toolbar's component - return false to let it propagate
            return false;
        }
        
        // Handle command center input when focused
        if (command_focused_) {
            // Text input
            if (event.is_character()) {
                command_input_ += event.character();
                return true;
            }
            
            // Backspace
            if (event == Event::Backspace && !command_input_.empty()) {
                command_input_.pop_back();
                return true;
            }
            
            // Enter - execute command
            if (event == Event::Return) {
                executeCommand(command_input_);
                command_input_.clear();
                return true;
            }
            
            // History navigation
            if (event == Event::ArrowUp && !command_history_.empty()) {
                if (history_index_ < 0) {
                    history_index_ = command_history_.size() - 1;
                } else if (history_index_ > 0) {
                    history_index_--;
                }
                command_input_ = command_history_[history_index_];
                return true;
            }
            
            if (event == Event::ArrowDown && !command_history_.empty()) {
                if (history_index_ >= 0) {
                    history_index_++;
                    if (history_index_ >= static_cast<int>(command_history_.size())) {
                        history_index_ = -1;
                        command_input_.clear();
                    } else {
                        command_input_ = command_history_[history_index_];
                    }
                }
                return true;
            }
            
            // Scroll output
            if (event == Event::PageUp) {
                output_scroll_ = std::max(0, output_scroll_ - 3);
                return true;
            }
            if (event == Event::PageDown) {
                output_scroll_ = std::min(
                    static_cast<int>(command_output_.size()) - 6,
                    output_scroll_ + 3);
                return true;
            }
            
            // Escape to unfocus
            if (event == Event::Escape) {
                command_focused_ = false;
                return true;
            }
            
            // Tab to unfocus (switch to panels)
            if (event == Event::Tab) {
                command_focused_ = false;
                return true;
            }
            
            return true;  // Consume all other events when focused
        }
        
        // ':' key focuses command center (vim-like)
        if (event == Event::Character(':')) {
            command_focused_ = true;
            command_input_.clear();
            return true;
        }
        
        // '/' also focuses for search-like input
        if (event == Event::Character('/')) {
            command_focused_ = true;
            command_input_ = "strings ";
            return true;
        }
        
        // Ctrl+Q to quit
        if (event.input() == "\x11") {
            quit();
            return true;
        }
        
        // Search shortcuts (when not in command center)
        if (event.input() == "\x13") {  // Ctrl+S - Search Strings
            search_dialog_->show(SearchType::Strings);
            return true;
        }
        if (event.input() == "\x01") {  // Ctrl+A - Search Assembly
            search_dialog_->show(SearchType::Assembly);
            return true;
        }
        if (event.input() == "\x08") {  // Ctrl+H - Search Hex
            search_dialog_->show(SearchType::RawHex);
            return true;
        }
        if (event.input() == "\x10") {  // Ctrl+P - Search Pseudocode
            search_dialog_->show(SearchType::Pseudocode);
            return true;
        }
        
        // Escape closes dialogs
        if (event == Event::Escape) {
            if (search_dialog_->isVisible()) {
                search_dialog_->hide();
                return true;
            }
        }
        
        // Let F1-F4 through to toolbar for menu access
        if (event == Event::F1 || event == Event::F2 || 
            event == Event::F3 || event == Event::F4) {
            return false;  // Don't consume, let toolbar handle
        }
        
        return false;
    });
}

} // namespace gui
} // namespace kiloader

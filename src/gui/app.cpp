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

namespace kiloader {
namespace gui {

using namespace ftxui;

App::App() : screen_(ScreenInteractive::Fullscreen()) {
    toolbar_ = std::make_shared<Toolbar>(*this);
    function_view_ = std::make_shared<FunctionView>(*this);
    pseudo_view_ = std::make_shared<PseudoView>(*this);
    disasm_view_ = std::make_shared<DisasmView>(*this);
    search_dialog_ = std::make_shared<SearchDialog>(*this);
}

App::~App() = default;

void App::run() {
    auto component = createMainComponent();
    screen_.Loop(component);
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
        
        // Status bar
        main_content.push_back(separator());
        main_content.push_back(
            hbox({
                text(" " + status_ + " ") | flex,
                separator(),
                text(" Ctrl+Q: Quit | Tab: Switch Focus "),
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
        // Quit
        if (event == Event::Character('q') && event.is_character()) {
            // Check for Ctrl
            return false;  // Let it propagate
        }
        
        if (event.input() == "\x11") {  // Ctrl+Q
            quit();
            return true;
        }
        
        // Search shortcuts
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
        
        return false;
    });
}

} // namespace gui
} // namespace kiloader

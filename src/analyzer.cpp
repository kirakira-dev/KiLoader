#include "analyzer.h"
#include <fstream>
#include <iostream>
#include <iomanip>

namespace kiloader {

Analyzer::Analyzer() {
    nso_ = std::make_unique<NsoFile>();
    disasm_ = std::make_unique<Disassembler>();
}

Analyzer::~Analyzer() = default;

bool Analyzer::loadNso(const std::string& path) {
    if (!nso_->load(path)) {
        std::cerr << "Failed to load NSO: " << path << std::endl;
        return false;
    }
    
    if (!disasm_->initialize()) {
        std::cerr << "Failed to initialize disassembler: " << disasm_->getError() << std::endl;
        return false;
    }
    
    // Create other components
    func_finder_ = std::make_unique<FunctionFinder>(*nso_, *disasm_);
    string_table_ = std::make_unique<StringTable>(*nso_);
    
    loaded_ = true;
    analyzed_ = false;
    
    std::cout << "Loaded NSO: " << path << std::endl;
    std::cout << "  Build ID: " << nso_->getBuildId() << std::endl;
    std::cout << "  Text size: 0x" << std::hex << nso_->getTextSegment().size << std::endl;
    std::cout << "  Rodata size: 0x" << nso_->getRodataSegment().size << std::endl;
    std::cout << "  Data size: 0x" << nso_->getDataSegment().size << std::dec << std::endl;
    
    return true;
}

void Analyzer::analyze() {
    if (!loaded_) {
        std::cerr << "No NSO loaded" << std::endl;
        return;
    }
    
    std::cout << "\nFinding strings..." << std::endl;
    string_table_->findStrings();
    std::cout << "  Found " << string_table_->getStrings().size() << " strings" << std::endl;
    
    std::cout << "\nFinding functions..." << std::endl;
    func_finder_->findFunctions();
    std::cout << "  Found " << func_finder_->getFunctions().size() << " functions" << std::endl;
    
    std::cout << "\nAnalyzing cross-references..." << std::endl;
    xref_analyzer_ = std::make_unique<XRefAnalyzer>(*nso_, *disasm_, *func_finder_);
    xref_analyzer_->analyze();
    std::cout << "  Found " << xref_analyzer_->getAllXRefs().size() << " xrefs" << std::endl;
    
    // Create pseudocode generator
    pseudocode_ = std::make_unique<PseudocodeGenerator>(*nso_, *func_finder_, *xref_analyzer_);
    
    analyzed_ = true;
    std::cout << "\nAnalysis complete!" << std::endl;
}

std::vector<Instruction> Analyzer::disassembleAt(uint64_t address, size_t count) {
    if (!loaded_) return {};
    
    uint8_t buf[1024];
    size_t size = std::min(count * 4, sizeof(buf));
    
    if (!nso_->readMemory(address, buf, size)) {
        return {};
    }
    
    return disasm_->disassemble(buf, size, address, count);
}

Function* Analyzer::getFunctionAt(uint64_t address) {
    if (!analyzed_) return nullptr;
    return func_finder_->getFunction(address);
}

std::string Analyzer::getPseudocodeAt(uint64_t address) {
    if (!analyzed_) return "";
    return pseudocode_->generate(address);
}

std::vector<XRef> Analyzer::getRefsTo(uint64_t address) {
    if (!analyzed_) return {};
    return xref_analyzer_->getRefsTo(address);
}

std::vector<XRef> Analyzer::getRefsFrom(uint64_t address) {
    if (!analyzed_) return {};
    return xref_analyzer_->getRefsFrom(address);
}

std::vector<StringEntry> Analyzer::searchStrings(const std::string& pattern) {
    if (!loaded_) return {};
    return string_table_->search(pattern);
}

uint64_t Analyzer::findString(const std::string& str) {
    auto results = string_table_->search(str);
    for (const auto& entry : results) {
        if (entry.value == str) {
            return entry.address;
        }
    }
    return 0;
}

void Analyzer::exportToFile(const std::string& path) {
    std::ofstream f(path);
    if (!f) {
        std::cerr << "Failed to open: " << path << std::endl;
        return;
    }
    
    f << "KILOADER ANALYSIS DUMP\n";
    f << "======================\n\n";
    f << "Build ID: " << nso_->getBuildId() << "\n\n";
    
    // Strings
    f << "STRINGS\n";
    f << "-------\n";
    for (const auto& s : string_table_->getStrings()) {
        f << "0x" << std::hex << s.address << ": " << s.value << "\n";
    }
    f << "\n";
    
    // Functions
    f << "FUNCTIONS\n";
    f << "---------\n";
    for (const auto& [addr, func] : func_finder_->getFunctions()) {
        f << "0x" << std::hex << addr << ": " << func.name << " (size: " << std::dec << func.size << ")\n";
    }
    f << "\n";
    
    // XRefs
    f << "CROSS-REFERENCES\n";
    f << "----------------\n";
    for (const auto& xref : xref_analyzer_->getAllXRefs()) {
        f << "0x" << std::hex << xref.from_address << " -> 0x" << xref.to_address;
        f << " (" << xref.description << ")\n";
    }
    
    std::cout << "Exported to: " << path << std::endl;
}

void Analyzer::exportFunctions(const std::string& path) {
    std::ofstream f(path);
    if (!f) return;
    
    for (const auto& [addr, func] : func_finder_->getFunctions()) {
        f << "0x" << std::hex << addr << "|" << func.name << "|" << std::dec << func.size << "\n";
    }
}

void Analyzer::exportStrings(const std::string& path) {
    std::ofstream f(path);
    if (!f) return;
    
    for (const auto& s : string_table_->getStrings()) {
        f << "0x" << std::hex << s.address << "|" << s.value << "\n";
    }
}

void Analyzer::printDisassembly(uint64_t address, size_t count) {
    auto insns = disassembleAt(address, count);
    for (const auto& insn : insns) {
        std::cout << insn.toString() << std::endl;
    }
}

void Analyzer::printFunction(uint64_t address) {
    auto* func = getFunctionAt(address);
    if (!func) {
        std::cout << "No function at 0x" << std::hex << address << std::endl;
        return;
    }
    
    std::cout << "Function: " << func->name << std::endl;
    std::cout << "Address: 0x" << std::hex << func->address << std::endl;
    std::cout << "Size: " << std::dec << func->size << " bytes" << std::endl;
    std::cout << "Leaf: " << (func->is_leaf ? "yes" : "no") << std::endl;
    std::cout << "\nDisassembly:\n";
    
    for (const auto& insn : func->instructions) {
        std::cout << "  " << insn.toString() << std::endl;
    }
}

void Analyzer::printXRefs(uint64_t address) {
    std::cout << "References TO 0x" << std::hex << address << ":\n";
    auto refs = getRefsTo(address);
    for (const auto& xref : refs) {
        std::cout << "  0x" << std::hex << xref.from_address;
        std::cout << " in " << xref.from_function_name;
        std::cout << " (" << xref.description << ")\n";
    }
    
    std::cout << "\nReferences FROM 0x" << std::hex << address << ":\n";
    refs = getRefsFrom(address);
    for (const auto& xref : refs) {
        std::cout << "  -> 0x" << std::hex << xref.to_address;
        std::cout << " (" << xref.description << ")\n";
    }
}

void Analyzer::printStrings(const std::string& pattern) {
    auto results = searchStrings(pattern);
    std::cout << "Strings matching '" << pattern << "':\n";
    for (const auto& s : results) {
        std::cout << "  0x" << std::hex << s.address << ": " << s.value << std::endl;
    }
}

} // namespace kiloader

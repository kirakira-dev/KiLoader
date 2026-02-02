#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "nso_loader.h"
#include "disassembler.h"
#include "function_finder.h"
#include "xref_analyzer.h"
#include "pseudocode.h"
#include "string_table.h"

namespace kiloader {

// Main analyzer class - coordinates all analysis
class Analyzer {
public:
    Analyzer();
    ~Analyzer();
    
    // Load NSO file
    bool loadNso(const std::string& path);
    
    // Run full analysis
    void analyze();
    
    // Get components
    NsoFile& getNso() { return *nso_; }
    Disassembler& getDisassembler() { return *disasm_; }
    FunctionFinder& getFunctionFinder() { return *func_finder_; }
    XRefAnalyzer& getXRefAnalyzer() { return *xref_analyzer_; }
    StringTable& getStringTable() { return *string_table_; }
    PseudocodeGenerator& getPseudocode() { return *pseudocode_; }
    
    // Convenience methods
    
    // Disassemble at address
    std::vector<Instruction> disassembleAt(uint64_t address, size_t count = 20);
    
    // Get function at address
    Function* getFunctionAt(uint64_t address);
    
    // Get pseudocode for function
    std::string getPseudocodeAt(uint64_t address);
    
    // Find references to address
    std::vector<XRef> getRefsTo(uint64_t address);
    
    // Find references from address
    std::vector<XRef> getRefsFrom(uint64_t address);
    
    // Search strings
    std::vector<StringEntry> searchStrings(const std::string& pattern);
    
    // Find string address
    uint64_t findString(const std::string& str);
    
    // Export analysis to file
    void exportToFile(const std::string& path);
    
    // Export functions list
    void exportFunctions(const std::string& path);
    
    // Export strings
    void exportStrings(const std::string& path);
    
    // Interactive commands
    void printDisassembly(uint64_t address, size_t count = 20);
    void printFunction(uint64_t address);
    void printXRefs(uint64_t address);
    void printStrings(const std::string& pattern);
    
private:
    std::unique_ptr<NsoFile> nso_;
    std::unique_ptr<Disassembler> disasm_;
    std::unique_ptr<FunctionFinder> func_finder_;
    std::unique_ptr<XRefAnalyzer> xref_analyzer_;
    std::unique_ptr<StringTable> string_table_;
    std::unique_ptr<PseudocodeGenerator> pseudocode_;
    
    bool loaded_ = false;
    bool analyzed_ = false;
};

} // namespace kiloader

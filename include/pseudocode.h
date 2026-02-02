#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "function_finder.h"
#include "xref_analyzer.h"

namespace kiloader {

// Simple pseudocode generator
// Not a full decompiler, but provides readable C-like output

class PseudocodeGenerator {
public:
    PseudocodeGenerator(NsoFile& nso, FunctionFinder& func_finder, XRefAnalyzer& xref);
    
    // Generate pseudocode for a function
    std::string generate(uint64_t func_address);
    std::string generate(const Function& func);
    
    // Generate for all functions
    std::map<uint64_t, std::string> generateAll();
    
private:
    std::string translateInstruction(const Instruction& insn);
    std::string formatRegister(const std::string& reg);
    std::string formatImmediate(int64_t value);
    std::string formatAddress(uint64_t addr);
    std::string getStringAt(uint64_t addr);
    
    NsoFile& nso_;
    FunctionFinder& func_finder_;
    XRefAnalyzer& xref_;
};

} // namespace kiloader

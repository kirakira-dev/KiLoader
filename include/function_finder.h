#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "nso_loader.h"
#include "disassembler.h"

namespace kiloader {

// Function information
struct Function {
    uint64_t address;
    uint64_t end_address;
    size_t size;
    std::string name;
    std::vector<Instruction> instructions;
    
    // Callees and callers
    std::set<uint64_t> calls_to;      // Functions this function calls
    std::set<uint64_t> called_from;   // Functions that call this function
    
    // Basic blocks (for CFG)
    std::vector<std::pair<uint64_t, uint64_t>> basic_blocks;
    
    bool is_leaf;       // Doesn't call other functions
    bool is_thunk;      // Just a jump to another function
    bool is_noreturn;   // Doesn't return (like abort)
};

// Function finder - detects functions in binary
class FunctionFinder {
public:
    FunctionFinder(NsoFile& nso, Disassembler& disasm);
    
    // Find all functions
    void findFunctions();
    
    // Find functions by pattern
    void findFunctionsByPrologue();
    
    // Find functions by call references
    void findFunctionsByCallTargets();
    
    // Analyze a specific function
    Function* analyzeFunction(uint64_t address);
    
    // Get all functions
    const std::map<uint64_t, Function>& getFunctions() const { return functions_; }
    
    // Get function at address
    Function* getFunction(uint64_t address);
    
    // Get function containing address
    Function* getFunctionContaining(uint64_t address);
    
    // Name a function
    void nameFunction(uint64_t address, const std::string& name);
    
    // Auto-name functions from strings/symbols
    void autoNameFunctions();
    
private:
    bool isPrologue(const uint8_t* code, size_t size);
    bool isEpilogue(const Instruction& insn);
    void analyzeBasicBlocks(Function& func);
    
    NsoFile& nso_;
    Disassembler& disasm_;
    std::map<uint64_t, Function> functions_;
    std::set<uint64_t> analyzed_addresses_;
};

} // namespace kiloader

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "nso_loader.h"
#include "disassembler.h"
#include "function_finder.h"

namespace kiloader {

// Cross-reference types
enum class XRefType {
    Call,           // Function call (BL)
    Jump,           // Direct jump (B)
    DataRead,       // Data read (LDR)
    DataWrite,      // Data write (STR)
    AddressLoad,    // Address load (ADRP+ADD)
    Unknown
};

// Cross-reference entry
struct XRef {
    uint64_t from_address;
    uint64_t to_address;
    XRefType type;
    std::string description;
    
    // Context
    uint64_t from_function;
    std::string from_function_name;
};

// Cross-reference analyzer
class XRefAnalyzer {
public:
    XRefAnalyzer(NsoFile& nso, Disassembler& disasm, FunctionFinder& func_finder);
    
    // Analyze all cross-references
    void analyze();
    
    // Get references TO an address
    std::vector<XRef> getRefsTo(uint64_t address) const;
    
    // Get references FROM an address
    std::vector<XRef> getRefsFrom(uint64_t address) const;
    
    // Get all references to a function
    std::vector<XRef> getCallsTo(uint64_t func_address) const;
    
    // Get all references from a function
    std::vector<XRef> getCallsFrom(uint64_t func_address) const;
    
    // Find references to a string
    std::vector<XRef> getStringRefs(const std::string& str) const;
    
    // Find all data references in rodata range
    std::vector<XRef> getRodataRefs() const;
    
    // Get all xrefs
    const std::vector<XRef>& getAllXRefs() const { return xrefs_; }
    
private:
    void analyzeInstruction(const Instruction& insn, uint64_t func_addr);
    void analyzeAdrpSequence(uint64_t address);
    
    NsoFile& nso_;
    Disassembler& disasm_;
    FunctionFinder& func_finder_;
    
    std::vector<XRef> xrefs_;
    std::map<uint64_t, std::vector<size_t>> refs_to_;    // address -> indices in xrefs_
    std::map<uint64_t, std::vector<size_t>> refs_from_;  // address -> indices in xrefs_
};

} // namespace kiloader

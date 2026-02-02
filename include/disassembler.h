#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <capstone/capstone.h>

namespace kiloader {

// Disassembled instruction
struct Instruction {
    uint64_t address;
    std::vector<uint8_t> bytes;
    std::string mnemonic;
    std::string operands;
    
    // Parsed operand info
    bool is_branch;
    bool is_call;
    bool is_return;
    bool is_load;
    bool is_store;
    uint64_t branch_target;
    
    std::string toString() const;
};

// ARM64 Disassembler using Capstone
class Disassembler {
public:
    Disassembler();
    ~Disassembler();
    
    // Initialize (must call before use)
    bool initialize();
    
    // Disassemble a single instruction
    bool disassembleOne(const uint8_t* code, size_t size, uint64_t address, Instruction& out);
    
    // Disassemble a block of code
    std::vector<Instruction> disassemble(const uint8_t* code, size_t size, uint64_t address, size_t count = 0);
    
    // Disassemble until return or invalid
    std::vector<Instruction> disassembleFunction(const uint8_t* code, size_t max_size, uint64_t address);
    
    // Check if instruction is valid
    bool isValidInstruction(const uint8_t* code, size_t size, uint64_t address);
    
    // Get error message
    std::string getError() const { return error_; }
    
private:
    void parseInstruction(cs_insn* insn, Instruction& out);
    
    csh handle_ = 0;
    bool initialized_ = false;
    std::string error_;
};

} // namespace kiloader

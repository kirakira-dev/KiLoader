#include "disassembler.h"
#include <cstring>
#include <sstream>

namespace kiloader {

Disassembler::Disassembler() = default;

Disassembler::~Disassembler() {
    if (initialized_) {
        cs_close(&handle_);
    }
}

bool Disassembler::initialize() {
    if (initialized_) {
        return true;
    }
    
    cs_err err = cs_open(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, &handle_);
    if (err != CS_ERR_OK) {
        error_ = cs_strerror(err);
        return false;
    }
    
    // Enable detailed mode for operand info
    cs_option(handle_, CS_OPT_DETAIL, CS_OPT_ON);
    
    initialized_ = true;
    return true;
}

bool Disassembler::disassembleOne(const uint8_t* code, size_t size, uint64_t address, Instruction& out) {
    if (!initialized_) {
        error_ = "Disassembler not initialized";
        return false;
    }
    
    cs_insn* insn = nullptr;
    size_t count = cs_disasm(handle_, code, size, address, 1, &insn);
    
    if (count == 0) {
        error_ = "Failed to disassemble instruction";
        return false;
    }
    
    parseInstruction(insn, out);
    cs_free(insn, count);
    
    return true;
}

std::vector<Instruction> Disassembler::disassemble(const uint8_t* code, size_t size, 
                                                    uint64_t address, size_t count) {
    std::vector<Instruction> result;
    
    if (!initialized_) {
        return result;
    }
    
    cs_insn* insn = nullptr;
    size_t actual_count = cs_disasm(handle_, code, size, address, count, &insn);
    
    result.reserve(actual_count);
    for (size_t i = 0; i < actual_count; i++) {
        Instruction inst;
        parseInstruction(&insn[i], inst);
        result.push_back(std::move(inst));
    }
    
    cs_free(insn, actual_count);
    return result;
}

std::vector<Instruction> Disassembler::disassembleFunction(const uint8_t* code, size_t max_size, 
                                                            uint64_t address) {
    std::vector<Instruction> result;
    
    if (!initialized_) {
        return result;
    }
    
    size_t offset = 0;
    while (offset < max_size) {
        Instruction inst;
        if (!disassembleOne(code + offset, max_size - offset, address + offset, inst)) {
            break;
        }
        
        result.push_back(inst);
        offset += inst.bytes.size();
        
        // Stop at return instruction
        if (inst.is_return) {
            break;
        }
        
        // Limit to prevent infinite loops
        if (result.size() > 10000) {
            break;
        }
    }
    
    return result;
}

bool Disassembler::isValidInstruction(const uint8_t* code, size_t size, uint64_t address) {
    if (!initialized_ || size < 4) {
        return false;
    }
    
    cs_insn* insn = nullptr;
    size_t count = cs_disasm(handle_, code, 4, address, 1, &insn);
    
    if (count > 0) {
        cs_free(insn, count);
        return true;
    }
    
    return false;
}

void Disassembler::parseInstruction(cs_insn* insn, Instruction& out) {
    out.address = insn->address;
    out.bytes.assign(insn->bytes, insn->bytes + insn->size);
    out.mnemonic = insn->mnemonic;
    out.operands = insn->op_str;
    
    // Default values
    out.is_branch = false;
    out.is_call = false;
    out.is_return = false;
    out.is_load = false;
    out.is_store = false;
    out.branch_target = 0;
    
    // Analyze instruction type
    cs_detail* detail = insn->detail;
    if (detail) {
        // Check instruction groups
        for (int i = 0; i < detail->groups_count; i++) {
            switch (detail->groups[i]) {
                case CS_GRP_JUMP:
                    out.is_branch = true;
                    break;
                case CS_GRP_CALL:
                    out.is_call = true;
                    break;
                case CS_GRP_RET:
                    out.is_return = true;
                    break;
            }
        }
        
        // Check for branch target
        cs_arm64* arm64 = &detail->arm64;
        for (int i = 0; i < arm64->op_count; i++) {
            cs_arm64_op* op = &arm64->operands[i];
            if (op->type == ARM64_OP_IMM && (out.is_branch || out.is_call)) {
                out.branch_target = op->imm;
            }
        }
    }
    
    // Check mnemonic for load/store
    if (out.mnemonic[0] == 'l' && out.mnemonic[1] == 'd') {
        out.is_load = true;
    } else if (out.mnemonic[0] == 's' && out.mnemonic[1] == 't') {
        out.is_store = true;
    }
    
    // Special cases
    if (out.mnemonic == "bl") {
        out.is_call = true;
    } else if (out.mnemonic == "b" || out.mnemonic == "br") {
        out.is_branch = true;
    } else if (out.mnemonic == "ret") {
        out.is_return = true;
    }
}

std::string Instruction::toString() const {
    std::ostringstream ss;
    ss << std::hex << "0x" << address << ": ";
    
    // Bytes
    for (const auto& b : bytes) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", b);
        ss << buf;
    }
    
    // Pad to align
    for (size_t i = bytes.size(); i < 4; i++) {
        ss << "   ";
    }
    
    ss << mnemonic << " " << operands;
    
    return ss.str();
}

} // namespace kiloader

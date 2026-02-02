#include "function_finder.h"
#include <algorithm>
#include <sstream>

namespace kiloader {

FunctionFinder::FunctionFinder(NsoFile& nso, Disassembler& disasm)
    : nso_(nso), disasm_(disasm) {}

void FunctionFinder::findFunctions() {
    findFunctionsByPrologue();
    findFunctionsByCallTargets();
    autoNameFunctions();
}

void FunctionFinder::findFunctionsByPrologue() {
    const Segment& text = nso_.getTextSegment();
    const uint8_t* code = text.data.data();
    size_t size = text.size;
    uint64_t base = nso_.getBaseAddress() + text.mem_offset;
    
    // Common ARM64 function prologues:
    // STP X29, X30, [SP, #-0x??]!  (save frame pointer and link register)
    // SUB SP, SP, #0x??           (allocate stack frame)
    // STP X??, X??, [SP, #0x??]   (save callee-saved registers)
    
    for (size_t offset = 0; offset + 4 <= size; offset += 4) {
        if (isPrologue(code + offset, size - offset)) {
            uint64_t addr = base + offset;
            if (functions_.find(addr) == functions_.end()) {
                analyzeFunction(addr);
            }
        }
    }
}

void FunctionFinder::findFunctionsByCallTargets() {
    // Look for BL (branch and link) instructions and mark their targets as functions
    const Segment& text = nso_.getTextSegment();
    const uint8_t* code = text.data.data();
    size_t size = text.size;
    uint64_t base = nso_.getBaseAddress() + text.mem_offset;
    
    std::vector<uint64_t> call_targets;
    
    for (size_t offset = 0; offset + 4 <= size; offset += 4) {
        uint32_t insn = *reinterpret_cast<const uint32_t*>(code + offset);
        
        // BL instruction: 0x94000000 | imm26
        if ((insn & 0xFC000000) == 0x94000000) {
            int32_t imm26 = insn & 0x03FFFFFF;
            // Sign extend
            if (imm26 & 0x02000000) {
                imm26 |= 0xFC000000;
            }
            int64_t target = (base + offset) + (imm26 << 2);
            
            if (target >= static_cast<int64_t>(base) && 
                target < static_cast<int64_t>(base + size)) {
                call_targets.push_back(target);
            }
        }
    }
    
    // Analyze each call target as a function
    for (uint64_t target : call_targets) {
        if (functions_.find(target) == functions_.end()) {
            analyzeFunction(target);
        }
    }
}

bool FunctionFinder::isPrologue(const uint8_t* code, size_t size) {
    if (size < 4) return false;
    
    uint32_t insn = *reinterpret_cast<const uint32_t*>(code);
    
    // STP X29, X30, [SP, #-??]! (pre-indexed)
    // Encoding: 0xA9B00000 | (imm7 << 15) | (Rt2 << 10) | (Rn << 5) | Rt
    // For STP X29, X30, [SP, #-16]!: 0xA9BF7BFD
    if ((insn & 0xFFC003E0) == 0xA9800000) {  // STP Xt, Xt, [SP, #imm]!
        uint32_t rt = insn & 0x1F;
        uint32_t rt2 = (insn >> 10) & 0x1F;
        if (rt == 29 && rt2 == 30) {  // X29 (FP) and X30 (LR)
            return true;
        }
    }
    
    // SUB SP, SP, #imm
    // Encoding: 0xD1000000 | (shift << 22) | (imm12 << 10) | (Rn << 5) | Rd
    if ((insn & 0xFF0003FF) == 0xD10003FF) {  // SUB SP, SP, #imm
        return true;
    }
    
    // PACIASP (pointer auth)
    if (insn == 0xD503233F) {
        return true;
    }
    
    return false;
}

bool FunctionFinder::isEpilogue(const Instruction& insn) {
    return insn.is_return;
}

Function* FunctionFinder::analyzeFunction(uint64_t address) {
    if (analyzed_addresses_.count(address)) {
        auto it = functions_.find(address);
        return it != functions_.end() ? &it->second : nullptr;
    }
    
    analyzed_addresses_.insert(address);
    
    const Segment& text = nso_.getTextSegment();
    uint64_t text_base = nso_.getBaseAddress() + text.mem_offset;
    
    if (address < text_base || address >= text_base + text.size) {
        return nullptr;
    }
    
    size_t offset = address - text_base;
    const uint8_t* code = text.data.data() + offset;
    size_t max_size = text.size - offset;
    
    // Disassemble function
    auto instructions = disasm_.disassembleFunction(code, max_size, address);
    if (instructions.empty()) {
        return nullptr;
    }
    
    // Create function entry
    Function func;
    func.address = address;
    func.instructions = std::move(instructions);
    func.end_address = func.instructions.back().address + func.instructions.back().bytes.size();
    func.size = func.end_address - func.address;
    func.is_leaf = true;
    func.is_thunk = false;
    func.is_noreturn = false;
    
    // Generate default name
    std::ostringstream ss;
    ss << "FUN_" << std::hex << address;
    func.name = ss.str();
    
    // Analyze calls
    for (const auto& insn : func.instructions) {
        if (insn.is_call && insn.branch_target != 0) {
            func.calls_to.insert(insn.branch_target);
            func.is_leaf = false;
        }
    }
    
    // Check if thunk (single branch)
    if (func.instructions.size() == 1 && func.instructions[0].is_branch) {
        func.is_thunk = true;
    }
    
    // Insert and return
    auto [it, inserted] = functions_.emplace(address, std::move(func));
    return &it->second;
}

Function* FunctionFinder::getFunction(uint64_t address) {
    auto it = functions_.find(address);
    return it != functions_.end() ? &it->second : nullptr;
}

Function* FunctionFinder::getFunctionContaining(uint64_t address) {
    for (auto& [addr, func] : functions_) {
        if (address >= func.address && address < func.end_address) {
            return &func;
        }
    }
    return nullptr;
}

void FunctionFinder::nameFunction(uint64_t address, const std::string& name) {
    auto it = functions_.find(address);
    if (it != functions_.end()) {
        it->second.name = name;
    }
}

void FunctionFinder::autoNameFunctions() {
    // This is a placeholder - would need string analysis to auto-name
    // Based on string references and patterns
}

void FunctionFinder::analyzeBasicBlocks(Function& func) {
    // Split function into basic blocks at branches and branch targets
    std::set<uint64_t> leaders;
    leaders.insert(func.address);
    
    for (const auto& insn : func.instructions) {
        if (insn.is_branch || insn.is_call) {
            // Next instruction is a leader
            uint64_t next = insn.address + insn.bytes.size();
            if (next < func.end_address) {
                leaders.insert(next);
            }
            // Branch target is a leader
            if (insn.branch_target >= func.address && insn.branch_target < func.end_address) {
                leaders.insert(insn.branch_target);
            }
        }
    }
    
    // Create basic blocks
    std::vector<uint64_t> sorted_leaders(leaders.begin(), leaders.end());
    std::sort(sorted_leaders.begin(), sorted_leaders.end());
    
    func.basic_blocks.clear();
    for (size_t i = 0; i < sorted_leaders.size(); i++) {
        uint64_t start = sorted_leaders[i];
        uint64_t end = (i + 1 < sorted_leaders.size()) ? sorted_leaders[i + 1] : func.end_address;
        func.basic_blocks.emplace_back(start, end);
    }
}

} // namespace kiloader

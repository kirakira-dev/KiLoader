#include "xref_analyzer.h"
#include <sstream>
#include <thread>
#include <mutex>

namespace kiloader {

constexpr int NUM_THREADS = 32;

XRefAnalyzer::XRefAnalyzer(NsoFile& nso, Disassembler& disasm, FunctionFinder& func_finder)
    : nso_(nso), disasm_(disasm), func_finder_(func_finder) {}

void XRefAnalyzer::analyze() {
    xrefs_.clear();
    refs_to_.clear();
    refs_from_.clear();
    
    // Get all function addresses
    std::vector<uint64_t> func_addrs;
    for (const auto& [addr, func] : func_finder_.getFunctions()) {
        func_addrs.push_back(addr);
    }
    
    // Phase 1: Analyze functions in parallel
    std::vector<std::vector<XRef>> thread_results(NUM_THREADS);
    std::vector<std::thread> threads;
    
    size_t chunk_size = func_addrs.size() / NUM_THREADS + 1;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            size_t start = t * chunk_size;
            size_t end = std::min(start + chunk_size, func_addrs.size());
            
            for (size_t i = start; i < end; i++) {
                uint64_t addr = func_addrs[i];
                auto* func = func_finder_.getFunction(addr);
                if (!func) continue;
                
                for (const auto& insn : func->instructions) {
                    // Inline the analysis to avoid race conditions
                    XRef xref;
                    xref.from_address = insn.address;
                    xref.from_function = addr;
                    xref.from_function_name = func->name;
                    
                    if (insn.is_call && insn.branch_target != 0) {
                        xref.to_address = insn.branch_target;
                        xref.type = XRefType::Call;
                        xref.description = "function call";
                        thread_results[t].push_back(xref);
                    }
                    else if (insn.is_branch && insn.branch_target != 0) {
                        xref.to_address = insn.branch_target;
                        xref.type = XRefType::Jump;
                        xref.description = "branch";
                        thread_results[t].push_back(xref);
                    }
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Phase 2: Merge results
    for (auto& results : thread_results) {
        xrefs_.insert(xrefs_.end(), results.begin(), results.end());
    }
    
    // Phase 3: Analyze ADRP sequences (needs memory access, do sequentially)
    for (const auto& [addr, func] : func_finder_.getFunctions()) {
        for (const auto& insn : func.instructions) {
            if (insn.mnemonic == "adrp") {
                analyzeAdrpSequence(insn.address);
            }
        }
    }
    
    // Build reverse indices
    for (size_t i = 0; i < xrefs_.size(); i++) {
        refs_to_[xrefs_[i].to_address].push_back(i);
        refs_from_[xrefs_[i].from_address].push_back(i);
    }
}

void XRefAnalyzer::analyzeInstruction(const Instruction& insn, uint64_t func_addr) {
    XRef xref;
    xref.from_address = insn.address;
    xref.from_function = func_addr;
    
    auto* func = func_finder_.getFunction(func_addr);
    xref.from_function_name = func ? func->name : "unknown";
    
    if (insn.is_call && insn.branch_target != 0) {
        xref.to_address = insn.branch_target;
        xref.type = XRefType::Call;
        xref.description = "function call";
        xrefs_.push_back(xref);
    }
    else if (insn.is_branch && insn.branch_target != 0) {
        xref.to_address = insn.branch_target;
        xref.type = XRefType::Jump;
        xref.description = "branch";
        xrefs_.push_back(xref);
    }
    
    // Check for ADRP + ADD/LDR patterns for data references
    if (insn.mnemonic == "adrp") {
        analyzeAdrpSequence(insn.address);
    }
}

void XRefAnalyzer::analyzeAdrpSequence(uint64_t address) {
    // ADRP loads a page address
    // Usually followed by ADD or LDR to get the final address
    
    uint8_t code[8];
    if (!nso_.readMemory(address, code, 8)) {
        return;
    }
    
    uint32_t adrp_insn = *reinterpret_cast<uint32_t*>(code);
    uint32_t next_insn = *reinterpret_cast<uint32_t*>(code + 4);
    
    // Decode ADRP
    if ((adrp_insn & 0x9F000000) != 0x90000000) {
        return;  // Not ADRP
    }
    
    uint32_t rd = adrp_insn & 0x1F;
    int64_t immhi = (adrp_insn >> 5) & 0x7FFFF;
    int64_t immlo = (adrp_insn >> 29) & 0x3;
    int64_t imm = (immhi << 2) | immlo;
    
    // Sign extend
    if (imm & 0x100000) {
        imm |= 0xFFFFFFFFFFE00000ULL;
    }
    
    uint64_t page_addr = (address & ~0xFFFULL) + (imm << 12);
    
    // Check next instruction
    uint64_t final_addr = 0;
    XRefType type = XRefType::AddressLoad;
    
    // ADD Xd, Xn, #imm
    if ((next_insn & 0xFF800000) == 0x91000000) {
        uint32_t rn = (next_insn >> 5) & 0x1F;
        if (rn == rd) {
            uint32_t imm12 = (next_insn >> 10) & 0xFFF;
            final_addr = page_addr + imm12;
        }
    }
    // LDR Xd, [Xn, #imm]
    else if ((next_insn & 0xFFC00000) == 0xF9400000) {
        uint32_t rn = (next_insn >> 5) & 0x1F;
        if (rn == rd) {
            uint32_t imm12 = ((next_insn >> 10) & 0xFFF) * 8;
            final_addr = page_addr + imm12;
            type = XRefType::DataRead;
        }
    }
    // LDR Wd, [Xn, #imm]
    else if ((next_insn & 0xFFC00000) == 0xB9400000) {
        uint32_t rn = (next_insn >> 5) & 0x1F;
        if (rn == rd) {
            uint32_t imm12 = ((next_insn >> 10) & 0xFFF) * 4;
            final_addr = page_addr + imm12;
            type = XRefType::DataRead;
        }
    }
    
    if (final_addr != 0) {
        XRef xref;
        xref.from_address = address;
        xref.to_address = final_addr;
        xref.type = type;
        xref.description = type == XRefType::DataRead ? "data read" : "address load";
        
        auto* func = func_finder_.getFunctionContaining(address);
        if (func) {
            xref.from_function = func->address;
            xref.from_function_name = func->name;
        }
        
        xrefs_.push_back(xref);
    }
}

std::vector<XRef> XRefAnalyzer::getRefsTo(uint64_t address) const {
    std::vector<XRef> result;
    auto it = refs_to_.find(address);
    if (it != refs_to_.end()) {
        for (size_t idx : it->second) {
            result.push_back(xrefs_[idx]);
        }
    }
    return result;
}

std::vector<XRef> XRefAnalyzer::getRefsFrom(uint64_t address) const {
    std::vector<XRef> result;
    auto it = refs_from_.find(address);
    if (it != refs_from_.end()) {
        for (size_t idx : it->second) {
            result.push_back(xrefs_[idx]);
        }
    }
    return result;
}

std::vector<XRef> XRefAnalyzer::getCallsTo(uint64_t func_address) const {
    std::vector<XRef> result;
    for (const auto& xref : xrefs_) {
        if (xref.to_address == func_address && xref.type == XRefType::Call) {
            result.push_back(xref);
        }
    }
    return result;
}

std::vector<XRef> XRefAnalyzer::getCallsFrom(uint64_t func_address) const {
    std::vector<XRef> result;
    for (const auto& xref : xrefs_) {
        if (xref.from_function == func_address && xref.type == XRefType::Call) {
            result.push_back(xref);
        }
    }
    return result;
}

std::vector<XRef> XRefAnalyzer::getStringRefs(const std::string& str) const {
    // This would need string table integration
    return {};
}

std::vector<XRef> XRefAnalyzer::getRodataRefs() const {
    std::vector<XRef> result;
    
    const Segment& rodata = nso_.getRodataSegment();
    uint64_t rodata_start = nso_.getBaseAddress() + rodata.mem_offset;
    uint64_t rodata_end = rodata_start + rodata.size;
    
    for (const auto& xref : xrefs_) {
        if (xref.to_address >= rodata_start && xref.to_address < rodata_end) {
            result.push_back(xref);
        }
    }
    
    return result;
}

} // namespace kiloader

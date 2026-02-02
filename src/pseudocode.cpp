#include "pseudocode.h"
#include <sstream>
#include <iomanip>
#include <regex>

namespace kiloader {

PseudocodeGenerator::PseudocodeGenerator(NsoFile& nso, FunctionFinder& func_finder, XRefAnalyzer& xref)
    : nso_(nso), func_finder_(func_finder), xref_(xref) {}

std::string PseudocodeGenerator::generate(uint64_t func_address) {
    auto* func = func_finder_.getFunction(func_address);
    if (!func) {
        return "// Function not found\n";
    }
    return generate(*func);
}

std::string PseudocodeGenerator::generate(const Function& func) {
    std::ostringstream ss;
    
    // Function header
    ss << "// Function: " << func.name << "\n";
    ss << "// Address: 0x" << std::hex << func.address << "\n";
    ss << "// Size: " << std::dec << func.size << " bytes\n";
    ss << "// Leaf: " << (func.is_leaf ? "yes" : "no") << "\n";
    ss << "\n";
    
    // Signature (simplified)
    ss << "void " << func.name << "(void) {\n";
    
    // Track register states for basic analysis
    std::map<std::string, std::string> reg_values;
    
    for (const auto& insn : func.instructions) {
        ss << "    // 0x" << std::hex << insn.address << ": ";
        ss << insn.mnemonic << " " << insn.operands << "\n";
        
        // Generate pseudocode
        std::string pseudo = translateInstruction(insn);
        if (!pseudo.empty()) {
            ss << "    " << pseudo << "\n";
        }
        ss << "\n";
    }
    
    ss << "}\n";
    
    return ss.str();
}

std::string PseudocodeGenerator::translateInstruction(const Instruction& insn) {
    std::string m = insn.mnemonic;
    std::string ops = insn.operands;
    
    // Parse operands
    std::vector<std::string> operands;
    std::regex op_regex(R"([xwXW]\d+|sp|SP|#-?\d+|#0x[0-9a-fA-F]+|\[[^\]]+\])");
    std::sregex_iterator it(ops.begin(), ops.end(), op_regex);
    std::sregex_iterator end;
    while (it != end) {
        operands.push_back(it->str());
        ++it;
    }
    
    // MOV
    if (m == "mov" && operands.size() >= 2) {
        return formatRegister(operands[0]) + " = " + formatRegister(operands[1]) + ";";
    }
    
    // ADD
    if (m == "add" && operands.size() >= 3) {
        return formatRegister(operands[0]) + " = " + formatRegister(operands[1]) + 
               " + " + formatRegister(operands[2]) + ";";
    }
    
    // SUB
    if (m == "sub" && operands.size() >= 3) {
        return formatRegister(operands[0]) + " = " + formatRegister(operands[1]) + 
               " - " + formatRegister(operands[2]) + ";";
    }
    
    // MUL
    if (m == "mul" && operands.size() >= 3) {
        return formatRegister(operands[0]) + " = " + formatRegister(operands[1]) + 
               " * " + formatRegister(operands[2]) + ";";
    }
    
    // LDR
    if ((m == "ldr" || m == "ldrsw" || m == "ldrb" || m == "ldrh") && operands.size() >= 2) {
        return formatRegister(operands[0]) + " = *(" + operands[1] + ");";
    }
    
    // STR
    if ((m == "str" || m == "strb" || m == "strh") && operands.size() >= 2) {
        return "*(" + operands[1] + ") = " + formatRegister(operands[0]) + ";";
    }
    
    // BL (call)
    if (m == "bl" && insn.branch_target != 0) {
        std::string target_name;
        auto* target_func = func_finder_.getFunction(insn.branch_target);
        if (target_func) {
            target_name = target_func->name;
        } else {
            std::ostringstream ss;
            ss << "FUN_" << std::hex << insn.branch_target;
            target_name = ss.str();
        }
        return target_name + "();";
    }
    
    // RET
    if (m == "ret") {
        return "return;";
    }
    
    // CMP
    if (m == "cmp" && operands.size() >= 2) {
        return "// compare " + formatRegister(operands[0]) + ", " + formatRegister(operands[1]);
    }
    
    // Conditional branches
    if (m[0] == 'b' && m.length() > 1 && insn.is_branch) {
        std::string cond = m.substr(1);
        std::ostringstream ss;
        ss << "if (" << cond << ") goto 0x" << std::hex << insn.branch_target << ";";
        return ss.str();
    }
    
    // B (unconditional)
    if (m == "b" && insn.branch_target != 0) {
        std::ostringstream ss;
        ss << "goto 0x" << std::hex << insn.branch_target << ";";
        return ss.str();
    }
    
    // STP
    if (m == "stp") {
        return "// save registers to stack";
    }
    
    // LDP
    if (m == "ldp") {
        return "// load registers from stack";
    }
    
    // ADRP
    if (m == "adrp") {
        return "// load page address";
    }
    
    // NOP
    if (m == "nop") {
        return "// nop";
    }
    
    return "";
}

std::string PseudocodeGenerator::formatRegister(const std::string& reg) {
    if (reg.empty()) return reg;
    
    // Convert to lowercase
    std::string r = reg;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    
    // Check for immediates
    if (r[0] == '#') {
        return r.substr(1);  // Remove #
    }
    
    // Named registers
    if (r == "sp") return "sp";
    if (r == "lr" || r == "x30") return "lr";
    if (r == "fp" || r == "x29") return "fp";
    if (r == "xzr" || r == "wzr") return "0";
    
    return r;
}

std::string PseudocodeGenerator::formatImmediate(int64_t value) {
    std::ostringstream ss;
    ss << "0x" << std::hex << value;
    return ss.str();
}

std::string PseudocodeGenerator::formatAddress(uint64_t addr) {
    std::ostringstream ss;
    ss << "0x" << std::hex << addr;
    return ss.str();
}

std::string PseudocodeGenerator::getStringAt(uint64_t addr) {
    // Read null-terminated string from address
    char buf[256];
    size_t i = 0;
    while (i < 255) {
        uint8_t c;
        if (!nso_.readMemory(addr + i, &c, 1)) {
            break;
        }
        if (c == 0) {
            break;
        }
        buf[i++] = c;
    }
    buf[i] = 0;
    return std::string(buf);
}

std::map<uint64_t, std::string> PseudocodeGenerator::generateAll() {
    std::map<uint64_t, std::string> result;
    
    for (const auto& [addr, func] : func_finder_.getFunctions()) {
        result[addr] = generate(func);
    }
    
    return result;
}

} // namespace kiloader

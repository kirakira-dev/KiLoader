#include "analyzer.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace kiloader;

void printHelp() {
    std::cout << R"(
KILOADER - Nintendo Switch NSO Analyzer
========================================

Commands:
  load <path>           Load an NSO file
  analyze               Run full analysis (functions, strings, xrefs)
  
  disasm <addr> [n]     Disassemble n instructions at address
  func <addr|name>      Show function at address or by name (e.g. FUN_7104e53010)
  pseudo <addr|name>    Show pseudocode for function
  
  xref <addr>           Show cross-references to/from address
  xrefto <addr>         Show references TO address
  xreffrom <addr>       Show references FROM address
  
  strings <pattern>     Search for strings containing pattern
  findstr <string>      Find exact string address
  
  list funcs [n]        List functions (optionally first n)
  list funccount        Show function count
  list strcount         Show string count
  list strings [n]      List strings (optionally first n)
  
  export <path>         Export full analysis to file
  expfunc <path>        Export function list
  expstr <path>         Export string list
  
  info                  Show loaded NSO info
  help                  Show this help
  quit                  Exit

Addresses can be in hex (0x...) or decimal.
Function names can be like: FUN_7104e53010 or sub_7104e53010
)";
}

uint64_t parseAddress(const std::string& s) {
    uint64_t addr = 0;
    if (s.substr(0, 2) == "0x" || s.substr(0, 2) == "0X") {
        std::stringstream ss;
        ss << std::hex << s.substr(2);
        ss >> addr;
    } else {
        addr = std::stoull(s);
    }
    return addr;
}

// Parse function name like FUN_7104e53010 or sub_7104e53010
// Returns 0 if not a valid function name format
uint64_t parseFunctionName(const std::string& s) {
    std::string name = s;
    // Convert to uppercase for comparison
    std::string upper = s;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    // Check for FUN_ or SUB_ prefix
    size_t prefix_len = 0;
    if (upper.substr(0, 4) == "FUN_") {
        prefix_len = 4;
    } else if (upper.substr(0, 4) == "SUB_") {
        prefix_len = 4;
    } else {
        return 0;
    }
    
    // Extract hex address part
    std::string hex_part = s.substr(prefix_len);
    if (hex_part.empty()) {
        return 0;
    }
    
    // Check if all characters are hex
    for (char c : hex_part) {
        if (!std::isxdigit(c)) {
            return 0;
        }
    }
    
    // Parse as hex
    uint64_t addr = 0;
    std::stringstream ss;
    ss << std::hex << hex_part;
    ss >> addr;
    return addr;
}

// Parse input that could be either an address or function name
uint64_t parseAddressOrName(const std::string& s) {
    // First try as function name
    uint64_t addr = parseFunctionName(s);
    if (addr != 0) {
        return addr;
    }
    
    // Otherwise parse as address
    return parseAddress(s);
}

int main(int argc, char* argv[]) {
    std::cout << "KILOADER - Nintendo Switch NSO Analyzer\n";
    std::cout << "========================================\n\n";
    
    Analyzer analyzer;
    
    // If path provided as argument, load it
    if (argc > 1) {
        if (!analyzer.loadNso(argv[1])) {
            return 1;
        }
        
        // Auto-analyze if -a flag
        if (argc > 2 && std::string(argv[2]) == "-a") {
            analyzer.analyze();
        }
    }
    
    std::cout << "\nType 'help' for commands.\n\n";
    
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        // Parse command
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        
        // Convert to lowercase
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
        if (cmd.empty()) {
            continue;
        }
        
        if (cmd == "quit" || cmd == "exit" || cmd == "q") {
            break;
        }
        
        if (cmd == "help" || cmd == "h" || cmd == "?") {
            printHelp();
            continue;
        }
        
        if (cmd == "load") {
            std::string path;
            iss >> path;
            if (path.empty()) {
                std::cout << "Usage: load <path>\n";
                continue;
            }
            analyzer.loadNso(path);
            continue;
        }
        
        if (cmd == "analyze") {
            analyzer.analyze();
            continue;
        }
        
        if (cmd == "info") {
            auto& nso = analyzer.getNso();
            std::cout << "Build ID: " << nso.getBuildId() << "\n";
            std::cout << "Base: 0x" << std::hex << nso.getBaseAddress() << "\n";
            std::cout << "Text: 0x" << nso.getTextSegment().size << " bytes\n";
            std::cout << "Rodata: 0x" << nso.getRodataSegment().size << " bytes\n";
            std::cout << "Data: 0x" << nso.getDataSegment().size << " bytes\n";
            std::cout << std::dec;
            continue;
        }
        
        if (cmd == "disasm" || cmd == "d") {
            std::string addr_str;
            size_t count = 20;
            iss >> addr_str;
            iss >> count;
            
            if (addr_str.empty()) {
                std::cout << "Usage: disasm <addr|name> [count]\n";
                std::cout << "  Examples: disasm 0x7104e53010 50\n";
                std::cout << "            disasm FUN_7104e53010\n";
                continue;
            }
            
            uint64_t addr = parseAddressOrName(addr_str);
            if (addr == 0) {
                std::cout << "Invalid address or function name: " << addr_str << "\n";
                continue;
            }
            analyzer.printDisassembly(addr, count);
            continue;
        }
        
        if (cmd == "func" || cmd == "f") {
            std::string addr_str;
            iss >> addr_str;
            
            if (addr_str.empty()) {
                std::cout << "Usage: func <addr|name>\n";
                std::cout << "  Examples: func 0x7104e53010\n";
                std::cout << "            func FUN_7104e53010\n";
                continue;
            }
            
            uint64_t addr = parseAddressOrName(addr_str);
            if (addr == 0) {
                std::cout << "Invalid address or function name: " << addr_str << "\n";
                continue;
            }
            analyzer.printFunction(addr);
            continue;
        }
        
        if (cmd == "pseudo" || cmd == "p") {
            std::string addr_str;
            iss >> addr_str;
            
            if (addr_str.empty()) {
                std::cout << "Usage: pseudo <addr|name>\n";
                std::cout << "  Examples: pseudo 0x7104e53010\n";
                std::cout << "            pseudo FUN_7104e53010\n";
                continue;
            }
            
            uint64_t addr = parseAddressOrName(addr_str);
            if (addr == 0) {
                std::cout << "Invalid address or function name: " << addr_str << "\n";
                continue;
            }
            std::cout << analyzer.getPseudocodeAt(addr);
            continue;
        }
        
        if (cmd == "xref" || cmd == "x") {
            std::string addr_str;
            iss >> addr_str;
            
            if (addr_str.empty()) {
                std::cout << "Usage: xref <addr>\n";
                continue;
            }
            
            uint64_t addr = parseAddress(addr_str);
            analyzer.printXRefs(addr);
            continue;
        }
        
        if (cmd == "xrefto") {
            std::string addr_str;
            iss >> addr_str;
            
            if (addr_str.empty()) {
                std::cout << "Usage: xrefto <addr>\n";
                continue;
            }
            
            uint64_t addr = parseAddress(addr_str);
            auto refs = analyzer.getRefsTo(addr);
            std::cout << "References TO 0x" << std::hex << addr << ":\n";
            for (const auto& xref : refs) {
                std::cout << "  0x" << std::hex << xref.from_address;
                std::cout << " in " << xref.from_function_name << "\n";
            }
            continue;
        }
        
        if (cmd == "xreffrom") {
            std::string addr_str;
            iss >> addr_str;
            
            if (addr_str.empty()) {
                std::cout << "Usage: xreffrom <addr>\n";
                continue;
            }
            
            uint64_t addr = parseAddress(addr_str);
            auto refs = analyzer.getRefsFrom(addr);
            std::cout << "References FROM 0x" << std::hex << addr << ":\n";
            for (const auto& xref : refs) {
                std::cout << "  -> 0x" << std::hex << xref.to_address << "\n";
            }
            continue;
        }
        
        if (cmd == "strings" || cmd == "s") {
            std::string pattern;
            std::getline(iss >> std::ws, pattern);
            
            if (pattern.empty()) {
                std::cout << "Usage: strings <pattern>\n";
                continue;
            }
            
            analyzer.printStrings(pattern);
            continue;
        }
        
        if (cmd == "findstr") {
            std::string str;
            std::getline(iss >> std::ws, str);
            
            if (str.empty()) {
                std::cout << "Usage: findstr <string>\n";
                continue;
            }
            
            uint64_t addr = analyzer.findString(str);
            if (addr) {
                std::cout << "Found at 0x" << std::hex << addr << std::dec << "\n";
            } else {
                std::cout << "Not found\n";
            }
            continue;
        }
        
        if (cmd == "list" || cmd == "l") {
            std::string subcmd;
            iss >> subcmd;
            
            // Convert to lowercase
            std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(), ::tolower);
            
            if (subcmd.empty()) {
                std::cout << "Usage: list <funcs|funccount|strcount|strings> [limit]\n";
                std::cout << "  list funcs [n]     - List all/first n functions\n";
                std::cout << "  list funccount     - Show function count\n";
                std::cout << "  list strcount      - Show string count\n";
                std::cout << "  list strings [n]   - List all/first n strings\n";
                continue;
            }
            
            if (subcmd == "funcs" || subcmd == "functions" || subcmd == "func") {
                size_t limit = 0;
                iss >> limit;  // Optional limit
                
                auto& funcs = analyzer.getFunctionFinder().getFunctions();
                size_t count = 0;
                for (const auto& [addr, func] : funcs) {
                    std::cout << "0x" << std::hex << addr << ": " << func.name;
                    std::cout << " (" << std::dec << func.size << " bytes)\n";
                    count++;
                    if (limit > 0 && count >= limit) {
                        std::cout << "... (showing " << limit << " of " << funcs.size() << ")\n";
                        break;
                    }
                }
                if (limit == 0 || count < limit) {
                    std::cout << "Total: " << funcs.size() << " functions\n";
                }
                continue;
            }
            
            if (subcmd == "funccount" || subcmd == "fc") {
                std::cout << "Functions: " << analyzer.getFunctionFinder().getFunctions().size() << "\n";
                continue;
            }
            
            if (subcmd == "strcount" || subcmd == "sc") {
                std::cout << "Strings: " << analyzer.getStringTable().getStrings().size() << "\n";
                continue;
            }
            
            if (subcmd == "strings" || subcmd == "strs" || subcmd == "str") {
                size_t limit = 0;
                iss >> limit;  // Optional limit
                
                auto& strs = analyzer.getStringTable().getStrings();
                size_t count = 0;
                for (const auto& entry : strs) {
                    std::string display = entry.value;
                    if (display.length() > 80) {
                        display = display.substr(0, 77) + "...";
                    }
                    // Escape newlines for display
                    for (size_t i = 0; i < display.length(); i++) {
                        if (display[i] == '\n') display[i] = ' ';
                        if (display[i] == '\r') display[i] = ' ';
                        if (display[i] == '\t') display[i] = ' ';
                    }
                    std::cout << "0x" << std::hex << entry.address << std::dec;
                    std::cout << " [" << entry.value.length() << "]: " << display << "\n";
                    count++;
                    if (limit > 0 && count >= limit) {
                        std::cout << "... (showing " << limit << " of " << strs.size() << ")\n";
                        break;
                    }
                }
                if (limit == 0 || count < limit) {
                    std::cout << "Total: " << strs.size() << " strings\n";
                }
                continue;
            }
            
            std::cout << "Unknown list subcommand: " << subcmd << "\n";
            std::cout << "Valid: funcs, funccount, strcount, strings\n";
            continue;
        }
        
        // Keep old shortcuts for backward compatibility
        if (cmd == "funcs") {
            auto& funcs = analyzer.getFunctionFinder().getFunctions();
            for (const auto& [addr, func] : funcs) {
                std::cout << "0x" << std::hex << addr << ": " << func.name;
                std::cout << " (" << std::dec << func.size << " bytes)\n";
            }
            std::cout << "Total: " << funcs.size() << " functions\n";
            continue;
        }
        
        if (cmd == "funccount") {
            std::cout << "Functions: " << analyzer.getFunctionFinder().getFunctions().size() << "\n";
            continue;
        }
        
        if (cmd == "strcount") {
            std::cout << "Strings: " << analyzer.getStringTable().getStrings().size() << "\n";
            continue;
        }
        
        if (cmd == "export") {
            std::string path;
            iss >> path;
            if (path.empty()) {
                std::cout << "Usage: export <path>\n";
                continue;
            }
            analyzer.exportToFile(path);
            continue;
        }
        
        if (cmd == "expfunc") {
            std::string path;
            iss >> path;
            if (path.empty()) {
                std::cout << "Usage: expfunc <path>\n";
                continue;
            }
            analyzer.exportFunctions(path);
            std::cout << "Exported functions to: " << path << "\n";
            continue;
        }
        
        if (cmd == "expstr") {
            std::string path;
            iss >> path;
            if (path.empty()) {
                std::cout << "Usage: expstr <path>\n";
                continue;
            }
            analyzer.exportStrings(path);
            std::cout << "Exported strings to: " << path << "\n";
            continue;
        }
        
        std::cout << "Unknown command: " << cmd << ". Type 'help' for commands.\n";
    }
    
    std::cout << "Goodbye!\n";
    return 0;
}

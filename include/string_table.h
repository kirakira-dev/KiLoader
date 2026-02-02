#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "nso_loader.h"

namespace kiloader {

// String entry
struct StringEntry {
    uint64_t address;
    std::string value;
    size_t length;
    bool is_wide;  // UTF-16
};

// String table - finds and manages strings in the binary
class StringTable {
public:
    StringTable(NsoFile& nso);
    
    // Find all strings in rodata
    void findStrings(size_t min_length = 4);
    
    // Search for strings containing pattern
    std::vector<StringEntry> search(const std::string& pattern, bool case_sensitive = false) const;
    
    // Get string at address
    const StringEntry* getString(uint64_t address) const;
    
    // Get all strings
    const std::vector<StringEntry>& getStrings() const { return strings_; }
    
    // Check if address is a string
    bool isString(uint64_t address) const;
    
    // Get string value at address (returns empty if not a string)
    std::string getStringValue(uint64_t address) const;
    
private:
    bool isAsciiPrintable(uint8_t c) const;
    bool isValidStringChar(uint8_t c) const;
    
    NsoFile& nso_;
    std::vector<StringEntry> strings_;
    std::map<uint64_t, size_t> address_map_;  // address -> index in strings_
};

} // namespace kiloader

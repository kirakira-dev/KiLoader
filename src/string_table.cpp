#include "string_table.h"
#include <algorithm>
#include <cctype>

namespace kiloader {

StringTable::StringTable(NsoFile& nso) : nso_(nso) {}

void StringTable::findStrings(size_t min_length) {
    strings_.clear();
    address_map_.clear();
    
    const Segment& rodata = nso_.getRodataSegment();
    const uint8_t* data = rodata.data.data();
    size_t size = rodata.size;
    uint64_t base = nso_.getBaseAddress() + rodata.mem_offset;
    
    size_t i = 0;
    while (i < size) {
        // Check if this looks like a string start
        if (!isValidStringChar(data[i])) {
            i++;
            continue;
        }
        
        // Find string end
        size_t start = i;
        size_t len = 0;
        bool valid = true;
        
        while (i < size && data[i] != 0) {
            if (!isValidStringChar(data[i])) {
                valid = false;
                break;
            }
            len++;
            i++;
        }
        
        // Check if null terminated
        if (i < size && data[i] == 0 && valid && len >= min_length) {
            StringEntry entry;
            entry.address = base + start;
            entry.value = std::string(reinterpret_cast<const char*>(data + start), len);
            entry.length = len;
            entry.is_wide = false;
            
            address_map_[entry.address] = strings_.size();
            strings_.push_back(std::move(entry));
        }
        
        i++;
    }
}

std::vector<StringEntry> StringTable::search(const std::string& pattern, bool case_sensitive) const {
    std::vector<StringEntry> result;
    
    std::string search_pattern = pattern;
    if (!case_sensitive) {
        std::transform(search_pattern.begin(), search_pattern.end(), 
                      search_pattern.begin(), ::tolower);
    }
    
    for (const auto& entry : strings_) {
        std::string value = entry.value;
        if (!case_sensitive) {
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        }
        
        if (value.find(search_pattern) != std::string::npos) {
            result.push_back(entry);
        }
    }
    
    return result;
}

const StringEntry* StringTable::getString(uint64_t address) const {
    auto it = address_map_.find(address);
    if (it != address_map_.end()) {
        return &strings_[it->second];
    }
    return nullptr;
}

bool StringTable::isString(uint64_t address) const {
    return address_map_.find(address) != address_map_.end();
}

std::string StringTable::getStringValue(uint64_t address) const {
    auto* entry = getString(address);
    return entry ? entry->value : "";
}

bool StringTable::isAsciiPrintable(uint8_t c) const {
    return c >= 0x20 && c <= 0x7E;
}

bool StringTable::isValidStringChar(uint8_t c) const {
    // Printable ASCII, tab, newline, carriage return
    return isAsciiPrintable(c) || c == '\t' || c == '\n' || c == '\r';
}

} // namespace kiloader

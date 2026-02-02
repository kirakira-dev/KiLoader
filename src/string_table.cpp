#include "string_table.h"
#include <algorithm>
#include <cctype>
#include <thread>
#include <mutex>

namespace kiloader {

constexpr int NUM_THREADS = 16;

StringTable::StringTable(NsoFile& nso) : nso_(nso) {}

void StringTable::findStrings(size_t min_length) {
    strings_.clear();
    address_map_.clear();
    
    const Segment& rodata = nso_.getRodataSegment();
    const uint8_t* data = rodata.data.data();
    size_t size = rodata.size;
    uint64_t base = nso_.getBaseAddress() + rodata.mem_offset;
    
    // Phase 1: Find strings in parallel
    std::vector<std::vector<StringEntry>> thread_results(NUM_THREADS);
    std::vector<std::thread> threads;
    
    size_t chunk_size = size / NUM_THREADS + 1;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t, min_length]() {
            size_t start = t * chunk_size;
            size_t end = std::min(start + chunk_size + 256, size);  // Overlap for strings spanning chunks
            
            // Adjust start to avoid cutting strings (skip until null or invalid char)
            if (t > 0 && start < size) {
                while (start < end && data[start] != 0 && isValidStringChar(data[start])) {
                    start++;
                }
                if (start < end && data[start] == 0) start++;
            }
            
            size_t i = start;
            while (i < end && i < size) {
                if (!isValidStringChar(data[i])) {
                    i++;
                    continue;
                }
                
                size_t str_start = i;
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
                
                // Only add if within our chunk (avoid duplicates from overlap)
                if (i < size && data[i] == 0 && valid && len >= min_length) {
                    if (str_start >= t * chunk_size && str_start < (t + 1) * chunk_size) {
                        StringEntry entry;
                        entry.address = base + str_start;
                        entry.value = std::string(reinterpret_cast<const char*>(data + str_start), len);
                        entry.length = len;
                        entry.is_wide = false;
                        thread_results[t].push_back(std::move(entry));
                    }
                }
                
                i++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Phase 2: Merge results
    for (auto& results : thread_results) {
        for (auto& entry : results) {
            address_map_[entry.address] = strings_.size();
            strings_.push_back(std::move(entry));
        }
    }
    
    // Sort by address
    std::sort(strings_.begin(), strings_.end(), 
              [](const StringEntry& a, const StringEntry& b) { return a.address < b.address; });
    
    // Rebuild address map after sort
    address_map_.clear();
    for (size_t i = 0; i < strings_.size(); i++) {
        address_map_[strings_[i].address] = i;
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

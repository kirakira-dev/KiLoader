#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include "analyzer.h"

namespace kiloader {

// Binary progress file format
// Header: KILO + version + build_id + counts
// Then serialized functions, strings, xrefs

constexpr uint32_t PROGRESS_MAGIC = 0x4F4C494B;  // "KILO"
constexpr uint32_t PROGRESS_VERSION = 1;

struct ProgressHeader {
    uint32_t magic;
    uint32_t version;
    char build_id[64];
    uint64_t function_count;
    uint64_t string_count;
    uint64_t xref_count;
    uint64_t text_size;
    uint64_t rodata_size;
    uint64_t data_size;
};

class ProgressManager {
public:
    ProgressManager();
    
    // Set the base directory for progress files
    // Default: <executable_dir>/kiloader/tmp
    void setBaseDir(const std::string& dir);
    std::string getBaseDir() const { return base_dir_; }
    
    // Get progress directory for a specific build ID
    std::string getProgressDir(const std::string& build_id) const;
    
    // Save analysis progress
    bool saveProgress(Analyzer& analyzer);
    
    // Load analysis progress
    bool loadProgress(Analyzer& analyzer, const std::string& build_id);
    
    // Check if progress exists for a build ID
    bool hasProgress(const std::string& build_id) const;
    
    // List all saved progress (returns build IDs)
    std::vector<std::string> listProgress() const;
    
    // Delete progress for a build ID
    bool deleteProgress(const std::string& build_id);
    
    // Get last error message
    std::string getError() const { return error_; }
    
private:
    std::string base_dir_;
    std::string error_;
    
    // Serialization helpers
    bool writeFunctions(std::ofstream& f, const std::map<uint64_t, Function>& funcs);
    bool readFunctions(std::ifstream& f, uint64_t count, std::map<uint64_t, Function>& funcs);
    
    bool writeStrings(std::ofstream& f, const std::vector<StringEntry>& strings);
    bool readStrings(std::ifstream& f, uint64_t count, std::vector<StringEntry>& strings);
    
    bool writeXRefs(std::ofstream& f, const std::vector<XRef>& xrefs);
    bool readXRefs(std::ifstream& f, uint64_t count, std::vector<XRef>& xrefs);
    
    // Helper to write length-prefixed string
    void writeString(std::ofstream& f, const std::string& s);
    std::string readString(std::ifstream& f);
};

} // namespace kiloader

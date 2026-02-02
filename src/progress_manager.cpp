#include "progress_manager.h"
#include <fstream>
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#endif

namespace kiloader {

namespace fs = std::filesystem;

ProgressManager::ProgressManager() {
    // Get executable directory
    std::string exe_dir;
    
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    exe_dir = fs::path(path).parent_path().string();
#else
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) {
        exe_dir = fs::path(std::string(path, count)).parent_path().string();
    } else {
        // Fallback for macOS
        exe_dir = ".";
    }
#endif
    
    base_dir_ = exe_dir + "/kiloader/tmp";
}

void ProgressManager::setBaseDir(const std::string& dir) {
    base_dir_ = dir;
}

std::string ProgressManager::getProgressDir(const std::string& build_id) const {
    // Use first 16 chars of build ID for directory name
    std::string short_id = build_id.substr(0, 16);
    return base_dir_ + "/" + short_id;
}

bool ProgressManager::saveProgress(Analyzer& analyzer) {
    if (!analyzer.getNso().isLoaded()) {
        error_ = "No NSO loaded";
        return false;
    }
    
    std::string build_id = analyzer.getNso().getBuildId();
    std::string dir = getProgressDir(build_id);
    
    // Create directory
    try {
        fs::create_directories(dir);
    } catch (const std::exception& e) {
        error_ = "Failed to create directory: " + std::string(e.what());
        return false;
    }
    
    std::string filepath = dir + "/progress.bin";
    std::ofstream f(filepath, std::ios::binary);
    if (!f) {
        error_ = "Failed to open file for writing: " + filepath;
        return false;
    }
    
    // Write header
    ProgressHeader header;
    header.magic = PROGRESS_MAGIC;
    header.version = PROGRESS_VERSION;
    memset(header.build_id, 0, sizeof(header.build_id));
    strncpy(header.build_id, build_id.c_str(), sizeof(header.build_id) - 1);
    
    auto& funcs = analyzer.getFunctionFinder().getFunctions();
    auto& strings = analyzer.getStringTable().getStrings();
    auto& xrefs = analyzer.getXRefAnalyzer().getAllXRefs();
    
    header.function_count = funcs.size();
    header.string_count = strings.size();
    header.xref_count = xrefs.size();
    header.text_size = analyzer.getNso().getTextSegment().size;
    header.rodata_size = analyzer.getNso().getRodataSegment().size;
    header.data_size = analyzer.getNso().getDataSegment().size;
    
    f.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // Write functions
    if (!writeFunctions(f, funcs)) {
        return false;
    }
    
    // Write strings
    if (!writeStrings(f, strings)) {
        return false;
    }
    
    // Write xrefs
    if (!writeXRefs(f, xrefs)) {
        return false;
    }
    
    std::cout << "Progress saved to: " << filepath << std::endl;
    return true;
}

bool ProgressManager::loadProgress(Analyzer& analyzer, const std::string& build_id) {
    std::string dir = getProgressDir(build_id);
    std::string filepath = dir + "/progress.bin";
    
    std::ifstream f(filepath, std::ios::binary);
    if (!f) {
        error_ = "Failed to open file: " + filepath;
        return false;
    }
    
    // Read header
    ProgressHeader header;
    f.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (header.magic != PROGRESS_MAGIC) {
        error_ = "Invalid progress file (bad magic)";
        return false;
    }
    
    if (header.version != PROGRESS_VERSION) {
        error_ = "Incompatible progress file version";
        return false;
    }
    
    std::cout << "Loading progress for build ID: " << header.build_id << std::endl;
    std::cout << "  Functions: " << header.function_count << std::endl;
    std::cout << "  Strings: " << header.string_count << std::endl;
    std::cout << "  XRefs: " << header.xref_count << std::endl;
    
    // Note: Loading into analyzer requires the NSO to be loaded first
    // This is a simplified implementation - full implementation would
    // restore full state without needing the NSO
    
    return true;
}

bool ProgressManager::hasProgress(const std::string& build_id) const {
    std::string filepath = getProgressDir(build_id) + "/progress.bin";
    return fs::exists(filepath);
}

std::vector<std::string> ProgressManager::listProgress() const {
    std::vector<std::string> result;
    
    if (!fs::exists(base_dir_)) {
        return result;
    }
    
    for (const auto& entry : fs::directory_iterator(base_dir_)) {
        if (entry.is_directory()) {
            std::string progress_file = entry.path().string() + "/progress.bin";
            if (fs::exists(progress_file)) {
                // Read build ID from header
                std::ifstream f(progress_file, std::ios::binary);
                if (f) {
                    ProgressHeader header;
                    f.read(reinterpret_cast<char*>(&header), sizeof(header));
                    if (header.magic == PROGRESS_MAGIC) {
                        result.push_back(header.build_id);
                    }
                }
            }
        }
    }
    
    return result;
}

bool ProgressManager::deleteProgress(const std::string& build_id) {
    std::string dir = getProgressDir(build_id);
    
    try {
        fs::remove_all(dir);
        return true;
    } catch (const std::exception& e) {
        error_ = "Failed to delete: " + std::string(e.what());
        return false;
    }
}

bool ProgressManager::writeFunctions(std::ofstream& f, const std::map<uint64_t, Function>& funcs) {
    for (const auto& [addr, func] : funcs) {
        f.write(reinterpret_cast<const char*>(&func.address), sizeof(func.address));
        f.write(reinterpret_cast<const char*>(&func.end_address), sizeof(func.end_address));
        f.write(reinterpret_cast<const char*>(&func.size), sizeof(func.size));
        
        uint8_t flags = (func.is_leaf ? 1 : 0) | 
                       (func.is_thunk ? 2 : 0) | 
                       (func.is_noreturn ? 4 : 0);
        f.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
        
        writeString(f, func.name);
    }
    return f.good();
}

bool ProgressManager::readFunctions(std::ifstream& f, uint64_t count, std::map<uint64_t, Function>& funcs) {
    for (uint64_t i = 0; i < count; i++) {
        Function func;
        f.read(reinterpret_cast<char*>(&func.address), sizeof(func.address));
        f.read(reinterpret_cast<char*>(&func.end_address), sizeof(func.end_address));
        f.read(reinterpret_cast<char*>(&func.size), sizeof(func.size));
        
        uint8_t flags;
        f.read(reinterpret_cast<char*>(&flags), sizeof(flags));
        func.is_leaf = (flags & 1) != 0;
        func.is_thunk = (flags & 2) != 0;
        func.is_noreturn = (flags & 4) != 0;
        
        func.name = readString(f);
        funcs[func.address] = std::move(func);
    }
    return f.good();
}

bool ProgressManager::writeStrings(std::ofstream& f, const std::vector<StringEntry>& strings) {
    for (const auto& entry : strings) {
        f.write(reinterpret_cast<const char*>(&entry.address), sizeof(entry.address));
        f.write(reinterpret_cast<const char*>(&entry.length), sizeof(entry.length));
        uint8_t is_wide = entry.is_wide ? 1 : 0;
        f.write(reinterpret_cast<const char*>(&is_wide), sizeof(is_wide));
        writeString(f, entry.value);
    }
    return f.good();
}

bool ProgressManager::readStrings(std::ifstream& f, uint64_t count, std::vector<StringEntry>& strings) {
    strings.reserve(count);
    for (uint64_t i = 0; i < count; i++) {
        StringEntry entry;
        f.read(reinterpret_cast<char*>(&entry.address), sizeof(entry.address));
        f.read(reinterpret_cast<char*>(&entry.length), sizeof(entry.length));
        uint8_t is_wide;
        f.read(reinterpret_cast<char*>(&is_wide), sizeof(is_wide));
        entry.is_wide = is_wide != 0;
        entry.value = readString(f);
        strings.push_back(std::move(entry));
    }
    return f.good();
}

bool ProgressManager::writeXRefs(std::ofstream& f, const std::vector<XRef>& xrefs) {
    for (const auto& xref : xrefs) {
        f.write(reinterpret_cast<const char*>(&xref.from_address), sizeof(xref.from_address));
        f.write(reinterpret_cast<const char*>(&xref.to_address), sizeof(xref.to_address));
        uint8_t type = static_cast<uint8_t>(xref.type);
        f.write(reinterpret_cast<const char*>(&type), sizeof(type));
        f.write(reinterpret_cast<const char*>(&xref.from_function), sizeof(xref.from_function));
        writeString(f, xref.description);
        writeString(f, xref.from_function_name);
    }
    return f.good();
}

bool ProgressManager::readXRefs(std::ifstream& f, uint64_t count, std::vector<XRef>& xrefs) {
    xrefs.reserve(count);
    for (uint64_t i = 0; i < count; i++) {
        XRef xref;
        f.read(reinterpret_cast<char*>(&xref.from_address), sizeof(xref.from_address));
        f.read(reinterpret_cast<char*>(&xref.to_address), sizeof(xref.to_address));
        uint8_t type;
        f.read(reinterpret_cast<char*>(&type), sizeof(type));
        xref.type = static_cast<XRefType>(type);
        f.read(reinterpret_cast<char*>(&xref.from_function), sizeof(xref.from_function));
        xref.description = readString(f);
        xref.from_function_name = readString(f);
        xrefs.push_back(std::move(xref));
    }
    return f.good();
}

void ProgressManager::writeString(std::ofstream& f, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.length());
    f.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) {
        f.write(s.data(), len);
    }
}

std::string ProgressManager::readString(std::ifstream& f) {
    uint32_t len;
    f.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (len == 0) {
        return "";
    }
    std::string s(len, '\0');
    f.read(&s[0], len);
    return s;
}

} // namespace kiloader

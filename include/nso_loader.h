#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace kiloader {

// NSO Segment header
struct NsoSegmentHeader {
    uint32_t file_offset;
    uint32_t mem_offset;
    uint32_t size;
};

// NSO Header structure (matches Nintendo Switch format)
struct NsoHeader {
    uint32_t magic;              // "NSO0" = 0x304F534E
    uint32_t version;
    uint32_t reserved;
    uint32_t flags;              // bit 0: text compressed, bit 1: rodata, bit 2: data
    
    NsoSegmentHeader text;       // 0x10
    uint32_t module_name_offset; // 0x1C
    NsoSegmentHeader rodata;     // 0x20
    uint32_t module_name_size;   // 0x2C
    NsoSegmentHeader data;       // 0x30
    uint32_t bss_size;           // 0x3C
    
    uint8_t build_id[32];        // 0x40
    
    uint32_t text_compressed_size;    // 0x60
    uint32_t rodata_compressed_size;  // 0x64
    uint32_t data_compressed_size;    // 0x68
    
    uint8_t reserved2[28];       // 0x6C
    
    // API info (0x88)
    uint32_t api_info_offset;
    uint32_t api_info_size;
    
    // Dynstr (0x90)
    uint32_t dynstr_offset;
    uint32_t dynstr_size;
    
    // Dynsym (0x98)
    uint32_t dynsym_offset;
    uint32_t dynsym_size;
    
    // Segment hashes (0xA0)
    uint8_t text_hash[32];
    uint8_t rodata_hash[32];
    uint8_t data_hash[32];
};

// Segment types
enum class SegmentType {
    Text,
    Rodata,
    Data
};

// Segment info
struct Segment {
    SegmentType type;
    uint64_t mem_offset;
    uint64_t size;
    std::vector<uint8_t> data;
    bool is_executable;
    bool is_writable;
};

// NSO file representation
class NsoFile {
public:
    NsoFile() = default;
    ~NsoFile() = default;
    
    // Load NSO from file
    bool load(const std::string& path);
    
    // Get segments
    const Segment& getTextSegment() const { return text_; }
    const Segment& getRodataSegment() const { return rodata_; }
    const Segment& getDataSegment() const { return data_; }
    
    // Get header
    const NsoHeader& getHeader() const { return header_; }
    
    // Get build ID as string
    std::string getBuildId() const;
    
    // Get base address (default for Switch)
    uint64_t getBaseAddress() const { return base_address_; }
    void setBaseAddress(uint64_t addr) { base_address_ = addr; }
    
    // Read memory at virtual address
    bool readMemory(uint64_t vaddr, void* buf, size_t size) const;
    
    // Get segment containing address
    const Segment* getSegmentAt(uint64_t vaddr) const;
    
    // Total size
    size_t getTotalSize() const;
    
private:
    bool decompressSegment(const uint8_t* compressed, size_t comp_size, 
                          std::vector<uint8_t>& output, size_t decomp_size);
    
    NsoHeader header_{};
    Segment text_;
    Segment rodata_;
    Segment data_;
    uint64_t base_address_ = 0x7100000000;  // Default Switch base
    std::vector<uint8_t> raw_data_;
};

} // namespace kiloader

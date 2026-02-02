#include "nso_loader.h"
#include <fstream>
#include <cstring>
#include <lz4.h>

namespace kiloader {

bool NsoFile::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // Read entire file
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    raw_data_.resize(file_size);
    file.read(reinterpret_cast<char*>(raw_data_.data()), file_size);
    file.close();
    
    // Parse header
    if (file_size < sizeof(NsoHeader)) {
        return false;
    }
    
    std::memcpy(&header_, raw_data_.data(), sizeof(NsoHeader));
    
    // Check magic
    if (header_.magic != 0x304F534E) {  // "NSO0"
        return false;
    }
    
    // Check flags for compression
    bool text_compressed = (header_.flags & 1) != 0;
    bool rodata_compressed = (header_.flags & 2) != 0;
    bool data_compressed = (header_.flags & 4) != 0;
    
    // Load text segment
    text_.type = SegmentType::Text;
    text_.mem_offset = header_.text.mem_offset;
    text_.size = header_.text.size;
    text_.is_executable = true;
    text_.is_writable = false;
    
    if (text_compressed) {
        if (!decompressSegment(raw_data_.data() + header_.text.file_offset,
                              header_.text_compressed_size,
                              text_.data, header_.text.size)) {
            return false;
        }
    } else {
        text_.data.resize(header_.text.size);
        std::memcpy(text_.data.data(), raw_data_.data() + header_.text.file_offset, header_.text.size);
    }
    
    // Load rodata segment
    rodata_.type = SegmentType::Rodata;
    rodata_.mem_offset = header_.rodata.mem_offset;
    rodata_.size = header_.rodata.size;
    rodata_.is_executable = false;
    rodata_.is_writable = false;
    
    if (rodata_compressed) {
        if (!decompressSegment(raw_data_.data() + header_.rodata.file_offset,
                              header_.rodata_compressed_size,
                              rodata_.data, header_.rodata.size)) {
            return false;
        }
    } else {
        rodata_.data.resize(header_.rodata.size);
        std::memcpy(rodata_.data.data(), raw_data_.data() + header_.rodata.file_offset, header_.rodata.size);
    }
    
    // Load data segment
    data_.type = SegmentType::Data;
    data_.mem_offset = header_.data.mem_offset;
    data_.size = header_.data.size;
    data_.is_executable = false;
    data_.is_writable = true;
    
    if (data_compressed) {
        if (!decompressSegment(raw_data_.data() + header_.data.file_offset,
                              header_.data_compressed_size,
                              data_.data, header_.data.size)) {
            return false;
        }
    } else {
        data_.data.resize(header_.data.size);
        std::memcpy(data_.data.data(), raw_data_.data() + header_.data.file_offset, header_.data.size);
    }
    
    return true;
}

bool NsoFile::decompressSegment(const uint8_t* compressed, size_t comp_size,
                                std::vector<uint8_t>& output, size_t decomp_size) {
    output.resize(decomp_size);
    
    int result = LZ4_decompress_safe(
        reinterpret_cast<const char*>(compressed),
        reinterpret_cast<char*>(output.data()),
        static_cast<int>(comp_size),
        static_cast<int>(decomp_size)
    );
    
    return result == static_cast<int>(decomp_size);
}

std::string NsoFile::getBuildId() const {
    std::string result;
    for (int i = 0; i < 32; i++) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02X", header_.build_id[i]);
        result += buf;
    }
    return result;
}

bool NsoFile::readMemory(uint64_t vaddr, void* buf, size_t size) const {
    // Adjust for base address
    uint64_t offset = vaddr - base_address_;
    
    // Find segment
    const Segment* seg = nullptr;
    uint64_t seg_offset = 0;
    
    if (offset >= text_.mem_offset && offset < text_.mem_offset + text_.size) {
        seg = &text_;
        seg_offset = offset - text_.mem_offset;
    } else if (offset >= rodata_.mem_offset && offset < rodata_.mem_offset + rodata_.size) {
        seg = &rodata_;
        seg_offset = offset - rodata_.mem_offset;
    } else if (offset >= data_.mem_offset && offset < data_.mem_offset + data_.size) {
        seg = &data_;
        seg_offset = offset - data_.mem_offset;
    }
    
    if (!seg || seg_offset + size > seg->data.size()) {
        return false;
    }
    
    std::memcpy(buf, seg->data.data() + seg_offset, size);
    return true;
}

const Segment* NsoFile::getSegmentAt(uint64_t vaddr) const {
    uint64_t offset = vaddr - base_address_;
    
    if (offset >= text_.mem_offset && offset < text_.mem_offset + text_.size) {
        return &text_;
    } else if (offset >= rodata_.mem_offset && offset < rodata_.mem_offset + rodata_.size) {
        return &rodata_;
    } else if (offset >= data_.mem_offset && offset < data_.mem_offset + data_.size) {
        return &data_;
    }
    
    return nullptr;
}

size_t NsoFile::getTotalSize() const {
    return text_.size + rodata_.size + data_.size + header_.bss_size;
}

} // namespace kiloader

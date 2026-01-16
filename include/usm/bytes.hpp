#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace usm {

    using Bytes = std::vector<uint8_t>;

    inline void require_size(const Bytes& b, size_t off, size_t n) {
        if (off + n > b.size()) {
            throw std::runtime_error("Buffer underrun");
        }
    }

    inline uint16_t read_be_u16(const Bytes& b, size_t off) {
        require_size(b, off, 2);
        return (uint16_t(b[off]) << 8) | uint16_t(b[off + 1]);
    }

    inline int16_t read_be_i16(const Bytes& b, size_t off) {
        return int16_t(read_be_u16(b, off));
    }

    inline uint32_t read_be_u32(const Bytes& b, size_t off) {
        require_size(b, off, 4);
        return (uint32_t(b[off]) << 24) | (uint32_t(b[off + 1]) << 16) |
            (uint32_t(b[off + 2]) << 8) | uint32_t(b[off + 3]);
    }

    inline int32_t read_be_i32(const Bytes& b, size_t off) {
        return int32_t(read_be_u32(b, off));
    }

    inline uint64_t read_be_u64(const Bytes& b, size_t off) {
        require_size(b, off, 8);
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) {
            v = (v << 8) | uint64_t(b[off + i]);
        }
        return v;
    }

    inline int64_t read_be_i64(const Bytes& b, size_t off) {
        return int64_t(read_be_u64(b, off));
    }

    inline void write_be_u16(Bytes& out, uint16_t v) {
        out.push_back(uint8_t((v >> 8) & 0xFF));
        out.push_back(uint8_t(v & 0xFF));
    }

    inline void write_be_u32(Bytes& out, uint32_t v) {
        out.push_back(uint8_t((v >> 24) & 0xFF));
        out.push_back(uint8_t((v >> 16) & 0xFF));
        out.push_back(uint8_t((v >> 8) & 0xFF));
        out.push_back(uint8_t(v & 0xFF));
    }

    inline void write_be_u64(Bytes& out, uint64_t v) {
        for (int i = 7; i >= 0; i--) {
            out.push_back(uint8_t((v >> (i * 8)) & 0xFF));
        }
    }

    inline Bytes slice(const Bytes& b, size_t off, size_t end) {
        if (end < off) {
            throw std::runtime_error("Invalid slice");
        }
        require_size(b, off, end - off);
        return Bytes(b.begin() + off, b.begin() + end);
    }

}  // namespace usm
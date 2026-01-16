#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace usm {

    constexpr uint32_t fourcc(char a, char b, char c, char d) {
        return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16) |
            (uint32_t(uint8_t(c)) << 8) | uint32_t(uint8_t(d));
    }

    enum class ChunkType : uint32_t {
        INFO = fourcc('C', 'R', 'I', 'D'),
        VIDEO = fourcc('@', 'S', 'F', 'V'),
        AUDIO = fourcc('@', 'S', 'F', 'A'),
        ALPHA = fourcc('@', 'A', 'L', 'P'),
        SUBTITLE = fourcc('@', 'S', 'B', 'T'),
        CUE = fourcc('@', 'C', 'U', 'E'),

        // Rare chunk types
        SFSH = fourcc('S', 'F', 'S', 'H'),
        AHX = fourcc('@', 'A', 'H', 'X'),
        USR = fourcc('@', 'U', 'S', 'R'),
        PST = fourcc('@', 'P', 'S', 'T'),
    };

    enum class PayloadType : uint8_t {
        STREAM = 0,
        HEADER = 1,
        SECTION_END = 2,
        METADATA = 3,
    };

    enum class ElementOccurrence : uint8_t {
        RECURRING = 1,
        NON_RECURRING = 2,
    };

    enum class ElementType : uint8_t {
        I8 = 0x10,
        U8 = 0x11,
        I16 = 0x12,
        U16 = 0x13,
        I32 = 0x14,
        U32 = 0x15,
        I64 = 0x16,
        U64 = 0x17,
        F32 = 0x18,
        F64 = 0x19,
        STRING = 0x1A,
        BYTES = 0x1B,
    };

    enum class OpMode : uint8_t { NONE, ENCRYPT, DECRYPT };

    inline std::string fourcc_to_string(uint32_t v) {
        char s[5];
        s[0] = char((v >> 24) & 0xFF);
        s[1] = char((v >> 16) & 0xFF);
        s[2] = char((v >> 8) & 0xFF);
        s[3] = char(v & 0xFF);
        s[4] = '\0';
        return std::string(s);
    }

    inline ChunkType chunk_type_from_u32(uint32_t v) {
        switch (v) {
        case uint32_t(ChunkType::INFO):
            return ChunkType::INFO;
        case uint32_t(ChunkType::VIDEO):
            return ChunkType::VIDEO;
        case uint32_t(ChunkType::AUDIO):
            return ChunkType::AUDIO;
        case uint32_t(ChunkType::ALPHA):
            return ChunkType::ALPHA;
        case uint32_t(ChunkType::SUBTITLE):
            return ChunkType::SUBTITLE;
        case uint32_t(ChunkType::CUE):
            return ChunkType::CUE;
        case uint32_t(ChunkType::SFSH):
            return ChunkType::SFSH;
        case uint32_t(ChunkType::AHX):
            return ChunkType::AHX;
        case uint32_t(ChunkType::USR):
            return ChunkType::USR;
        case uint32_t(ChunkType::PST):
            return ChunkType::PST;
        default:
            throw std::runtime_error("Unknown chunk signature: " +
                fourcc_to_string(v));
        }
    }

    inline PayloadType payload_type_from_u8(uint8_t v) {
        switch (v) {
        case 0:
            return PayloadType::STREAM;
        case 1:
            return PayloadType::HEADER;
        case 2:
            return PayloadType::SECTION_END;
        case 3:
            return PayloadType::METADATA;
        default:
            throw std::runtime_error("Unknown payload type: " + std::to_string(v));
        }
    }

    inline ElementOccurrence element_occurrence_from_u8(uint8_t v) {
        switch (v) {
        case 1:
            return ElementOccurrence::RECURRING;
        case 2:
            return ElementOccurrence::NON_RECURRING;
        default:
            throw std::runtime_error("Unknown element occurrence: " +
                std::to_string(v));
        }
    }

    inline ElementType element_type_from_u8(uint8_t v) {
        switch (v) {
        case 0x10:
            return ElementType::I8;
        case 0x11:
            return ElementType::U8;
        case 0x12:
            return ElementType::I16;
        case 0x13:
            return ElementType::U16;
        case 0x14:
            return ElementType::I32;
        case 0x15:
            return ElementType::U32;
        case 0x16:
            return ElementType::I64;
        case 0x17:
            return ElementType::U64;
        case 0x18:
            return ElementType::F32;
        case 0x19:
            return ElementType::F64;
        case 0x1A:
            return ElementType::STRING;
        case 0x1B:
            return ElementType::BYTES;
        default:
            throw std::runtime_error("Unknown element type: " + std::to_string(v));
        }
    }

}  // namespace usm
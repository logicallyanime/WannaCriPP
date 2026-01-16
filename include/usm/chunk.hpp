// TODO: INSPECT

#pragma once

#include "bytes.hpp"
#include "page.hpp"
#include "types.hpp"

#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace usm {

    class UsmChunk {
    public:
        ChunkType chunk_type;
        PayloadType payload_type;

        std::variant<Bytes, std::vector<UsmPage>> payload;

        int frame_rate = 30;
        int frame_time = 0;

        std::variant<int, std::function<int(int)>> padding = 0;

        int channel_number = 0;

        // For parsed chunks this is payload_begin (0x08 + offset_field).
        // For packed chunks payload begins at 0x20 (offset_field=0x18).
        int payload_offset = 0x18;

        std::string encoding = "UTF-8";

        static UsmChunk from_bytes(const Bytes& chunk,
            const std::string& encoding = "UTF-8");

        Bytes pack() const;

        int computed_padding() const;
        int packed_size() const;
    };

}  // namespace usm
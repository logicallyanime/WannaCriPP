// TODO: INSPECT

#include "usm/chunk.hpp"

#include "usm/tools.hpp"

#include <stdexcept>

namespace usm {

    static Bytes pack_payload_variant(const std::variant<Bytes, std::vector<UsmPage>>& p,
        const std::string& enc) {
        if (std::holds_alternative<Bytes>(p)) {
            return std::get<Bytes>(p);
        }
        return pack_pages(std::get<std::vector<UsmPage>>(p), enc);
    }

    int UsmChunk::computed_padding() const {
        if (std::holds_alternative<int>(padding)) {
            return std::get<int>(padding);
        }

        Bytes payload_bytes = pack_payload_variant(payload, encoding);
        const auto& fn = std::get<std::function<int(int)>>(padding);
        return fn(int(0x20 + payload_bytes.size()));
    }

    int UsmChunk::packed_size() const {
        Bytes payload_bytes = pack_payload_variant(payload, encoding);
        int pad = computed_padding();
        return int(0x20 + payload_bytes.size() + pad);
    }

    UsmChunk UsmChunk::from_bytes(const Bytes& chunk, const std::string& enc) {
        if (chunk.size() < 0x20) {
            throw std::runtime_error("Chunk too small");
        }

        uint32_t sig = read_be_u32(chunk, 0);
        ChunkType ct = chunk_type_from_u32(sig);

        uint32_t chunksize_field = read_be_u32(chunk, 0x4);
        uint8_t payload_offset_field = chunk[0x9];
        uint16_t padding_size = read_be_u16(chunk, 0xA);
        uint8_t chno = chunk[0xC];

        uint32_t frame_time = read_be_u32(chunk, 0x10);
        uint32_t frame_rate = read_be_u32(chunk, 0x14);

        const int payload_begin = int(0x08 + payload_offset_field);
        const int payload_size =
            int(chunksize_field) - int(padding_size) - int(payload_offset_field);

        if (payload_begin < 0 || payload_begin > int(chunk.size())) {
            throw std::runtime_error("Bad payload begin");
        }
        if (payload_size < 0) {
            throw std::runtime_error("Bad payload size");
        }
        if (payload_begin + payload_size > int(chunk.size())) {
            throw std::runtime_error("Chunk buffer missing payload bytes");
        }

        Bytes payload_raw = slice(chunk, payload_begin, payload_begin + payload_size);

        uint8_t payload_type_bits = uint8_t(chunk[0xF] & 0x3);
        PayloadType pt = payload_type_from_u8(payload_type_bits);

        std::variant<Bytes, std::vector<UsmPage>> payload_variant;
        if (is_payload_list_pages(payload_raw)) {
            payload_variant = get_pages(payload_raw, enc);
        }
        else {
            payload_variant = payload_raw;
        }

        UsmChunk out;
        out.chunk_type = ct;
        out.payload_type = pt;
        out.payload = std::move(payload_variant);
        out.frame_rate = int(frame_rate);
        out.frame_time = int(frame_time);
        out.padding = int(padding_size);
        out.channel_number = int(chno);
        out.payload_offset = payload_begin;
        out.encoding = enc;

        return out;
    }

    Bytes UsmChunk::pack() const {
        Bytes result;
        write_be_u32(result, uint32_t(chunk_type));

        Bytes payload_bytes = pack_payload_variant(payload, encoding);
        int pad = computed_padding();

        // Matches Python: chunksize field is 0x18 + payload + padding.
        uint32_t chunksize_field = uint32_t(0x18 + payload_bytes.size() + pad);
        write_be_u32(result, chunksize_field);

        // r08
        result.push_back(0x00);

        // payload offset field is always 0x18 in Python pack()
        result.push_back(0x18);

        write_be_u16(result, uint16_t(pad));

        result.push_back(uint8_t(channel_number & 0xFF));

        // r0D-r0E
        result.push_back(0x00);
        result.push_back(0x00);

        result.push_back(uint8_t(payload_type));

        write_be_u32(result, uint32_t(frame_time));
        write_be_u32(result, uint32_t(frame_rate));

        // r18-r1F
        result.insert(result.end(), 8, 0x00);

        // payload
        result.insert(result.end(), payload_bytes.begin(), payload_bytes.end());

        // padding bytes
        result.insert(result.end(), size_t(pad), 0x00);

        return result;
    }

}  // namespace usm
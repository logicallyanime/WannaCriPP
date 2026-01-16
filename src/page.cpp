// TODO: INSPECT
#include "usm/page.hpp"

#include "usm/bytes.hpp"
#include "usm/tools.hpp"
#include "usm/types.hpp"

#include <cstring>
#include <stdexcept>

namespace usm {

    UsmPage::UsmPage(std::string name) : name_(std::move(name)) {}

    const std::string& UsmPage::name() const { return name_; }

    const std::vector<std::string>& UsmPage::key_order() const { return order_; }

    const std::unordered_map<std::string, Element>& UsmPage::dict() const {
        return dict_;
    }

    void UsmPage::update(const std::string& key, ElementType type,
        ElementValue value) {
        // Match Python behavior: filename backslashes -> slashes.
        if (key == "filename") {
            if (auto p = std::get_if<std::string>(&value)) {
                std::string s = *p;
                for (auto& ch : s) {
                    if (ch == '\\') ch = '/';
                }
                value = s;
            }
        }

        if (dict_.find(key) == dict_.end()) {
            order_.push_back(key);
        }
        dict_[key] = Element{ type, std::move(value) };
    }

    std::optional<Element> UsmPage::get(const std::string& key) const {
        auto it = dict_.find(key);
        if (it == dict_.end()) return std::nullopt;
        return it->second;
    }

    const Element& UsmPage::at(const std::string& key) const {
        auto it = dict_.find(key);
        if (it == dict_.end()) throw std::runtime_error("Missing key: " + key);
        return it->second;
    }

    static std::string read_cstring(const Bytes& b, size_t off) {
        if (off >= b.size()) throw std::runtime_error("Bad string offset");
        size_t end = off;
        while (end < b.size() && b[end] != 0x00) end++;
        if (end >= b.size()) throw std::runtime_error("Unterminated string");
        return std::string(reinterpret_cast<const char*>(b.data() + off), end - off);
    }

    static float read_le_f32(const uint8_t* p) {
        uint32_t u = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
            (uint32_t(p[3]) << 24);
        float f;
        static_assert(sizeof(float) == 4);
        std::memcpy(&f, &u, 4);
        return f;
    }

    static void write_le_f32(Bytes& out, float f) {
        uint32_t u;
        static_assert(sizeof(float) == 4);
        std::memcpy(&u, &f, 4);
        out.push_back(uint8_t(u & 0xFF));
        out.push_back(uint8_t((u >> 8) & 0xFF));
        out.push_back(uint8_t((u >> 16) & 0xFF));
        out.push_back(uint8_t((u >> 24) & 0xFF));
    }

    std::vector<UsmPage> get_pages(const Bytes& info, const std::string&) {
        if (info.size() < 8) throw std::runtime_error("Invalid @UTF payload");

        if (!(info[0] == '@' && info[1] == 'U' && info[2] == 'T' && info[3] == 'F')) {
            throw std::runtime_error("Invalid info data signature");
        }

        const uint32_t payload_size = read_be_u32(info, 4);

        const uint32_t unique_array_offset = read_be_u32(info, 8);
        const uint32_t strings_offset = read_be_u32(info, 12);
        const uint32_t byte_array_offset = read_be_u32(info, 16);
        const uint32_t page_name_offset = read_be_u32(info, 20);

        const uint16_t num_elements_per_page = read_be_u16(info, 24);
        const uint16_t unique_array_size_per_page = read_be_u16(info, 26);
        const uint32_t num_pages = read_be_u32(info, 28);

        // Offsets are after the 8-byte header.
        Bytes string_array =
            slice(info, 8 + strings_offset, 8 + byte_array_offset);
        Bytes byte_array = slice(info, 8 + byte_array_offset, 8 + payload_size);

        std::string page_name = read_cstring(string_array, page_name_offset);
        std::vector<UsmPage> pages;
        pages.reserve(num_pages);
        for (uint32_t i = 0; i < num_pages; i++) {
            pages.emplace_back(page_name);
        }

        Bytes unique_array = slice(
            info, 8 + unique_array_offset,
            8 + unique_array_offset + size_t(unique_array_size_per_page) * num_pages);

        // shared_array = info[0x20 : 8 + unique_array_offset]
        Bytes shared_array_master = slice(info, 0x20, 8 + unique_array_offset);

        size_t unique_pos = 0;
        for (uint32_t p = 0; p < num_pages; p++) {
            size_t shared_pos = 0;

            for (uint16_t e = 0; e < num_elements_per_page; e++) {
                if (shared_pos + 5 > shared_array_master.size()) {
                    throw std::runtime_error("Bad shared array bounds");
                }

                uint8_t packed = shared_array_master[shared_pos];
                ElementType et = element_type_from_u8(packed & 0x1F);
                ElementOccurrence occ = element_occurrence_from_u8(packed >> 5);
                uint32_t name_off =
                    read_be_u32(shared_array_master, shared_pos + 1);
                shared_pos += 5;

                std::string element_name = read_cstring(string_array, name_off);
                auto& page = pages[p];

                auto read_bytes = [&](const Bytes& src, size_t pos, size_t n) -> Bytes {
                    if (pos + n > src.size()) throw std::runtime_error("Bad element bounds");
                    return Bytes(src.begin() + pos, src.begin() + pos + n);
                    };

                if (occ == ElementOccurrence::RECURRING) {
                    if (et == ElementType::I8) {
                        page.update(element_name, et, int8_t(shared_array_master[shared_pos]));
                        shared_pos += 1;
                    }
                    else if (et == ElementType::U8) {
                        page.update(element_name, et, uint8_t(shared_array_master[shared_pos]));
                        shared_pos += 1;
                    }
                    else if (et == ElementType::I16) {
                        page.update(element_name, et,
                            int16_t(read_be_i16(shared_array_master, shared_pos)));
                        shared_pos += 2;
                    }
                    else if (et == ElementType::U16) {
                        page.update(element_name, et,
                            uint16_t(read_be_u16(shared_array_master, shared_pos)));
                        shared_pos += 2;
                    }
                    else if (et == ElementType::I32) {
                        page.update(element_name, et,
                            int32_t(read_be_i32(shared_array_master, shared_pos)));
                        shared_pos += 4;
                    }
                    else if (et == ElementType::U32) {
                        page.update(element_name, et,
                            uint32_t(read_be_u32(shared_array_master, shared_pos)));
                        shared_pos += 4;
                    }
                    else if (et == ElementType::I64) {
                        page.update(element_name, et,
                            int64_t(read_be_i64(shared_array_master, shared_pos)));
                        shared_pos += 8;
                    }
                    else if (et == ElementType::U64) {
                        page.update(element_name, et,
                            uint64_t(read_be_u64(shared_array_master, shared_pos)));
                        shared_pos += 8;
                    }
                    else if (et == ElementType::F32) {
                        Bytes raw = read_bytes(shared_array_master, shared_pos, 4);
                        page.update(element_name, et, read_le_f32(raw.data()));
                        shared_pos += 4;
                    }
                    else if (et == ElementType::STRING) {
                        uint32_t str_off = read_be_u32(shared_array_master, shared_pos);
                        page.update(element_name, et, read_cstring(string_array, str_off));
                        shared_pos += 4;
                    }
                    else if (et == ElementType::BYTES) {
                        uint32_t data_off = read_be_u32(shared_array_master, shared_pos);
                        uint32_t data_end = read_be_u32(shared_array_master, shared_pos + 4);
                        if (data_end < data_off || data_end > byte_array.size()) {
                            throw std::runtime_error("Bad bytes element bounds");
                        }
                        page.update(element_name, et, slice(byte_array, data_off, data_end));
                        shared_pos += 8;
                    }
                    else {
                        throw std::runtime_error("Unsupported element type");
                    }
                }
                else {
                    // NON_RECURRING: values come from unique_array sequentially.
                    if (et == ElementType::I8) {
                        page.update(element_name, et, int8_t(unique_array[unique_pos]));
                        unique_pos += 1;
                    }
                    else if (et == ElementType::U8) {
                        page.update(element_name, et, uint8_t(unique_array[unique_pos]));
                        unique_pos += 1;
                    }
                    else if (et == ElementType::I16) {
                        page.update(element_name, et,
                            int16_t(read_be_i16(unique_array, unique_pos)));
                        unique_pos += 2;
                    }
                    else if (et == ElementType::U16) {
                        page.update(element_name, et,
                            uint16_t(read_be_u16(unique_array, unique_pos)));
                        unique_pos += 2;
                    }
                    else if (et == ElementType::I32) {
                        page.update(element_name, et,
                            int32_t(read_be_i32(unique_array, unique_pos)));
                        unique_pos += 4;
                    }
                    else if (et == ElementType::U32) {
                        page.update(element_name, et,
                            uint32_t(read_be_u32(unique_array, unique_pos)));
                        unique_pos += 4;
                    }
                    else if (et == ElementType::I64) {
                        page.update(element_name, et,
                            int64_t(read_be_i64(unique_array, unique_pos)));
                        unique_pos += 8;
                    }
                    else if (et == ElementType::U64) {
                        page.update(element_name, et,
                            uint64_t(read_be_u64(unique_array, unique_pos)));
                        unique_pos += 8;
                    }
                    else if (et == ElementType::F32) {
                        Bytes raw = read_bytes(unique_array, unique_pos, 4);
                        page.update(element_name, et, read_le_f32(raw.data()));
                        unique_pos += 4;
                    }
                    else if (et == ElementType::STRING) {
                        uint32_t str_off = read_be_u32(unique_array, unique_pos);
                        page.update(element_name, et, read_cstring(string_array, str_off));
                        unique_pos += 4;
                    }
                    else if (et == ElementType::BYTES) {
                        uint32_t data_off = read_be_u32(unique_array, unique_pos);
                        uint32_t data_end = read_be_u32(unique_array, unique_pos + 4);
                        if (data_end < data_off || data_end > byte_array.size()) {
                            throw std::runtime_error("Bad bytes element bounds");
                        }
                        page.update(element_name, et, slice(byte_array, data_off, data_end));
                        unique_pos += 8;
                    }
                    else {
                        throw std::runtime_error("Unsupported element type");
                    }
                }
            }
        }

        return pages;
    }

    static bool element_equal(const Element& a, const Element& b) {
        if (a.type != b.type) return false;
        return a.val == b.val;
    }

    Bytes pack_pages(const std::vector<UsmPage>& pages, const std::string&,
        int string_padding) {
        if (pages.empty()) return Bytes();

        const std::string page_name = pages[0].name();
        const auto& order = pages[0].key_order();

        for (const auto& p : pages) {
            if (p.name() != page_name) throw std::runtime_error("Pages name mismatch");
            if (p.key_order().size() != order.size()) {
                throw std::runtime_error("Pages keys mismatch");
            }
            for (size_t i = 0; i < order.size(); i++) {
                if (p.key_order()[i] != order[i]) {
                    throw std::runtime_error("Pages key order mismatch");
                }
            }
        }

        Bytes string_array;
        {
            const char* nulls = "<NULL>";
            string_array.insert(string_array.end(), nulls, nulls + std::strlen(nulls));
            string_array.push_back(0x00);
        }

        const uint32_t page_name_offset = uint32_t(string_array.size());
        string_array.insert(string_array.end(), page_name.begin(), page_name.end());
        string_array.push_back(0x00);

        // Map key -> name offset in string_array (by key order).
        std::vector<uint32_t> name_offsets(order.size());
        for (size_t i = 0; i < order.size(); i++) {
            name_offsets[i] = uint32_t(string_array.size());
            const auto& key = order[i];
            string_array.insert(string_array.end(), key.begin(), key.end());
            string_array.push_back(0x00);
        }

        // Determine recurring keys (common elements).
        std::vector<bool> recurring(order.size(), false);
        if (pages.size() > 1) {
            for (size_t i = 0; i < order.size(); i++) {
                const std::string& k = order[i];
                const Element& first = pages[0].at(k);
                bool all_same = true;
                for (size_t p = 1; p < pages.size(); p++) {
                    if (!element_equal(first, pages[p].at(k))) {
                        all_same = false;
                        break;
                    }
                }
                recurring[i] = all_same;
            }
        }

        Bytes shared_array;
        Bytes unique_array;
        Bytes byte_array;

        for (size_t pi = 0; pi < pages.size(); pi++) {
            const auto& page = pages[pi];

            for (size_t ki = 0; ki < order.size(); ki++) {
                const std::string& key = order[ki];
                const Element& el = page.at(key);

                uint8_t type_packed = uint8_t(el.type);

                if (recurring[ki]) {
                    if (pi != 0) continue;

                    type_packed |= uint8_t(uint8_t(ElementOccurrence::RECURRING) << 5);
                    shared_array.push_back(type_packed);
                    write_be_u32(shared_array, name_offsets[ki]);

                    Bytes& cur = shared_array;

                    if (el.type == ElementType::I8) {
                        cur.push_back(uint8_t(std::get<int8_t>(el.val)));
                    }
                    else if (el.type == ElementType::U8) {
                        cur.push_back(std::get<uint8_t>(el.val));
                    }
                    else if (el.type == ElementType::I16) {
                        write_be_u16(cur, uint16_t(std::get<int16_t>(el.val)));
                    }
                    else if (el.type == ElementType::U16) {
                        write_be_u16(cur, std::get<uint16_t>(el.val));
                    }
                    else if (el.type == ElementType::I32) {
                        write_be_u32(cur, uint32_t(std::get<int32_t>(el.val)));
                    }
                    else if (el.type == ElementType::U32) {
                        write_be_u32(cur, std::get<uint32_t>(el.val));
                    }
                    else if (el.type == ElementType::I64) {
                        write_be_u64(cur, uint64_t(std::get<int64_t>(el.val)));
                    }
                    else if (el.type == ElementType::U64) {
                        write_be_u64(cur, std::get<uint64_t>(el.val));
                    }
                    else if (el.type == ElementType::F32) {
                        write_le_f32(cur, std::get<float>(el.val));
                    }
                    else if (el.type == ElementType::STRING) {
                        const std::string& s = std::get<std::string>(el.val);
                        uint32_t off = uint32_t(string_array.size());
                        string_array.insert(string_array.end(), s.begin(), s.end());
                        string_array.push_back(0x00);
                        write_be_u32(cur, off);
                    }
                    else if (el.type == ElementType::BYTES) {
                        const Bytes& bb = std::get<Bytes>(el.val);
                        uint32_t off = uint32_t(byte_array.size());
                        uint32_t end = off + uint32_t(bb.size());
                        write_be_u32(cur, off);
                        write_be_u32(cur, end);
                        byte_array.insert(byte_array.end(), bb.begin(), bb.end());
                    }
                    else {
                        throw std::runtime_error("Unknown element type in pack_pages");
                    }

                }
                else {
                    // NON_RECURRING:
                    // - encode header (type + name offset) once (pi == 0) into shared_array
                    // - encode values each page into unique_array
                    if (pi == 0) {
                        type_packed |=
                            uint8_t(uint8_t(ElementOccurrence::NON_RECURRING) << 5);
                        shared_array.push_back(type_packed);
                        write_be_u32(shared_array, name_offsets[ki]);
                    }

                    Bytes& cur = unique_array;

                    if (el.type == ElementType::I8) {
                        cur.push_back(uint8_t(std::get<int8_t>(el.val)));
                    }
                    else if (el.type == ElementType::U8) {
                        cur.push_back(std::get<uint8_t>(el.val));
                    }
                    else if (el.type == ElementType::I16) {
                        write_be_u16(cur, uint16_t(std::get<int16_t>(el.val)));
                    }
                    else if (el.type == ElementType::U16) {
                        write_be_u16(cur, std::get<uint16_t>(el.val));
                    }
                    else if (el.type == ElementType::I32) {
                        write_be_u32(cur, uint32_t(std::get<int32_t>(el.val)));
                    }
                    else if (el.type == ElementType::U32) {
                        write_be_u32(cur, std::get<uint32_t>(el.val));
                    }
                    else if (el.type == ElementType::I64) {
                        write_be_u64(cur, uint64_t(std::get<int64_t>(el.val)));
                    }
                    else if (el.type == ElementType::U64) {
                        write_be_u64(cur, std::get<uint64_t>(el.val));
                    }
                    else if (el.type == ElementType::F32) {
                        write_le_f32(cur, std::get<float>(el.val));
                    }
                    else if (el.type == ElementType::STRING) {
                        const std::string& s = std::get<std::string>(el.val);
                        uint32_t off = uint32_t(string_array.size());
                        string_array.insert(string_array.end(), s.begin(), s.end());
                        string_array.push_back(0x00);
                        write_be_u32(cur, off);
                    }
                    else if (el.type == ElementType::BYTES) {
                        const Bytes& bb = std::get<Bytes>(el.val);
                        uint32_t off = uint32_t(byte_array.size());
                        uint32_t end = off + uint32_t(bb.size());
                        write_be_u32(cur, off);
                        write_be_u32(cur, end);
                        byte_array.insert(byte_array.end(), bb.begin(), bb.end());
                    }
                    else {
                        throw std::runtime_error("Unknown element type in pack_pages");
                    }
                }
            }
        }

        if (string_padding > 0) {
            string_array.insert(string_array.end(), size_t(string_padding), 0x00);
        }

        // Build final @UTF payload.
        Bytes result;
        result.push_back('@');
        result.push_back('U');
        result.push_back('T');
        result.push_back('F');

        // payload_size excludes the 8-byte header.
        const uint32_t data_size =
            uint32_t(24 + shared_array.size() + unique_array.size() +
                string_array.size() + byte_array.size());
        write_be_u32(result, data_size);

        const uint32_t unique_array_offset = uint32_t(24 + shared_array.size());
        write_be_u32(result, unique_array_offset);

        const uint32_t strings_offset =
            uint32_t(24 + shared_array.size() + unique_array.size());
        write_be_u32(result, strings_offset);

        const uint32_t byte_array_offset =
            uint32_t(24 + shared_array.size() + unique_array.size() +
                string_array.size());
        write_be_u32(result, byte_array_offset);

        write_be_u32(result, page_name_offset);

        write_be_u16(result, uint16_t(order.size()));

        if (pages.empty()) throw std::runtime_error("pack_pages: empty pages");
        if (unique_array.size() % pages.size() != 0) {
            throw std::runtime_error("unique_array not divisible by num pages");
        }
        const uint16_t unique_size_per_page =
            uint16_t(unique_array.size() / pages.size());
        write_be_u16(result, unique_size_per_page);

        write_be_u32(result, uint32_t(pages.size()));

        result.insert(result.end(), shared_array.begin(), shared_array.end());
        result.insert(result.end(), unique_array.begin(), unique_array.end());
        result.insert(result.end(), string_array.begin(), string_array.end());
        result.insert(result.end(), byte_array.begin(), byte_array.end());

        return result;
    }

    std::vector<int> keyframes_from_seek_pages(
        const std::optional<std::vector<UsmPage>>& seek_pages) {
        std::vector<int> result;
        if (!seek_pages.has_value()) return result;

        for (const auto& seek : *seek_pages) {
            if (seek.name() != "VIDEO_SEEKINFO") {
                throw std::runtime_error("Page name is not 'VIDEO_SEEKINFO'");
            }

            const Element& el = seek.at("ofs_frmid");
            if (el.type != ElementType::U32) {
                throw std::runtime_error("ofs_frmid is not U32");
            }
            result.push_back(int(std::get<uint32_t>(el.val)));
        }

        return result;
    }

}  // namespace usm
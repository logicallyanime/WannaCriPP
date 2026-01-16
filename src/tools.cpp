// TODO: INSPECT

#include "usm/tools.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <unicode/normalizer2.h>
#include <unicode/unistr.h>
#include <unicode/uchar.h>

namespace usm {

    std::string bytes_to_hex(const Bytes& data) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < data.size(); i++) {
            oss << std::setw(2) << int(data[i]);
            if (i + 1 != data.size()) oss << " ";
        }
        return oss.str();
    }

    bool is_usm_magic(const Bytes& magic4) {
        if (magic4.size() < 4) return false;
        return magic4[0] == 'C' && magic4[1] == 'R' && magic4[2] == 'I' &&
            magic4[3] == 'D';
    }

    bool is_payload_list_pages(const Bytes& payload) {
        if (payload.size() < 4) return false;
        return payload[0] == '@' && payload[1] == 'U' && payload[2] == 'T' &&
            payload[3] == 'F';
    }

    std::pair<int, int> chunk_size_and_padding(const Bytes& header20) {
        if (header20.size() < 0x20) {
            throw std::runtime_error("chunk_size_and_padding requires 0x20 bytes");
        }

        int size = int(read_be_u32(header20, 4));
        int offset = int(header20[9]);
        int padding_size = int(read_be_u16(header20, 10));
        size -= offset + padding_size;
        if (size < 0) {
            throw std::runtime_error("Negative size");
        }
        return { size, padding_size };
    }

    std::pair<Bytes, Bytes> generate_keys(uint64_t key_num) {
        // Mirrors your Python generate_keys() exactly (little-endian key_num).
        uint8_t cipher_key[8];
        for (int i = 0; i < 8; i++) {
            cipher_key[i] = uint8_t((key_num >> (i * 8)) & 0xFF);
        }

        Bytes key(0x20);
        key[0x00] = cipher_key[0];
        key[0x01] = cipher_key[1];
        key[0x02] = cipher_key[2];
        key[0x03] = uint8_t((cipher_key[3] - 0x34) & 0xFF);
        key[0x04] = uint8_t((cipher_key[4] + 0xF9) & 0xFF);
        key[0x05] = uint8_t(cipher_key[5] ^ 0x13);
        key[0x06] = uint8_t((cipher_key[6] + 0x61) & 0xFF);
        key[0x07] = uint8_t(key[0x00] ^ 0xFF);
        key[0x08] = uint8_t((key[0x01] + key[0x02]) & 0xFF);
        key[0x09] = uint8_t((key[0x01] - key[0x07]) & 0xFF);
        key[0x0A] = uint8_t(key[0x02] ^ 0xFF);
        key[0x0B] = uint8_t(key[0x01] ^ 0xFF);
        key[0x0C] = uint8_t((key[0x0B] + key[0x09]) & 0xFF);
        key[0x0D] = uint8_t((key[0x08] - key[0x03]) & 0xFF);
        key[0x0E] = uint8_t(key[0x0D] ^ 0xFF);
        key[0x0F] = uint8_t((key[0x0A] - key[0x0B]) & 0xFF);
        key[0x10] = uint8_t((key[0x08] - key[0x0F]) & 0xFF);
        key[0x11] = uint8_t(key[0x10] ^ key[0x07]);
        key[0x12] = uint8_t(key[0x0F] ^ 0xFF);
        key[0x13] = uint8_t(key[0x03] ^ 0x10);
        key[0x14] = uint8_t((key[0x04] - 0x32) & 0xFF);
        key[0x15] = uint8_t((key[0x05] + 0xED) & 0xFF);
        key[0x16] = uint8_t(key[0x06] ^ 0xF3);
        key[0x17] = uint8_t((key[0x13] - key[0x0F]) & 0xFF);
        key[0x18] = uint8_t((key[0x15] + key[0x07]) & 0xFF);
        key[0x19] = uint8_t((0x21 - key[0x13]) & 0xFF);
        key[0x1A] = uint8_t(key[0x14] ^ key[0x17]);
        key[0x1B] = uint8_t((key[0x16] + key[0x16]) & 0xFF);
        key[0x1C] = uint8_t((key[0x17] + 0x44) & 0xFF);
        key[0x1D] = uint8_t((key[0x03] + key[0x04]) & 0xFF);
        key[0x1E] = uint8_t((key[0x05] - key[0x16]) & 0xFF);
        key[0x1F] = uint8_t(key[0x1D] ^ key[0x13]);

        const uint8_t audio_t[4] = { 'U', 'R', 'U', 'C' };

        Bytes video_key(0x40);
        Bytes audio_key(0x20);

        for (int i = 0; i < 0x20; i++) {
            video_key[i] = key[i];
            video_key[0x20 + i] = uint8_t(key[i] ^ 0xFF);
            audio_key[i] =
                (i % 2 != 0) ? audio_t[(i >> 1) % 4] : uint8_t(key[i] ^ 0xFF);
        }

        return { video_key, audio_key };
    }

    Bytes decrypt_video_packet(const Bytes& packet, const Bytes& video_key) {
        if (video_key.size() < 0x40) {
            throw std::runtime_error("Video key should be 0x40 bytes");
        }

        Bytes data = packet;
        int encrypted_part_size = int(data.size()) - 0x40;
        if (encrypted_part_size >= 0x200) {
            Bytes rolling = video_key;

            for (int i = 0x100; i < encrypted_part_size; i++) {
                data[0x40 + i] ^= rolling[0x20 + (i % 0x20)];
                rolling[0x20 + (i % 0x20)] =
                    uint8_t(data[0x40 + i] ^ video_key[0x20 + (i % 0x20)]);
            }

            for (int i = 0; i < 0x100; i++) {
                rolling[i % 0x20] ^= data[0x140 + i];
                data[0x40 + i] ^= rolling[i % 0x20];
            }
        }

        return data;
    }

    Bytes encrypt_video_packet(const Bytes& packet, const Bytes& video_key) {
        if (video_key.size() < 0x40) {
            throw std::runtime_error("Video key should be 0x40 bytes");
        }

        Bytes data = packet;
        if (data.size() >= 0x240) {
            int encrypted_part_size = int(data.size()) - 0x40;
            Bytes rolling = video_key;

            for (int i = 0; i < 0x100; i++) {
                rolling[i % 0x20] ^= data[0x140 + i];
                data[0x40 + i] ^= rolling[i % 0x20];
            }

            for (int i = 0x100; i < encrypted_part_size; i++) {
                uint8_t plainbyte = data[0x40 + i];
                data[0x40 + i] ^= rolling[0x20 + (i % 0x20)];
                rolling[0x20 + (i % 0x20)] =
                    uint8_t(plainbyte ^ video_key[0x20 + (i % 0x20)]);
            }
        }

        return data;
    }

    Bytes crypt_audio_packet(const Bytes& packet, const Bytes& audio_key) {
        if (audio_key.size() < 0x20) {
            throw std::runtime_error("Audio key should be 0x20 bytes");
        }

        Bytes data = packet;
        if (data.size() > 0x140) {
            for (size_t i = 0x140; i < data.size(); i++) {
                data[i] ^= audio_key[i % 0x20];
            }
        }
        return data;
    }

    static std::string icu_to_utf8(const icu::UnicodeString& u) {
        std::string out;
        u.toUTF8String(out);
        return out;
    }

    std::string slugify_utf8(const std::string& s, bool allow_unicode) {
        UErrorCode status = U_ZERO_ERROR;

        const icu::Normalizer2* norm = nullptr;
        if (allow_unicode) {
            norm = icu::Normalizer2::getNFKCInstance(status);
        }
        else {
            norm = icu::Normalizer2::getNFKDInstance(status);
        }
        if (U_FAILURE(status) || norm == nullptr) {
            throw std::runtime_error("ICU normalizer init failed");
        }

        icu::UnicodeString u = icu::UnicodeString::fromUTF8(s);
        icu::UnicodeString normalized = norm->normalize(u, status);
        if (U_FAILURE(status)) {
            throw std::runtime_error("ICU normalize failed");
        }

        normalized.toLower();

        // Keep: unicode \w-ish (letters/digits/_), whitespace, and . , + -
        icu::UnicodeString filtered;

        for (int32_t i = 0; i < normalized.length();) {
            UChar32 c = normalized.char32At(i);
            i += U16_LENGTH(c);

            bool keep = false;
            if (c == '_' || c == '.' || c == ',' || c == '+' || c == '-') {
                keep = true;
            }
            else if (u_isUWhiteSpace(c)) {
                keep = true;
            }
            else if (u_isalnum(c)) {
                keep = true;
            }

            if (!allow_unicode) {
                // Drop non-ascii in ascii mode.
                if (c > 0x7F) keep = false;
            }

            if (keep) {
                filtered.append(c);
            }
        }

        // Collapse runs of whitespace or '-' into single '-'
        icu::UnicodeString collapsed;

        bool in_sep = false;
        for (int32_t i = 0; i < filtered.length();) {
            UChar32 c = filtered.char32At(i);
            i += U16_LENGTH(c);

            const bool sep = u_isUWhiteSpace(c) || c == '-';
            if (sep) {
                if (!in_sep) {
                    collapsed.append('-');
                    in_sep = true;
                }
                continue;
            }
            in_sep = false;
            collapsed.append(c);
        }

        // Strip leading/trailing '-' and '_'
        auto is_strip = [](UChar32 c) { return c == '-' || c == '_'; };

        int32_t start = 0;
        int32_t end = collapsed.length();

        while (start < end) {
            UChar32 c = collapsed.char32At(start);
            if (!is_strip(c)) break;
            start =collapsed.moveIndex32(start, 1);
        }

        while (end > start) {
            int32_t prev = collapsed.moveIndex32(end, -1);
            UChar32 c = collapsed.char32At(prev);
            if (!is_strip(c)) break;
            end = prev;
        }

        icu::UnicodeString trimmed = collapsed.tempSubStringBetween(start, end);
        return icu_to_utf8(trimmed);
    }

    std::string basename_utf8(const std::string& path_like) {
        // Handle both / and \ (your Python normalizes filenames later anyway).
        size_t p1 = path_like.find_last_of('/');
        size_t p2 = path_like.find_last_of('\\');
        size_t p = std::string::npos;
        if (p1 == std::string::npos) {
            p = p2;
        }
        else if (p2 == std::string::npos) {
            p = p1;
        }
        else {
            p = std::max(p1, p2);
        }
        if (p == std::string::npos) return path_like;
        return path_like.substr(p + 1);
    }

}  // namespace usm
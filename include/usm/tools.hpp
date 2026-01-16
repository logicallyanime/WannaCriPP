#pragma once

#include "bytes.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace usm {

	std::string bytes_to_hex(const Bytes& data);

	bool is_usm_magic(const Bytes& magic4);

	bool is_payload_list_pages(const Bytes& payload);

	std::pair<int, int> chunk_size_and_padding(const Bytes& header20);

	std::pair<Bytes, Bytes> generate_keys(uint64_t key_num);

	Bytes decrypt_video_packet(const Bytes& packet, const Bytes& video_key);
	Bytes encrypt_video_packet(const Bytes& packet, const Bytes& video_key);

	Bytes crypt_audio_packet(const Bytes& packet, const Bytes& audio_key);

	std::string slugify_utf8(const std::string& s, bool allow_unicode = true);

	std::string basename_utf8(const std::string& path_like);

}  // namespace usm
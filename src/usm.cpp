// TODO: INSPECT

#include "usm/usm.hpp"

#include "usm/chunk.hpp"
#include "usm/tools.hpp"
#include "usm/types.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace usm {

    static int16_t get_i16(const UsmPage& p, const std::string& k) {
        const Element& e = p.at(k);
        if (e.type != ElementType::I16) {
            throw std::runtime_error(k + " is not I16");
        }
        return std::get<int16_t>(e.val);
    }

    static int32_t get_i32(const UsmPage& p, const std::string& k) {
        const Element& e = p.at(k);
        if (e.type != ElementType::I32) {
            throw std::runtime_error(k + " is not I32");
        }
        return std::get<int32_t>(e.val);
    }

    static std::string get_str(const UsmPage& p, const std::string& k) {
        const Element& e = p.at(k);
        if (e.type != ElementType::STRING) {
            throw std::runtime_error(k + " is not STRING");
        }
        return std::get<std::string>(e.val);
    }

    struct ChannelAccum {
        std::vector<std::pair<uint64_t, uint32_t>> stream;
        UsmPage header{ "" };
        std::optional<std::vector<UsmPage>> metadata;
    };

    static void chunk_helper(std::unordered_map<int, ChannelAccum>& dst,
        const UsmChunk& c, uint64_t chunk_file_offset) {
        auto& ch = dst[c.channel_number];

        if (c.payload_type == PayloadType::STREAM) {
            if (!std::holds_alternative<Bytes>(c.payload)) {
                throw std::runtime_error("STREAM payload unexpectedly pages");
            }
            const auto& payload = std::get<Bytes>(c.payload);
            ch.stream.push_back(
                { chunk_file_offset + uint64_t(c.payload_offset), uint32_t(payload.size()) });
        }
        else if (c.payload_type == PayloadType::HEADER) {
            if (!std::holds_alternative<std::vector<UsmPage>>(c.payload)) {
                throw std::runtime_error("HEADER payload is not pages");
            }
            const auto& pages = std::get<std::vector<UsmPage>>(c.payload);
            if (pages.empty()) throw std::runtime_error("Empty HEADER pages");
            ch.header = pages[0];
        }
        else if (c.payload_type == PayloadType::METADATA) {
            if (!std::holds_alternative<std::vector<UsmPage>>(c.payload)) {
                throw std::runtime_error("METADATA payload is not pages");
            }
            ch.metadata = std::get<std::vector<UsmPage>>(c.payload);
        }
        else if (c.payload_type == PayloadType::SECTION_END) {
            // ignore
        }
        else {
            throw std::runtime_error("Unknown payload type");
        }
    }

    Usm Usm::open(const std::filesystem::path& path, std::optional<uint64_t> key,
        const std::string& encoding) {
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("File not found");
        }

        uint64_t filesize = std::filesystem::file_size(path);
        if (filesize <= 0x20) throw std::runtime_error("File too small");

        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("Failed to open file");

        Bytes magic(4);
        f.read(reinterpret_cast<char*>(magic.data()), 4);
        if (!f) throw std::runtime_error("Failed to read magic");
        if (!is_usm_magic(magic)) {
            throw std::runtime_error("Invalid file signature: " + bytes_to_hex(magic));
        }

        std::vector<UsmPage> crids;
        std::unordered_map<int, ChannelAccum> video_ch;
        std::unordered_map<int, ChannelAccum> audio_ch;
        std::unordered_map<int, ChannelAccum> alpha_ch;

        f.seekg(0, std::ios::beg);

        while (uint64_t(f.tellg()) < filesize) {
            uint64_t offset = uint64_t(f.tellg());

            Bytes header(0x20);
            f.read(reinterpret_cast<char*>(header.data()), header.size());
            if (!f) break;

            auto [payload_size, padding_size] = chunk_size_and_padding(header);

            // seek back and read full chunk header + payload (no padding)
            f.seekg(int64_t(-0x20), std::ios::cur);

            Bytes chunk_bytes(size_t(0x20 + payload_size));
            f.read(reinterpret_cast<char*>(chunk_bytes.data()), chunk_bytes.size());
            if (!f) throw std::runtime_error("Failed to read chunk bytes");

            // skip padding
            f.seekg(padding_size, std::ios::cur);

            UsmChunk c = UsmChunk::from_bytes(chunk_bytes, encoding);

            if (c.chunk_type == ChunkType::INFO) {
                if (std::holds_alternative<std::vector<UsmPage>>(c.payload)) {
                    const auto& pages = std::get<std::vector<UsmPage>>(c.payload);
                    crids.insert(crids.end(), pages.begin(), pages.end());
                }
                continue;
            }

            if (c.chunk_type == ChunkType::VIDEO) {
                chunk_helper(video_ch, c, offset);
            }
            else if (c.chunk_type == ChunkType::AUDIO) {
                chunk_helper(audio_ch, c, offset);
            }
            else if (c.chunk_type == ChunkType::ALPHA) {
                chunk_helper(alpha_ch, c, offset);
            }
        }

        // Find USM CRID page (chno == -1).
        std::optional<UsmPage> usm_crid;
        for (const auto& p : crids) {
            if (p.get("chno").has_value()) {
                int16_t chno = get_i16(p, "chno");
                if (chno == -1) {
                    usm_crid = p;
                    break;
                }
            }
        }
        if (!usm_crid.has_value()) {
            throw std::runtime_error("No usm crid page found");
        }

        Usm out;
        out.path_ = path;
        out.key_ = key;
        out.encoding_ = encoding;
        out.usm_crid_ = *usm_crid;

        auto build_tracks = [&](const std::unordered_map<int, ChannelAccum>& m,
            uint32_t want_stmid) -> std::vector<Track> {
                std::vector<Track> tracks;
                tracks.reserve(m.size());

                for (const auto& [chno, accum] : m) {
                    // Find matching CRIUSF_DIR_STREAM page for channel and stmid.
                    std::optional<UsmPage> crid_match;
                    for (const auto& p : crids) {
                        if (!p.get("chno").has_value() || !p.get("stmid").has_value()) continue;
                        if (get_i16(p, "chno") != int16_t(chno)) continue;
                        if (uint32_t(get_i32(p, "stmid")) != want_stmid) continue;
                        crid_match = p;
                        break;
                    }
                    if (!crid_match.has_value()) {
                        throw std::runtime_error("No crid page found for channel " +
                            std::to_string(chno));
                    }

                    Track t;
                    t.channel_number = chno;
                    t.crid = *crid_match;
                    t.header = accum.header;
                    t.metadata = accum.metadata;
                    t.stream = accum.stream;
                    tracks.push_back(std::move(t));
                }

                std::sort(tracks.begin(), tracks.end(),
                    [](const Track& a, const Track& b) {
                        return a.channel_number < b.channel_number;
                    });

                return tracks;
            };

        out.videos_ = build_tracks(video_ch, uint32_t(ChunkType::VIDEO));
        out.audios_ = build_tracks(audio_ch, uint32_t(ChunkType::AUDIO));
        out.alphas_ = build_tracks(alpha_ch, uint32_t(ChunkType::ALPHA));

        // version from fmtver of video channel 0 (if present).
        for (const auto& v : out.videos_) {
            if (v.channel_number != 0) continue;
            auto fmtver = v.crid.get("fmtver");
            if (fmtver.has_value() && fmtver->type == ElementType::I32) {
                out.version_ = int(std::get<int32_t>(fmtver->val));
            }
            break;
        }

        return out;
    }

    std::filesystem::path Usm::filepath() const { return path_; }

    const std::vector<Track>& Usm::videos() const { return videos_; }
    const std::vector<Track>& Usm::audios() const { return audios_; }
    const std::vector<Track>& Usm::alphas() const { return alphas_; }

    const UsmPage& Usm::usm_crid_page() const { return usm_crid_; }

    std::optional<int> Usm::version() const { return version_; }

    void Usm::demux(const std::filesystem::path& out_dir, bool save_video,
        bool save_audio, bool save_alpha,
        std::optional<uint64_t> key_override) const {
        std::optional<uint64_t> use_key = key_override.has_value() ? key_override : key_;
        std::optional<Bytes> video_key;
        std::optional<Bytes> audio_key;

        if (use_key.has_value()) {
            auto [vk, ak] = generate_keys(*use_key);
            video_key = std::move(vk);
            audio_key = std::move(ak);
        }

        std::string folder = path_.filename().string();
        folder = slugify_utf8(folder, true);

        std::filesystem::path out_root = out_dir / folder;
        std::filesystem::create_directories(out_root);

        auto write_track = [&](const Track& t, const std::filesystem::path& subdir,
            bool is_video, bool is_audio) {
                std::ifstream in(path_, std::ios::binary);
                if (!in) throw std::runtime_error("Failed to open input in demux");

                std::string name = get_str(t.crid, "filename");
                name = slugify_utf8(basename_utf8(name), true);

                std::filesystem::path out_path = subdir / name;
                std::ofstream out(out_path, std::ios::binary);
                if (!out) throw std::runtime_error("Failed to open output: " +
                    out_path.string());

                for (const auto& [off, sz] : t.stream) {
                    Bytes buf(sz);
                    in.seekg(int64_t(off), std::ios::beg);
                    in.read(reinterpret_cast<char*>(buf.data()), buf.size());
                    if (!in) throw std::runtime_error("Failed to read payload at offset");

                    if (use_key.has_value()) {
                        if (is_video && video_key.has_value()) {
                            buf = decrypt_video_packet(buf, *video_key);
                        }
                        else if (is_audio && audio_key.has_value()) {
                            buf = crypt_audio_packet(buf, *audio_key);
                        }
                    }

                    out.write(reinterpret_cast<const char*>(buf.data()), buf.size());
                }
            };

        if (save_video && !videos_.empty()) {
            auto sub = out_root / "videos";
            std::filesystem::create_directories(sub);
            for (const auto& t : videos_) {
                write_track(t, sub, true, false);
            }
        }

        if (save_audio && !audios_.empty()) {
            auto sub = out_root / "audios";
            std::filesystem::create_directories(sub);
            for (const auto& t : audios_) {
                write_track(t, sub, false, true);
            }
        }

        if (save_alpha && !alphas_.empty()) {
            auto sub = out_root / "alphas";
            std::filesystem::create_directories(sub);
            for (const auto& t : alphas_) {
                write_track(t, sub, true, false);
            }
        }
    }

}  // namespace usm
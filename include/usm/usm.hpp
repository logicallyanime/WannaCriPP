// TODO: INSPECT

#pragma once

#include "page.hpp"
#include "types.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace usm {

    struct Track {
        int channel_number = 0;
        UsmPage crid{ "CRIUSF_DIR_STREAM" };
        UsmPage header{ "" };
        std::optional<std::vector<UsmPage>> metadata;
        std::vector<std::pair<uint64_t, uint32_t>> stream;  // (offset, size)
    };

    class Usm {
    public:
        static Usm open(const std::filesystem::path& path,
            std::optional<uint64_t> key = std::nullopt,
            const std::string& encoding = "UTF-8");

        std::filesystem::path filepath() const;

        const std::vector<Track>& videos() const;
        const std::vector<Track>& audios() const;
        const std::vector<Track>& alphas() const;

        const UsmPage& usm_crid_page() const;
        std::optional<int> version() const;

        // Demux elementary streams (concatenated payloads).
        // If key was provided (or key_override), decrypt is applied.
        void demux(const std::filesystem::path& out_dir, bool save_video = true,
            bool save_audio = true, bool save_alpha = true,
            std::optional<uint64_t> key_override = std::nullopt) const;

    private:
        std::filesystem::path path_;
        std::optional<uint64_t> key_;
        std::string encoding_;

        UsmPage usm_crid_{ "CRIUSF_DIR_STREAM" };
        std::optional<int> version_;

        std::vector<Track> videos_;
        std::vector<Track> audios_;
        std::vector<Track> alphas_;
    };

}  // namespace usm
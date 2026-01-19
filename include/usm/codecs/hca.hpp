#pragma once
#include "usm/page.hpp"
#include "usm/media.hpp"

#include <filesystem>
#include <vector>

namespace usm {

    class HcaAudioSource final : public IAudioSource {
    public:
        explicit HcaAudioSource(std::filesystem::path hca_path,
                                int channel_number = 1,
                                std::optional<int32_t> fmtver = std::nullopt);

        const UsmPage& crid_page() const override;
        const UsmPage& header_page() const override;
        std::optional<std::vector<UsmPage>> metadata_pages() const override;

        int channel_number() const override;
        int length() const override;

        bool next(Bytes& out) override;
        void reset() override;

    private:
        void scan_hca();  // read metadata, compute frame sizes/count, pages
        std::filesystem::path path_;
        int chno_ = 1;

        UsmPage crid_{"CRIUSF_DIR_STREAM"};
        UsmPage header_{"AUDIO_HDRINFO"};

        // Packet table: first 96 bytes header, then frames
        std::vector<std::pair<uint64_t, uint32_t>> packets_;
        size_t cursor_ = 0;
    };

}  // namespace usm
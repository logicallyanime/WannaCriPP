#pragma once
#include "usm/page.hpp"
#include "usm/media.hpp"

#include <filesystem>
#include <vector>

namespace usm {

    class H264VideoSource final : public IVideoSource {
    public:
        explicit H264VideoSource(std::filesystem::path h264_path,
                                       int channel_number = 0,
                                       std::optional<int32_t> fmtver = std::nullopt);

        [[nodiscard]] const UsmPage& crid_page() const override;
        [[nodiscard]] const UsmPage& header_page() const override;
        [[nodiscard]] std::optional<std::vector<UsmPage>> metadata_pages() const override;

        [[nodiscard]] int channel_number() const override;
        [[nodiscard]] int length() const override;

        bool next(VideoPacket& out) override;
        void reset() override;

    private:
        void parse_access_units();  // builds AU table, keyframes, max size
        void build_pages();         // create CRID + VIDEO_HDRINFO

        std::filesystem::path path_;
        int chno_ = 0;

        UsmPage crid_{"CRIUSF_DIR_STREAM"};
        UsmPage header_{"VIDEO_HDRINFO"};

        struct AuInfo {
            uint64_t offset;
            uint32_t size;
            bool keyframe;
        };
        std::vector<AuInfo> aus_;
        size_t cursor_ = 0;
    };

}  // namespace usm
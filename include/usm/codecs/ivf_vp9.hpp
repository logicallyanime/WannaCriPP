#pragma once
#include "usm/page.hpp"
#include "usm/media.hpp"

#include <filesystem>
#include <vector>

namespace usm {

    class Vp9IvfVideoSource final : public IVideoSource {
    public:
        explicit Vp9IvfVideoSource(std::filesystem::path ivf_path,
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
        void scan_file();  // builds frame index, keyframes, max size, fps, pages

        std::filesystem::path path_;
        int chno_ = 0;

        UsmPage crid_{"CRIUSF_DIR_STREAM"};
        UsmPage header_{"VIDEO_HDRINFO"};

        // Frame table (offset,size) within IVF for fast reading
        struct FrameInfo {
            uint64_t offset;
            uint32_t size;
            bool keyframe;
        };
        std::vector<FrameInfo> frames_;
        size_t cursor_ = 0;
    };

}  // namespace usm
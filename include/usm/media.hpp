#pragma once
#include "bytes.hpp"
#include "page.hpp"
#include <optional>
#include <vector>

namespace usm {

    struct VideoPacket {
        Bytes data;
        bool keyframe = false;
    };

    class IVideoSource {
    public:
        virtual ~IVideoSource() = default;
        virtual const UsmPage& crid_page() const = 0;
        virtual const UsmPage& header_page() const = 0;
        virtual std::optional<std::vector<UsmPage>> metadata_pages() const = 0;

        virtual int channel_number() const = 0;
        virtual int length() const = 0;

        virtual bool next(VideoPacket& out) = 0;
        virtual void reset() = 0;

        bool is_alpha = false;
    };

    class IAudioSource {
    public:
        virtual ~IAudioSource() = default;
        virtual const UsmPage& crid_page() const = 0;
        virtual const UsmPage& header_page() const = 0;
        virtual std::optional<std::vector<UsmPage>> metadata_pages() const = 0;

        virtual int channel_number() const = 0;
        virtual int length() const = 0;

        virtual bool next(Bytes& out) = 0;
        virtual void reset() = 0;
    };

}  // namespace usm
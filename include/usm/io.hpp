#pragma once
#include "bytes.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ostream>

namespace usm {

    class TempFile {
    public:
        TempFile();            // create in %TEMP%
        ~TempFile();           // delete
        std::fstream& file();  // rb+/wb+

    private:
        std::filesystem::path path_;
        std::fstream f_;
    };

    class SectorWriter {
    public:
        explicit SectorWriter(std::ostream& out, uint32_t sector = 0x800);

        uint64_t tell() const;
        void write(const Bytes& b);
        void write_zeros(size_t n);
        void pad_to_sector();  // pad with zeros to next 0x800

    private:
        std::ostream& out_;
        uint32_t sector_;
        uint64_t pos_ = 0;
    };

}  // namespace usm
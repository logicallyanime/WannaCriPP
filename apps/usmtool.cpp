#include "usm/usm.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

static void usage() {
    std::cerr
        << "Usage:\n"
        << "  usmtool demux <input.usm> -o <outdir> [--key <num>]\n"
        << "             [--no-video] [--no-audio] [--no-alpha]\n";
}

static bool is_flag(const std::string& a, const char* s) { return a == s; }

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            usage();
            return 2;
        }

        std::vector<std::string> args(argv + 1, argv + argc);
        if (args[0] != "demux") {
            usage();
            return 2;
        }

        if (args.size() < 2) {
            usage();
            return 2;
        }

        std::filesystem::path input = args[1];
        std::filesystem::path outdir;
        bool save_video = true;
        bool save_audio = true;
        bool save_alpha = true;
        std::optional<uint64_t> key;

        for (size_t i = 2; i < args.size(); i++) {
            if (is_flag(args[i], "-o") && i + 1 < args.size()) {
                outdir = args[i + 1];
                i++;
            }
            else if (is_flag(args[i], "--key") && i + 1 < args.size()) {
                key = std::stoull(args[i + 1]);
                i++;
            }
            else if (is_flag(args[i], "--no-video")) {
                save_video = false;
            }
            else if (is_flag(args[i], "--no-audio")) {
                save_audio = false;
            }
            else if (is_flag(args[i], "--no-alpha")) {
                save_alpha = false;
            }
            else {
                usage();
                return 2;
            }
        }

        if (outdir.empty()) {
            usage();
            return 2;
        }

        usm::Usm u = usm::Usm::open(input, key);
        u.demux(outdir, save_video, save_audio, save_alpha);

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
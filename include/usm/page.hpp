// TODO: INSPECT

#pragma once

#include "bytes.hpp"
#include "types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace usm {

    using ElementValue =
        std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
        uint64_t, float, double, std::string, Bytes>;

    struct Element {
        ElementType type;
        ElementValue val;
    };

    class UsmPage {
    public:
        explicit UsmPage(std::string name);

        const std::string& name() const;
        const std::vector<std::string>& key_order() const;
        const std::unordered_map<std::string, Element>& dict() const;

        void update(const std::string& key, ElementType type, ElementValue value);

        std::optional<Element> get(const std::string& key) const;
        const Element& at(const std::string& key) const;

    private:
        std::string name_;
        std::vector<std::string> order_;
        std::unordered_map<std::string, Element> dict_;
    };

    std::vector<UsmPage> get_pages(const Bytes& info,
        const std::string& encoding = "UTF-8");

    Bytes pack_pages(const std::vector<UsmPage>& pages,
        const std::string& encoding = "UTF-8", int string_padding = 0);

    std::vector<int> keyframes_from_seek_pages(
        const std::optional<std::vector<UsmPage>>& seek_pages);

}  // namespace usm
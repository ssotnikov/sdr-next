#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace sdr {
inline std::filesystem::path path_from_utf8(std::string_view value) {
    return std::filesystem::path(std::u8string(value.begin(), value.end()));
}
} // namespace sdr

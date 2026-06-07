#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace wol {

struct MacAddress {
    std::array<std::uint8_t, 6> bytes{};
};

struct Ipv4Address {
    std::array<std::uint8_t, 4> bytes{};
};

MacAddress parse_mac(const std::string& text);
std::string format_mac(const MacAddress& mac);

Ipv4Address parse_ipv4(const std::string& text);
std::string format_ipv4(const Ipv4Address& ip);
std::uint32_t ipv4_to_host_u32(const Ipv4Address& ip);
Ipv4Address ipv4_from_host_u32(std::uint32_t value);

std::vector<std::uint8_t> build_magic_packet(const MacAddress& mac);

} // namespace wol

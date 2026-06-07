#include "wol/types.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace wol {

namespace {

int hex_value(char c) {
    const auto ch = static_cast<unsigned char>(c);
    if (std::isdigit(ch) != 0) {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

std::uint8_t parse_octet_hex(char high, char low) {
    const int hi = hex_value(high);
    const int lo = hex_value(low);
    if (hi < 0 || lo < 0) {
        throw std::invalid_argument("invalid MAC address hex digit");
    }
    return static_cast<std::uint8_t>((hi << 4) | lo);
}

} // namespace

MacAddress parse_mac(const std::string& text) {
    MacAddress mac{};
    const bool has_colon = text.find(':') != std::string::npos;
    const bool has_hyphen = text.find('-') != std::string::npos;
    if (has_colon && has_hyphen) {
        throw std::invalid_argument("MAC address must not mix ':' and '-' separators");
    }

    if (!has_colon && !has_hyphen) {
        if (text.size() != 12) {
            throw std::invalid_argument("compact MAC address must contain 12 hex digits");
        }
        for (std::size_t i = 0; i < 6; ++i) {
            mac.bytes[i] = parse_octet_hex(text[i * 2], text[(i * 2) + 1]);
        }
        return mac;
    }

    const char separator = has_colon ? ':' : '-';
    std::size_t start = 0;
    for (std::size_t i = 0; i < 6; ++i) {
        const auto end = text.find(separator, start);
        const auto token_end = (end == std::string::npos) ? text.size() : end;
        if (token_end - start != 2) {
            throw std::invalid_argument("separated MAC address must use six two-digit octets");
        }
        mac.bytes[i] = parse_octet_hex(text[start], text[start + 1]);
        start = token_end + 1;
        if (i < 5 && end == std::string::npos) {
            throw std::invalid_argument("MAC address has too few octets");
        }
        if (i == 5 && end != std::string::npos) {
            throw std::invalid_argument("MAC address has too many octets");
        }
    }
    return mac;
}

std::string format_mac(const MacAddress& mac) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < mac.bytes.size(); ++i) {
        if (i != 0) {
            out << ':';
        }
        out << std::setw(2) << static_cast<int>(mac.bytes[i]);
    }
    return out.str();
}

Ipv4Address parse_ipv4(const std::string& text) {
    in_addr parsed{};
    if (inet_pton(AF_INET, text.c_str(), &parsed) != 1) {
        throw std::invalid_argument("invalid IPv4 address: " + text);
    }
    const auto raw = ntohl(parsed.s_addr);
    return ipv4_from_host_u32(raw);
}

std::string format_ipv4(const Ipv4Address& ip) {
    std::ostringstream out;
    out << static_cast<int>(ip.bytes[0]) << '.' << static_cast<int>(ip.bytes[1]) << '.' << static_cast<int>(ip.bytes[2])
        << '.' << static_cast<int>(ip.bytes[3]);
    return out.str();
}

std::uint32_t ipv4_to_host_u32(const Ipv4Address& ip) {
    return (static_cast<std::uint32_t>(ip.bytes[0]) << 24U) | (static_cast<std::uint32_t>(ip.bytes[1]) << 16U) |
           (static_cast<std::uint32_t>(ip.bytes[2]) << 8U) | static_cast<std::uint32_t>(ip.bytes[3]);
}

Ipv4Address ipv4_from_host_u32(std::uint32_t value) {
    return Ipv4Address{{
        static_cast<std::uint8_t>((value >> 24U) & 0xFFU),
        static_cast<std::uint8_t>((value >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((value >> 8U) & 0xFFU),
        static_cast<std::uint8_t>(value & 0xFFU),
    }};
}

std::vector<std::uint8_t> build_magic_packet(const MacAddress& mac) {
    std::vector<std::uint8_t> packet(6, 0xFF);
    packet.reserve(102);
    for (int i = 0; i < 16; ++i) {
        packet.insert(packet.end(), mac.bytes.begin(), mac.bytes.end());
    }
    return packet;
}

} // namespace wol

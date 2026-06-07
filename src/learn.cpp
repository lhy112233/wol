#include "wol/learn.hpp"

#include "wol/network.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <fstream>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace wol {

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to read " + path.string());
    }
    std::ostringstream content;
    content << in.rdbuf();
    return content.str();
}

std::vector<InterfaceInfo> local_ipv4_interfaces() {
    ifaddrs* raw = nullptr;
    if (getifaddrs(&raw) != 0) {
        throw std::runtime_error("getifaddrs failed");
    }

    struct Guard {
        explicit Guard(ifaddrs* raw) : ptr(raw) {}
        ifaddrs* ptr;
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        Guard(Guard&&) = delete;
        Guard& operator=(Guard&&) = delete;
        ~Guard() {
            freeifaddrs(ptr);
        }
    } guard(raw);

    std::vector<InterfaceInfo> result;
    for (auto* item = raw; item != nullptr; item = item->ifa_next) {
        if (item->ifa_addr == nullptr || item->ifa_netmask == nullptr) {
            continue;
        }
        if (item->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if ((item->ifa_flags & IFF_LOOPBACK) != 0U) {
            continue;
        }

        const auto* addr = reinterpret_cast<const sockaddr_in*>(item->ifa_addr);
        const auto* mask = reinterpret_cast<const sockaddr_in*>(item->ifa_netmask);
        result.push_back(InterfaceInfo{
            item->ifa_name,
            ipv4_from_host_u32(ntohl(addr->sin_addr.s_addr)),
            ipv4_from_host_u32(ntohl(mask->sin_addr.s_addr)),
        });
    }
    return result;
}

} // namespace

std::optional<MacAddress> parse_arp_table_for_ip(const std::string& arp_table, const Ipv4Address& ip) {
    std::istringstream lines(arp_table);
    std::string line;
    const auto needle = format_ipv4(ip);
    bool first = true;
    while (std::getline(lines, line)) {
        if (first) {
            first = false;
            continue;
        }
        std::istringstream row(line);
        std::string ip_address;
        std::string hw_type;
        std::string flags;
        std::string hw_address;
        if (!(row >> ip_address >> hw_type >> flags >> hw_address)) {
            continue;
        }
        if (ip_address == needle && flags != "0x0" && hw_address != "00:00:00:00:00:00") {
            try {
                return parse_mac(hw_address);
            } catch (const std::exception&) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<Ipv4Address> broadcast_for_ip(const std::vector<InterfaceInfo>& interfaces, const Ipv4Address& ip) {
    const auto target = ipv4_to_host_u32(ip);
    for (const auto& interface : interfaces) {
        const auto address = ipv4_to_host_u32(interface.address);
        const auto mask = ipv4_to_host_u32(interface.netmask);
        if ((target & mask) == (address & mask)) {
            return ipv4_from_host_u32((address & mask) | (~mask));
        }
    }
    return std::nullopt;
}

void learn_target(Config& config, const std::string& name, const Ipv4Address& ip) {
    trigger_arp_resolution(ip);

    std::optional<MacAddress> mac;
    for (int attempt = 0; attempt < 20; ++attempt) {
        mac = parse_arp_table_for_ip(read_file("/proc/net/arp"), ip);
        if (mac) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!mac) {
        throw std::runtime_error("could not learn MAC for " + format_ipv4(ip) + " from /proc/net/arp");
    }

    const auto broadcast = broadcast_for_ip(local_ipv4_interfaces(), ip);
    if (!broadcast) {
        throw std::runtime_error("could not find a local IPv4 subnet for " + format_ipv4(ip));
    }

    auto& target = config.targets[name];
    target.mac = format_mac(*mac);
    target.ip = format_ipv4(ip);
    target.broadcast = format_ipv4(*broadcast);
    target.port = std::nullopt;
    if (config.default_target.empty()) {
        config.default_target = name;
    }
}

} // namespace wol

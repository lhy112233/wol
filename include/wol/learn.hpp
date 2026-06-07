#pragma once

#include "wol/config.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wol {

struct InterfaceInfo {
    std::string name;
    Ipv4Address address;
    Ipv4Address netmask;
};

std::optional<MacAddress> parse_arp_table_for_ip(const std::string& arp_table, const Ipv4Address& ip);
std::optional<Ipv4Address> broadcast_for_ip(const std::vector<InterfaceInfo>& interfaces, const Ipv4Address& ip);
void learn_target(Config& config, const std::string& name, const Ipv4Address& ip);

} // namespace wol

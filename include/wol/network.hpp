#pragma once

#include "wol/config.hpp"
#include "wol/types.hpp"

#include <string>

namespace wol {

void send_magic_packet(const MacAddress& mac, const NetworkConfig& network);
void trigger_arp_resolution(const Ipv4Address& ip);

} // namespace wol

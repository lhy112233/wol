#include "wol/network.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace wol {

namespace {

class Socket {
  public:
    explicit Socket(int fd) : fd_(fd) {}
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) = delete;
    Socket& operator=(Socket&&) = delete;
    ~Socket() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }
    [[nodiscard]]
    int get() const {
        return fd_;
    }

  private:
    int fd_ = -1;
};

std::string errno_message(const std::string& prefix) {
    return prefix + ": " + std::strerror(errno);
}

sockaddr_in sockaddr_for(const Ipv4Address& ip, int port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    address.sin_addr.s_addr = htonl(ipv4_to_host_u32(ip));
    return address;
}

} // namespace

void send_magic_packet(const MacAddress& mac, const NetworkConfig& network) {
    const auto broadcast = parse_ipv4(network.broadcast);
    const auto packet = build_magic_packet(mac);
    Socket sock(socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));
    if (sock.get() < 0) {
        throw std::runtime_error(errno_message("socket failed"));
    }

    int enabled = 1;
    if (setsockopt(sock.get(), SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled)) != 0) {
        throw std::runtime_error(errno_message("setsockopt SO_BROADCAST failed"));
    }

    const auto destination = sockaddr_for(broadcast, network.port);
    for (int i = 0; i < network.send_count; ++i) {
        const auto sent = sendto(sock.get(), packet.data(), packet.size(), 0,
                                 reinterpret_cast<const sockaddr*>(&destination), sizeof(destination));
        if (sent < 0 || static_cast<std::size_t>(sent) != packet.size()) {
            throw std::runtime_error(errno_message("sendto failed"));
        }
        if (i + 1 < network.send_count && network.interval_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(network.interval_ms));
        }
    }
}

void trigger_arp_resolution(const Ipv4Address& ip) {
    Socket sock(socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));
    if (sock.get() < 0) {
        throw std::runtime_error(errno_message("socket failed"));
    }
    const std::uint8_t byte = 0;
    const auto destination = sockaddr_for(ip, 9);
    (void)sendto(sock.get(), &byte, sizeof(byte), 0, reinterpret_cast<const sockaddr*>(&destination),
                 sizeof(destination));
}

} // namespace wol

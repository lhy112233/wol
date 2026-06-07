#pragma once

#include "wol/types.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace wol {

struct NetworkConfig {
    std::string broadcast = "255.255.255.255";
    int port = 9;
    int send_count = 1;
    int interval_ms = 100;
};

struct TargetConfig {
    std::string mac;
    std::optional<std::string> ip;
    std::optional<std::string> broadcast;
    std::optional<int> port;
};

struct Config {
    std::string default_target;
    NetworkConfig network;
    std::map<std::string, TargetConfig> targets;
};

Config load_config_file(const std::filesystem::path& path);
void save_config_file(const Config& config, const std::filesystem::path& path);
void validate_config(const Config& config);
const TargetConfig& select_target(const Config& config, const std::optional<std::string>& name);
NetworkConfig effective_network_for_target(const Config& config, const TargetConfig& target);
std::filesystem::path default_config_path(const char* argv0);

} // namespace wol

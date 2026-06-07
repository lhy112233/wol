#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wol {

enum class CommandKind {
    Wake,
    Learn,
    List,
    CheckConfig,
    PrintConfigPath,
    Help,
    Version,
};

struct CliOptions {
    CommandKind command = CommandKind::Wake;
    std::optional<std::filesystem::path> config_path;
    std::optional<std::string> target_name;
    std::optional<std::string> learn_ip;
    bool dry_run = false;
    bool json = false;
};

CliOptions parse_cli(int argc, char** argv);
int run_cli(int argc, char** argv);

} // namespace wol

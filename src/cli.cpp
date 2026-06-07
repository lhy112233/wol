#include "wol/cli.hpp"

#include "wol/config.hpp"
#include "wol/help.hpp"
#include "wol/learn.hpp"
#include "wol/network.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wol {

namespace {

bool is_option(const std::string& arg) {
    return arg.rfind("-", 0) == 0;
}

bool json_requested(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--json") {
            return true;
        }
    }
    return false;
}

void require_value(int& index, int argc, const std::string& option) {
    if (index + 1 >= argc) {
        throw std::invalid_argument(option + " requires a value");
    }
    ++index;
}

std::filesystem::path resolved_config_path(const CliOptions& options, char** argv) {
    if (options.config_path) {
        return *options.config_path;
    }
    return default_config_path(argv != nullptr ? argv[0] : nullptr);
}

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20U) {
                out << "\\u00";
                constexpr char hex[] = "0123456789abcdef";
                out << hex[(static_cast<unsigned char>(ch) >> 4U) & 0x0FU]
                    << hex[static_cast<unsigned char>(ch) & 0x0FU];
            } else {
                out << ch;
            }
            break;
        }
    }
    return out.str();
}

std::string json_string(const std::string& value) {
    return "\"" + json_escape(value) + "\"";
}

void print_json_error(const std::string& kind, const std::string& message, int exit_code) {
    std::cerr << "{\"ok\":false,\"error\":{\"kind\":" << json_string(kind)
              << ",\"message\":" << json_string(message)
              << ",\"exit_code\":" << exit_code << "}}\n";
}

std::string optional_json_string(const std::optional<std::string>& value) {
    return value ? json_string(*value) : "null";
}

std::string target_json(const std::string& name, const TargetConfig& target, bool is_default) {
    std::ostringstream out;
    out << "{\"name\":" << json_string(name)
        << ",\"default\":" << (is_default ? "true" : "false")
        << ",\"mac\":" << json_string(target.mac)
        << ",\"ip\":" << optional_json_string(target.ip)
        << ",\"broadcast\":" << optional_json_string(target.broadcast)
        << ",\"port\":";
    if (target.port) {
        out << *target.port;
    } else {
        out << "null";
    }
    out << "}";
    return out.str();
}

void print_target_line(const std::string& name, const TargetConfig& target, bool is_default) {
    std::cout << (is_default ? "* " : "  ") << name << "  mac=" << target.mac;
    if (target.ip) {
        std::cout << "  ip=" << *target.ip;
    }
    if (target.broadcast) {
        std::cout << "  broadcast=" << *target.broadcast;
    }
    if (target.port) {
        std::cout << "  port=" << *target.port;
    }
    std::cout << '\n';
}

Config load_required_config(const std::filesystem::path& config_path) {
    if (!std::filesystem::exists(config_path)) {
        throw std::runtime_error("config file not found: " + config_path.string()
            + ". Create wol.toml beside the executable, pass --config <path>, or run 'wol learn <name> <ipv4>' to create a learned target.");
    }
    auto config = load_config_file(config_path);
    validate_config(config);
    return config;
}

} // namespace

CliOptions parse_cli(int argc, char** argv) {
    CliOptions options;
    std::vector<std::string> positionals;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            options.command = CommandKind::Help;
        } else if (arg == "--version") {
            options.command = CommandKind::Version;
        } else if (arg == "--list") {
            options.command = CommandKind::List;
        } else if (arg == "--check-config") {
            options.command = CommandKind::CheckConfig;
        } else if (arg == "--print-config-path") {
            options.command = CommandKind::PrintConfigPath;
        } else if (arg == "--dry-run") {
            options.dry_run = true;
        } else if (arg == "--json") {
            options.json = true;
        } else if (arg == "--config") {
            require_value(i, argc, arg);
            options.config_path = std::filesystem::path(argv[i]);
        } else if (is_option(arg)) {
            throw std::invalid_argument("unknown option: " + arg);
        } else {
            positionals.push_back(arg);
        }
    }

    if (!positionals.empty() && positionals[0] == "learn") {
        if (options.command != CommandKind::Wake) {
            throw std::invalid_argument("learn cannot be combined with --list, --help, or --version");
        }
        if (positionals.size() != 3) {
            throw std::invalid_argument("usage: wol learn <name> <ipv4>");
        }
        options.command = CommandKind::Learn;
        options.target_name = positionals[1];
        options.learn_ip = positionals[2];
        return options;
    }

    if (positionals.size() > 1) {
        throw std::invalid_argument("too many positional arguments");
    }
    if (!positionals.empty()) {
        if (options.command != CommandKind::Wake) {
            throw std::invalid_argument("target name cannot be combined with control commands such as --list, --help, --version, --check-config, or --print-config-path");
        }
        options.target_name = positionals[0];
    }
    return options;
}

int run_cli(int argc, char** argv) {
    const bool wants_json = json_requested(argc, argv);
    try {
        const auto options = parse_cli(argc, argv);
        if (options.command == CommandKind::Help) {
            std::cout << help_text();
            return 0;
        }
        if (options.command == CommandKind::Version) {
            if (options.json) {
                std::cout << "{\"ok\":true,\"version\":" << json_string(version_text()) << "}\n";
                return 0;
            }
            std::cout << version_text() << '\n';
            return 0;
        }

        const auto config_path = resolved_config_path(options, argv);
        if (options.command == CommandKind::PrintConfigPath) {
            if (options.json) {
                std::cout << "{\"ok\":true,\"config_path\":" << json_string(config_path.string()) << "}\n";
            } else {
                std::cout << config_path.string() << '\n';
            }
            return 0;
        }

        auto config = (options.command == CommandKind::Learn && !std::filesystem::exists(config_path))
            ? Config{}
            : load_required_config(config_path);

        if (options.command == CommandKind::Learn) {
            if (!options.target_name || !options.learn_ip) {
                throw std::invalid_argument("usage: wol learn <name> <ipv4>");
            }
            learn_target(config, *options.target_name, parse_ipv4(*options.learn_ip));
            validate_config(config);
            save_config_file(config, config_path);
            const auto& target = config.targets.at(*options.target_name);
            if (options.json) {
                std::cout << "{\"ok\":true,\"action\":\"learn\",\"config_path\":"
                          << json_string(config_path.string())
                          << ",\"target\":" << target_json(*options.target_name, target, *options.target_name == config.default_target)
                          << "}\n";
                return 0;
            }
            std::cout << "learned " << *options.target_name << " mac=" << target.mac;
            if (target.ip) {
                std::cout << " ip=" << *target.ip;
            }
            if (target.broadcast) {
                std::cout << " broadcast=" << *target.broadcast;
            }
            std::cout << '\n';
            return 0;
        }

        if (options.command == CommandKind::CheckConfig) {
            if (options.json) {
                std::cout << "{\"ok\":true,\"config_path\":" << json_string(config_path.string())
                          << ",\"default_target\":" << json_string(config.default_target)
                          << ",\"target_count\":" << config.targets.size() << "}\n";
            } else {
                std::cout << "Config OK: " << config_path << " (default target: "
                          << config.default_target << ", targets: " << config.targets.size() << ")\n";
            }
            return 0;
        }

        if (options.command == CommandKind::List) {
            if (options.json) {
                std::cout << "{\"ok\":true,\"default_target\":" << json_string(config.default_target)
                          << ",\"targets\":[";
                bool first = true;
                for (const auto& [name, target] : config.targets) {
                    if (!first) {
                        std::cout << ',';
                    }
                    first = false;
                    std::cout << target_json(name, target, name == config.default_target);
                }
                std::cout << "]}\n";
                return 0;
            }
            for (const auto& [name, target] : config.targets) {
                print_target_line(name, target, name == config.default_target);
            }
            return 0;
        }

        const auto& target = select_target(config, options.target_name);
        const auto network = effective_network_for_target(config, target);
        const auto mac = parse_mac(target.mac);
        if (options.dry_run) {
            if (options.json) {
                std::cout << "{\"ok\":true,\"action\":\"dry-run\",\"target\":"
                          << json_string(options.target_name ? *options.target_name : config.default_target)
                          << ",\"mac\":" << json_string(format_mac(mac))
                          << ",\"broadcast\":" << json_string(network.broadcast)
                          << ",\"port\":" << network.port
                          << ",\"send_count\":" << network.send_count
                          << ",\"interval_ms\":" << network.interval_ms << "}\n";
                return 0;
            }
            std::cout << "dry-run target="
                      << (options.target_name ? *options.target_name : config.default_target)
                      << " mac=" << format_mac(mac)
                      << " broadcast=" << network.broadcast
                      << " port=" << network.port
                      << " send_count=" << network.send_count
                      << " interval_ms=" << network.interval_ms << '\n';
            return 0;
        }

        send_magic_packet(mac, network);
        if (options.json) {
            std::cout << "{\"ok\":true,\"action\":\"wake\",\"target\":"
                      << json_string(options.target_name ? *options.target_name : config.default_target)
                      << ",\"mac\":" << json_string(format_mac(mac))
                      << ",\"broadcast\":" << json_string(network.broadcast)
                      << ",\"port\":" << network.port
                      << ",\"send_count\":" << network.send_count << "}\n";
            return 0;
        }
        std::cout << "sent magic packet to "
                  << (options.target_name ? *options.target_name : config.default_target)
                  << " via " << network.broadcast << ':' << network.port << '\n';
        return 0;
    } catch (const std::invalid_argument& error) {
        if (wants_json) {
            print_json_error("usage", error.what(), 2);
            return 2;
        }
        std::cerr << "usage error: " << error.what() << "\n\n" << help_text();
        return 2;
    } catch (const std::exception& error) {
        if (wants_json) {
            print_json_error("runtime", error.what(), 1);
            return 1;
        }
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}

} // namespace wol

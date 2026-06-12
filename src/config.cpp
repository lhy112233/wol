#include "wol/config.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

namespace wol {

namespace {

enum class SectionKind {
    Root,
    Network,
    Target,
};

struct Section {
    SectionKind kind = SectionKind::Root;
    std::string target_name;
};

std::string trim(std::string text) {
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
    return text;
}

std::string strip_comment(const std::string& line) {
    bool in_quote = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '"') {
            in_quote = !in_quote;
        }
        if (!in_quote && line[i] == '#') {
            return line.substr(0, i);
        }
    }
    return line;
}

bool valid_target_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    return std::all_of(name.begin(), name.end(),
                       [](unsigned char ch) { return std::isalnum(ch) != 0 || ch == '_' || ch == '-' || ch == '.'; });
}

std::string parse_string_value(const std::string& value, int line_number) {
    const auto trimmed = trim(value);
    if (trimmed.size() < 2 || trimmed.front() != '"' || trimmed.back() != '"') {
        throw std::runtime_error("line " + std::to_string(line_number) + ": expected quoted string value");
    }
    std::string result;
    result.reserve(trimmed.size() - 2);
    for (std::size_t i = 1; i + 1 < trimmed.size(); ++i) {
        if (trimmed[i] == '\\') {
            if (i + 2 >= trimmed.size()) {
                throw std::runtime_error("line " + std::to_string(line_number) + ": dangling escape in string");
            }
            const char escaped = trimmed[++i];
            if (escaped == '"' || escaped == '\\') {
                result.push_back(escaped);
            } else {
                throw std::runtime_error("line " + std::to_string(line_number) + ": unsupported string escape");
            }
        } else {
            result.push_back(trimmed[i]);
        }
    }
    return result;
}

int parse_int_value(const std::string& value, int line_number) {
    const auto trimmed = trim(value);
    std::size_t index = 0;
    try {
        const int parsed = std::stoi(trimmed, &index, 10);
        if (index != trimmed.size()) {
            throw std::runtime_error("trailing characters");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::runtime_error("line " + std::to_string(line_number) + ": expected integer value");
    }
}

std::string quote_string(const std::string& text) {
    std::string result = "\"";
    for (char ch : text) {
        if (ch == '"' || ch == '\\') {
            result.push_back('\\');
        }
        result.push_back(ch);
    }
    result.push_back('"');
    return result;
}

void validate_port(int port, const std::string& label) {
    if (port < 1 || port > 65535) {
        throw std::runtime_error(label + " must be between 1 and 65535");
    }
}

std::filesystem::path temporary_config_path(const std::filesystem::path& path) {
    static std::atomic<unsigned long long> counter{0};

    const auto parent = path.parent_path();
    const auto filename = path.filename().string();
    const auto sequence = counter.fetch_add(1, std::memory_order_relaxed);
    const auto temporary_name = "." + filename + ".tmp." + std::to_string(getpid()) + "." + std::to_string(sequence);
    return parent.empty() ? std::filesystem::path(temporary_name) : parent / temporary_name;
}

void write_config_contents(std::ostream& out, const Config& config) {
    out << "default_target = " << quote_string(config.default_target) << "\n\n";
    out << "[network]\n";
    out << "port = " << config.network.port << "\n";
    out << "send_count = " << config.network.send_count << "\n";
    out << "interval_ms = " << config.network.interval_ms << "\n";
    out << "broadcast = " << quote_string(config.network.broadcast) << "\n";

    for (const auto& [name, target] : config.targets) {
        out << "\n[targets." << name << "]\n";
        out << "mac = " << quote_string(target.mac) << "\n";
        if (target.ip) {
            out << "ip = " << quote_string(*target.ip) << "\n";
        }
        if (target.broadcast) {
            out << "broadcast = " << quote_string(*target.broadcast) << "\n";
        }
        if (target.port) {
            out << "port = " << *target.port << "\n";
        }
    }
}

std::string config_contents(const Config& config) {
    std::ostringstream out;
    write_config_contents(out, config);
    return out.str();
}

std::runtime_error errno_error(const std::string& message) {
    return std::runtime_error(message + ": " + std::strerror(errno));
}

class FileDescriptor {
  public:
    explicit FileDescriptor(int fd) : fd_(fd) {}
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&&) = delete;
    FileDescriptor& operator=(FileDescriptor&&) = delete;
    ~FileDescriptor() {
        if (fd_ >= 0) {
            (void)::close(fd_);
        }
    }

    [[nodiscard]]
    int get() const {
        return fd_;
    }

    void close_or_throw(const std::string& path) {
        if (fd_ >= 0 && ::close(fd_) != 0) {
            fd_ = -1;
            throw errno_error("failed to close " + path);
        }
        fd_ = -1;
    }

  private:
    int fd_ = -1;
};

FileDescriptor create_unique_temporary_file(const std::filesystem::path& path, std::filesystem::path& temporary_path) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        temporary_path = temporary_config_path(path);
        const int fd = ::open(temporary_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd >= 0) {
            return FileDescriptor(fd);
        }
        if (errno != EEXIST) {
            throw errno_error("failed to open temporary config file " + temporary_path.string());
        }
    }
    throw std::runtime_error("failed to create a unique temporary config file for " + path.string());
}

void write_all(int fd, const std::string& content, const std::filesystem::path& path) {
    std::size_t written = 0;
    while (written < content.size()) {
        const auto result = ::write(fd, content.data() + written, content.size() - written);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw errno_error("failed to write temporary config file " + path.string());
        }
        if (result == 0) {
            throw std::runtime_error("failed to write temporary config file " + path.string() + ": wrote zero bytes");
        }
        written += static_cast<std::size_t>(result);
    }
}

void fsync_file_or_throw(int fd, const std::filesystem::path& path) {
    while (::fsync(fd) != 0) {
        if (errno != EINTR) {
            throw errno_error("failed to sync temporary config file " + path.string());
        }
    }
}

void fsync_parent_directory(const std::filesystem::path& path) {
    const auto parent = path.parent_path().empty() ? std::filesystem::path(".") : path.parent_path();
    const int fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) {
        return;
    }
    FileDescriptor directory(fd);
    while (::fsync(directory.get()) != 0) {
        if (errno != EINTR) {
            return;
        }
    }
}

} // namespace

Config load_config_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open config file: " + path.string());
    }

    Config config;
    Section section;
    std::string line;
    int line_number = 0;
    while (std::getline(in, line)) {
        ++line_number;
        line = trim(strip_comment(line));
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[') {
            if (line.back() != ']') {
                throw std::runtime_error("line " + std::to_string(line_number) + ": malformed table header");
            }
            const auto table = line.substr(1, line.size() - 2);
            if (table == "network") {
                section = {SectionKind::Network, {}};
            } else if (table.starts_with("targets.")) {
                const auto name = table.substr(8);
                if (!valid_target_name(name)) {
                    throw std::runtime_error("line " + std::to_string(line_number) + ": invalid target name");
                }
                config.targets.try_emplace(name);
                section = {SectionKind::Target, name};
            } else {
                throw std::runtime_error("line " + std::to_string(line_number) + ": unsupported table [" + table + "]");
            }
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error("line " + std::to_string(line_number) + ": expected key = value");
        }
        const auto key = trim(line.substr(0, equals));
        const auto value = line.substr(equals + 1);
        if (key.empty()) {
            throw std::runtime_error("line " + std::to_string(line_number) + ": empty key");
        }

        if (section.kind == SectionKind::Root) {
            if (key == "default_target") {
                config.default_target = parse_string_value(value, line_number);
            } else {
                throw std::runtime_error("line " + std::to_string(line_number) + ": unsupported root key " + key);
            }
        } else if (section.kind == SectionKind::Network) {
            if (key == "broadcast") {
                config.network.broadcast = parse_string_value(value, line_number);
            } else if (key == "port") {
                config.network.port = parse_int_value(value, line_number);
            } else if (key == "send_count") {
                config.network.send_count = parse_int_value(value, line_number);
            } else if (key == "interval_ms") {
                config.network.interval_ms = parse_int_value(value, line_number);
            } else {
                throw std::runtime_error("line " + std::to_string(line_number) + ": unsupported network key " + key);
            }
        } else {
            auto& target = config.targets.at(section.target_name);
            if (key == "mac") {
                target.mac = parse_string_value(value, line_number);
            } else if (key == "ip") {
                target.ip = parse_string_value(value, line_number);
            } else if (key == "broadcast") {
                target.broadcast = parse_string_value(value, line_number);
            } else if (key == "port") {
                target.port = parse_int_value(value, line_number);
            } else {
                throw std::runtime_error("line " + std::to_string(line_number) + ": unsupported target key " + key);
            }
        }
    }

    return config;
}

void save_config_file(const Config& config, const std::filesystem::path& path) {
    std::filesystem::path temporary_path;
    try {
        {
            auto temporary_file = create_unique_temporary_file(path, temporary_path);
            const auto content = config_contents(config);
            write_all(temporary_file.get(), content, temporary_path);
            fsync_file_or_throw(temporary_file.get(), temporary_path);
            temporary_file.close_or_throw(temporary_path.string());
        }
        std::filesystem::rename(temporary_path, path);
        fsync_parent_directory(path);
    } catch (const std::exception& error) {
        std::error_code ignored;
        if (!temporary_path.empty()) {
            std::filesystem::remove(temporary_path, ignored);
        }
        throw std::runtime_error("failed to write config file atomically: " + path.string() + ": " + error.what());
    }
}

void validate_config(const Config& config) {
    if (config.default_target.empty()) {
        throw std::runtime_error("default_target is required");
    }
    if (!valid_target_name(config.default_target)) {
        throw std::runtime_error("default_target contains unsupported characters");
    }
    if (!config.targets.contains(config.default_target)) {
        throw std::runtime_error("default_target '" + config.default_target + "' does not exist in targets");
    }
    validate_port(config.network.port, "network.port");
    if (config.network.send_count < 1 || config.network.send_count > 20) {
        throw std::runtime_error("network.send_count must be between 1 and 20");
    }
    if (config.network.interval_ms < 0 || config.network.interval_ms > 60000) {
        throw std::runtime_error("network.interval_ms must be between 0 and 60000");
    }
    (void)parse_ipv4(config.network.broadcast);

    for (const auto& [name, target] : config.targets) {
        if (!valid_target_name(name)) {
            throw std::runtime_error("invalid target name: " + name);
        }
        if (target.mac.empty()) {
            throw std::runtime_error("target '" + name + "' is missing mac");
        }
        (void)parse_mac(target.mac);
        if (target.ip) {
            (void)parse_ipv4(*target.ip);
        }
        if (target.broadcast) {
            (void)parse_ipv4(*target.broadcast);
        }
        if (target.port) {
            validate_port(*target.port, "target '" + name + "'.port");
        }
    }
}

const TargetConfig& select_target(const Config& config, const std::optional<std::string>& name) {
    const auto& selected = name ? *name : config.default_target;
    const auto found = config.targets.find(selected);
    if (found == config.targets.end()) {
        throw std::runtime_error("unknown target: " + selected);
    }
    return found->second;
}

NetworkConfig effective_network_for_target(const Config& config, const TargetConfig& target) {
    auto network = config.network;
    if (target.broadcast) {
        network.broadcast = *target.broadcast;
    }
    if (target.port) {
        network.port = *target.port;
    }
    return network;
}

std::filesystem::path default_config_path(const char* argv0) {
    std::array<char, 4096> buffer{};
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length > 0) {
        buffer[static_cast<std::size_t>(length)] = '\0';
        return std::filesystem::path(buffer.data()).parent_path() / "wol.toml";
    }

    if (argv0 != nullptr && std::string(argv0).find('/') != std::string::npos) {
        return std::filesystem::absolute(argv0).parent_path() / "wol.toml";
    }
    return std::filesystem::current_path() / "wol.toml";
}

} // namespace wol

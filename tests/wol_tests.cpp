#include "wol/cli.hpp"
#include "wol/config.hpp"
#include "wol/help.hpp"
#include "wol/learn.hpp"
#include "wol/types.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::filesystem::path temp_file(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove(path);
    return path;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path);
    out << text;
}

std::string sample_config_text() {
    return "default_target = \"desktop\"\n"
           "\n"
           "[network]\n"
           "port = 7\n"
           "send_count = 2\n"
           "interval_ms = 50\n"
           "broadcast = \"192.168.1.255\"\n"
           "\n"
           "[targets.desktop]\n"
           "mac = \"AA:BB:CC:DD:EE:FF\"\n"
           "ip = \"192.168.1.20\"\n"
           "\n"
           "[targets.nas]\n"
           "mac = \"01-23-45-67-89-ab\"\n"
           "broadcast = \"192.168.1.255\"\n"
           "port = 9\n";
}

wol::CliOptions parse_test_cli(std::initializer_list<std::string_view> args) {
    std::vector<std::string> storage;
    storage.reserve(args.size());
    std::vector<char*> argv;
    argv.reserve(args.size());

    for (const auto arg : args) {
        storage.emplace_back(arg);
        argv.push_back(storage.back().data());
    }

    return wol::parse_cli(static_cast<int>(argv.size()), argv.data());
}

template <typename T>
const T& require_optional(const std::optional<T>& value) {
    if (!value.has_value()) {
        throw std::runtime_error("expected optional value");
    }
    return value.value();
}

} // namespace

TEST(MacAddressTest, ParsesSupportedFormatsAndFormatsCanonicalUppercase) {
    const auto colon = wol::parse_mac("aa:bb:cc:dd:ee:ff");
    const auto hyphen = wol::parse_mac("AA-BB-CC-DD-EE-FF");
    const auto compact = wol::parse_mac("AABBCCDDEEFF");

    EXPECT_EQ(colon.bytes, hyphen.bytes);
    EXPECT_EQ(colon.bytes, compact.bytes);
    EXPECT_EQ(wol::format_mac(colon), "AA:BB:CC:DD:EE:FF");
}

TEST(MacAddressTest, RejectsMalformedMacAddresses) {
    EXPECT_THROW((void)wol::parse_mac("AA:BB:CC:DD:EE"), std::invalid_argument);
    EXPECT_THROW((void)wol::parse_mac("AA:BB:CC:DD:EE:GG"), std::invalid_argument);
    EXPECT_THROW((void)wol::parse_mac("AA:BB-CC:DD:EE:FF"), std::invalid_argument);
}

TEST(Ipv4AddressTest, ParsesFormatsAndConvertsToHostInteger) {
    const auto ip = wol::parse_ipv4("192.168.1.20");

    EXPECT_EQ(wol::format_ipv4(ip), "192.168.1.20");
    EXPECT_EQ(wol::ipv4_from_host_u32(wol::ipv4_to_host_u32(ip)).bytes, ip.bytes);
    EXPECT_THROW((void)wol::parse_ipv4("192.168.1.300"), std::invalid_argument);
}

TEST(Ipv4AddressTest, ComputesBroadcastForMatchingInterface) {
    const std::vector<wol::InterfaceInfo> interfaces{
        {"eth0", wol::parse_ipv4("192.168.1.2"), wol::parse_ipv4("255.255.255.0")},
    };

    const auto broadcast = wol::broadcast_for_ip(interfaces, wol::parse_ipv4("192.168.1.20"));

    EXPECT_EQ(wol::format_ipv4(require_optional(broadcast)), "192.168.1.255");
}

TEST(MagicPacketTest, ContainsHeaderAndSixteenMacRepetitions) {
    const auto mac = wol::parse_mac("01:23:45:67:89:AB");
    const auto packet = wol::build_magic_packet(mac);

    ASSERT_EQ(packet.size(), 102U);
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(packet[static_cast<std::size_t>(i)], 0xFF);
    }
    for (int repeat = 0; repeat < 16; ++repeat) {
        for (int octet = 0; octet < 6; ++octet) {
            EXPECT_EQ(packet[6 + (repeat * 6) + octet], mac.bytes[static_cast<std::size_t>(octet)]);
        }
    }
}

TEST(ConfigTest, LoadsValidatesSelectsAndRoundtripsToml) {
    const auto path = temp_file("wol-config-test.toml");
    write_text(path, sample_config_text());

    const auto config = wol::load_config_file(path);

    EXPECT_NO_THROW(wol::validate_config(config));
    EXPECT_EQ(config.default_target, "desktop");
    EXPECT_EQ(config.network.port, 7);
    EXPECT_EQ(config.targets.size(), 2U);
    EXPECT_EQ(wol::select_target(config, std::nullopt).mac, "AA:BB:CC:DD:EE:FF");
    EXPECT_EQ(wol::select_target(config, std::optional<std::string>("nas")).mac, "01-23-45-67-89-ab");

    const auto effective = wol::effective_network_for_target(config, config.targets.at("nas"));
    EXPECT_EQ(effective.port, 9);
    EXPECT_EQ(effective.broadcast, "192.168.1.255");

    const auto saved = temp_file("wol-config-saved.toml");
    wol::save_config_file(config, saved);
    const auto roundtrip = wol::load_config_file(saved);
    EXPECT_NO_THROW(wol::validate_config(roundtrip));
    EXPECT_EQ(require_optional(roundtrip.targets.at("desktop").ip), "192.168.1.20");
}

TEST(ConfigTest, DefaultsToDebianWakeonlanSingleSendWireBehavior) {
    const wol::NetworkConfig network;

    EXPECT_EQ(network.broadcast, "255.255.255.255");
    EXPECT_EQ(network.port, 9);
    EXPECT_EQ(network.send_count, 1);
}

TEST(ConfigTest, RejectsInvalidConfigWithUsefulContext) {
    auto config = wol::Config{};
    config.default_target = "missing";
    config.targets["desktop"].mac = "not-a-mac";

    EXPECT_THROW(wol::validate_config(config), std::runtime_error);
}

TEST(ArpLearnTest, ParsesArpTableRows) {
    const std::string arp = "IP address       HW type     Flags       HW address            Mask     Device\n"
                            "192.168.1.20     0x1         0x2         aa:bb:cc:dd:ee:ff     *        eth0\n";

    const auto mac = wol::parse_arp_table_for_ip(arp, wol::parse_ipv4("192.168.1.20"));

    EXPECT_EQ(wol::format_mac(require_optional(mac)), "AA:BB:CC:DD:EE:FF");
    EXPECT_FALSE(wol::parse_arp_table_for_ip(arp, wol::parse_ipv4("192.168.1.21")).has_value());
}

TEST(CliParseTest, ParsesDefaultWakeNamedWakeAndDryRun) {
    const auto wake_default = parse_test_cli({"wol"});
    EXPECT_EQ(wake_default.command, wol::CommandKind::Wake);
    EXPECT_FALSE(wake_default.target_name.has_value());

    const auto dry_run = parse_test_cli({"wol", "--dry-run", "--config", "/tmp/wol.toml", "nas"});
    EXPECT_TRUE(dry_run.dry_run);
    EXPECT_EQ(dry_run.config_path, std::filesystem::path("/tmp/wol.toml"));
    EXPECT_EQ(require_optional(dry_run.target_name), "nas");
}

TEST(CliParseTest, ParsesAgentFriendlyCommands) {
    const auto list = parse_test_cli({"wol", "--list", "--json"});
    EXPECT_EQ(list.command, wol::CommandKind::List);
    EXPECT_TRUE(list.json);

    const auto check = parse_test_cli({"wol", "--check-config", "--json"});
    EXPECT_EQ(check.command, wol::CommandKind::CheckConfig);
    EXPECT_TRUE(check.json);

    const auto print_path = parse_test_cli({"wol", "--print-config-path"});
    EXPECT_EQ(print_path.command, wol::CommandKind::PrintConfigPath);
}

TEST(CliParseTest, ParsesLearnCommand) {
    const auto learn = parse_test_cli({"wol", "learn", "desktop", "192.168.1.20"});

    EXPECT_EQ(learn.command, wol::CommandKind::Learn);
    EXPECT_EQ(require_optional(learn.target_name), "desktop");
    EXPECT_EQ(require_optional(learn.learn_ip), "192.168.1.20");
}

TEST(HelpTest, DocumentsHumansAndAgents) {
    const auto help = wol::help_text();

    EXPECT_NE(help.find("Usage:"), std::string::npos);
    EXPECT_NE(help.find("wol learn <name> <ipv4>"), std::string::npos);
    EXPECT_NE(help.find("--json"), std::string::npos);
    EXPECT_NE(help.find("--check-config"), std::string::npos);
    EXPECT_NE(help.find("Exit codes"), std::string::npos);
}

TEST(VersionTest, ReportsVersionOnePointZero) {
    EXPECT_EQ(wol::version_text(), "wol v1.0");
}

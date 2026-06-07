#include "wol/help.hpp"

#include <sstream>

#ifndef WOL_VERSION
#define WOL_VERSION "dev"
#endif

namespace wol {

std::string version_text() {
    return "wol v" WOL_VERSION;
}

std::string help_text() {
    return R"(wol - dependency-free Wake-on-LAN for Linux

Default wake behavior follows Debian wakeonlan wire semantics: one UDP magic
packet to 255.255.255.255:9 with SO_BROADCAST enabled. Named TOML targets,
JSON output, and learned subnet broadcasts are project conveniences around that
same packet format.

Usage:
  wol [options] [target]
  wol learn [options] <name> <ipv4>
  wol --list [--config <path>]
  wol --help
  wol --version

Commands:
  wol
      Wake the default target from wol.toml.

  wol <target>
      Wake a named target from [targets.<target>].

  wol learn <name> <ipv4>
      Learn an online local IPv4 device. The command sends a small UDP probe,
      reads /proc/net/arp, computes the interface broadcast address, and writes
      mac/ip/broadcast into wol.toml. This only works for devices on a directly
      connected IPv4 network.

Options:
  --config <path>   Use a specific TOML config file.
  --json            Emit stable JSON for agent/automation-friendly commands.
  --dry-run         Validate and print the selected wake packet destination
                   without sending network packets.
  --list            List configured targets.
  --check-config    Validate the config file and report the selected default.
  --print-config-path
                   Print the config path wol would use, then exit.
  --help, -h        Show this help.
  --version         Show version.

Examples:
  wol
  wol nas
  wol --dry-run desktop
  wol --dry-run --json desktop
  wol --list
  wol --list --json
  wol --check-config --json
  wol --print-config-path
  wol learn desktop 192.168.1.20
  wol --config /opt/wol/wol.toml nas

Agent and script usage:
  Use --json with --list, --dry-run, --check-config, learn, and wake commands
  when another program needs stable output. JSON errors use this shape:
  {"ok":false,"error":{"kind":"runtime","message":"...","exit_code":1}}
  Commands never prompt interactively and do not use color output.

Config discovery:
  If --config is not provided, wol reads wol.toml from the same directory as
  the executable. A release archive therefore only needs these two files:
  wol and wol.toml.

TOML example:
  default_target = "desktop"

  [network]
  port = 9
  send_count = 1
  interval_ms = 100
  broadcast = "255.255.255.255"

  [targets.desktop]
  mac = "AA:BB:CC:DD:EE:FF"
  ip = "192.168.1.20"
  broadcast = "192.168.1.255"
  port = 9

Exit codes:
  0  Success.
  1  Runtime error such as invalid config, unknown target, or send failure.
  2  Command-line usage error.

Troubleshooting:
  - learn requires the target to be powered on, reachable, and on the same
    local IPv4 network. If learning fails, ping or otherwise contact the host
    first, then retry.
  - Wake failures are often firmware, BIOS/UEFI, NIC, switch, or subnet
    broadcast configuration issues. Confirm Wake-on-LAN is enabled on the
    target and use --dry-run to verify the selected MAC, broadcast, and port.
  - This release is intended to be fully static: it should not need shared
    runtime libraries on Debian/Ubuntu or Fedora-family x86_64 systems.
  - By default wol sends one packet, matching Debian wakeonlan. Set send_count
    above 1 only if you explicitly want repeated sends.
)";
}

} // namespace wol

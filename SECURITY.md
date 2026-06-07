# Security Policy

## Supported Versions

Security fixes are applied to the latest tagged release and `main`.

| Version | Supported |
| ------- | --------- |
| v1.1.x  | Yes       |
| < v1.1  | No        |

## Reporting A Vulnerability

Please report security-sensitive issues privately to the repository owner before
opening a public issue. Include:

- The affected version or commit.
- The command and configuration involved.
- Expected and observed behavior.
- Any logs or JSON output that help reproduce the issue.

This project sends UDP Wake-on-LAN packets and reads local Linux network state.
Security reports should focus on local file handling, unsafe parsing, unexpected
network behavior, or release artifact integrity.

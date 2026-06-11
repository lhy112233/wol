# wol

[English](README.md) | [简体中文](README.zh-CN.md)

`wol` 是一个小型、无运行时依赖的 Linux x86_64 Wake-on-LAN 命令行工具。
它同时照顾人类用户和自动化 Agent：普通命令输出易读文本，支持的命令可以通过
`--json` 输出稳定的机器可读 JSON。

默认网络唤醒行为与 Debian `wakeonlan` 的线缆级行为一致：向
`255.255.255.255:9` 通过 UDP broadcast 发送一个 Wake-on-LAN magic packet。
也支持为目标单独配置 broadcast 地址，这相当于 Debian `wakeonlan -i` 的用法。

发布压缩包只包含两个运行时文件：

- `wol`
- `wol.toml`

除非使用 `--config <path>` 指定配置，否则可执行文件会在自身所在目录查找
`wol.toml`。

## 构建

项目使用 CMake presets 和 Ninja。

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

格式和静态分析：

```bash
cmake --build --preset format-check
cmake --build --preset tidy
```

需要主动格式化代码时运行：

```bash
cmake --build --preset format
```

构建全静态 release：

```bash
cmake --preset release-static
cmake --build --preset release-static
ctest --preset release-static
```

创建发布压缩包：

```bash
cmake --preset package
cmake --build --preset package
```

package target 会输出：

```text
dist/wol-linux-x86_64.tar.gz
```

打包前会验证可执行文件是静态链接：

- `ldd wol` 必须报告它不是动态可执行文件。
- `readelf -d wol` 不能包含 `NEEDED` 条目。
- `file wol` 必须报告这是 statically linked executable。

## GitHub 自动化

仓库包含常见的 GitHub 项目自动化配置：

- CI 会在 push 到 `main`、pull request 和手动触发时运行。
- CI 会验证格式、clang-tidy、dev 测试、release-static 测试和静态打包。
- 推送匹配 `v*` 的 tag 会运行 release workflow，并把
  `dist/wol-linux-x86_64.tar.gz`、SHA256 校验文件和 SPDX SBOM 发布到 GitHub Release。
- Dependabot 每周检查 GitHub Actions 更新。
- Issue 模板、PR 模板、CODEOWNERS、CONTRIBUTING 和 SECURITY 文件记录了常规
  社区协作流程。

## 验证发布包

从 GitHub Release 下载这些资产：

- `wol-linux-x86_64.tar.gz`
- `wol-linux-x86_64.tar.gz.sha256`
- `wol-linux-x86_64.spdx.json`

验证 SHA256：

```bash
sha256sum -c wol-linux-x86_64.tar.gz.sha256
```

验证 GitHub artifact provenance：

```bash
gh attestation verify wol-linux-x86_64.tar.gz --repo lhy112233/wol
```

SBOM 会以 SPDX JSON 形式随 release 发布。它不会放进运行时压缩包，所以压缩包仍然
只包含 `wol` 和 `wol.toml`。GitHub Release workflow 会自动生成 SBOM；如果手动
发布，需要先生成 SBOM 再上传资产。

## 构建依赖

Debian/Ubuntu：

```bash
sudo apt install cmake ninja-build g++ libc6-dev
```

Fedora：

```bash
sudo dnf install cmake ninja-build gcc-c++ glibc-static libstdc++-static gtest-devel clang-tools-extra binutils file tar gzip
```

不同发行版可能会把静态 C++ 运行时文件拆到单独的软件包中。如果
`release-static` 因为缺少 `libstdc++.a` 或 `libc.a` 失败，请安装发行版提供的
对应静态库软件包。

如果维护者需要在 GitHub Actions 之外生成 SBOM，还需要安装 Syft。具体本地命令见
[Release](docs/RELEASE.md)。

## 使用

唤醒默认目标：

```bash
./wol
```

唤醒命名目标：

```bash
./wol desktop
```

验证并显示将要发送的内容，但不真正发包：

```bash
./wol --dry-run desktop
```

列出目标：

```bash
./wol --list
```

Agent 友好的 JSON 输出：

```bash
./wol --list --json
./wol --dry-run --json desktop
./wol --check-config --json
./wol --print-config-path
```

学习一个在线的本地 IPv4 目标：

```bash
./wol learn desktop 192.168.1.20
```

`learn` 会发送一个很小的 UDP probe，读取 `/proc/net/arp`，根据匹配到的网络接口
计算本地 broadcast 地址，并更新 `wol.toml`。

显示完整内置手册：

```bash
./wol --help
```

## 配置

```toml
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
```

目标级别的 `broadcast` 和 `port` 会覆盖 `[network]` 中的默认值。

## 退出码

- `0`：成功
- `1`：运行时错误，例如配置无效、目标不存在或发送失败
- `2`：命令行用法错误

## 故障排查

`learn` 只适用于已经开机、可访问、且位于同一本地 IPv4 网络中的目标。如果学习
失败，请先用 ping 或其他本地网络操作接触该主机，再重试。

唤醒失败通常和固件、BIOS/UEFI、网卡、交换机或跨网段 broadcast 设置有关。请确认
目标设备已启用 Wake-on-LAN，并运行 `./wol --dry-run <target>` 检查选中的 MAC
地址、broadcast 地址和端口。

## 更多文档

- [Agent guide](AGENTS.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Testing](docs/TESTING.md)
- [Release](docs/RELEASE.md)
- [Handoff](docs/HANDOFF.md)

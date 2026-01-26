# oatmux

Stream tmux sessions to your browser. Access your terminal from any device.

## Quick Start

```bash
# Install dependencies (Ubuntu/Debian)
./install.sh

# Build
./build.sh

# Run
./build/oatmux -s mysession
```

Open `http://localhost:8080` in your browser.

## Usage

```
oatmux [OPTIONS]

Options:
  -p, --port PORT      Port to listen on (default: 8080)
  -s, --session NAME   tmux session name (default: 0)
  -b, --bind ADDR      Bind address (default: 0.0.0.0)
  -h, --help           Show help
```

## Examples

```bash
# Stream session "dev" on port 3000
./build/oatmux -s dev -p 3000

# Local only (no external access)
./build/oatmux -b 127.0.0.1 -s mysession
```

## Build Options

```bash
./build.sh Release      # Optimized (default)
./build.sh Debug        # With sanitizers
```

## Requirements

- Linux (x86_64, ARM64, ARMv7)
- tmux
- OpenSSL
- CMake 3.10+

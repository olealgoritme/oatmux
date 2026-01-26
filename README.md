# oatmux

Stream tmux sessions to your browser. Access your terminal from any device.

## Quick Start

```bash
./install.sh       # Install dependencies
./compile.sh       # Build
./build/oatmux     # Run (interactive session picker)
```

Open `http://localhost:8080` in your browser.

## Usage

```
oatmux [OPTIONS]

Options:
  -p, --port PORT      Port (default: 8080)
  -s, --session NAME   tmux session (interactive if omitted)
  -b, --bind ADDR      Bind address (default: 0.0.0.0)
  -l, --list           List sessions
  -h, --help           Show help
```

## Examples

```bash
oatmux                    # Interactive picker
oatmux -s dev -p 3000     # Stream "dev" on port 3000
oatmux -b 127.0.0.1       # Local only
oatmux -l                 # List sessions
```

## Controls

**Session Picker:**
- `↑/↓` or `j/k` - Navigate
- `Enter` or `1-9` - Select
- `q` or `Esc` - Quit

## Build

```bash
./compile.sh             # Release build (default)
./compile.sh Debug       # Debug with sanitizers
./compile.sh install     # Install to /usr/local/bin
./compile.sh clean       # Clean build directory
```

## Requirements

- Linux (x86_64, ARM64, ARMv7)
- tmux
- OpenSSL
- CMake 3.10+

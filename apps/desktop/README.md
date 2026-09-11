# xblob Desktop

Desktop GUI shell for **xblob** — an exploratory, research-oriented Classic Xbox media inspection and preservation toolkit.

## Architecture

xblob Desktop is structured into strictly isolated architectural layers:

1. **React UI Layer (`apps/desktop/src`)**:
   - Built with React 19, TypeScript (strict mode), and Vite.
   - 100% offline and compliant with WCAG 2.2 AA accessibility standards.
   - Communicates solely through an abstract typed bridge interface (`BridgeContract`).
   - Supports both native Tauri IPC and an in-memory synthetic Mock mode for browser-based development.

2. **Rust Shell Adapter (`apps/desktop/src-tauri`)**:
   - Built with Tauri v2.
   - Contains **zero domain logic**. Functions strictly as an FFI bridge and IPC translator between Webview and the C ABI.
   - Enforces a restricted Content Security Policy (CSP) with arbitrary network, shell, and filesystem write access completely disabled.

3. **Core C ABI (`libs/c_api`)**:
   - Pure C11 ABI boundary (`xblob/c_api.h`) with ABI version 1.1.
   - Exposes opaque handles, explicit error codes, ABI version negotiation, UTF-8 safe buffers, and machine diagnostic preparation.
   - Traps all C++ exceptions (`noexcept` boundary).

4. **Core C++ Engine (`libs/bus`, `libs/memory`, `libs/loader`, `libs/machine`, `libs/formats`, `libs/io`, `libs/common`)**:
   - Safe parsing, hashing, and header inspection of XBE executables and XISO disc images.
   - Guest bus with synthetic register banks and MMIO routing.
   - IA-32 4 KiB paging virtual memory with TLB.
   - Pure transactional XBE loader with atomic rollback.
   - Deterministic machine session lifecycle management.

## Disclaimer & Scope

> **Important**: This application is strictly an educational and preservation-oriented media inspection and diagnostic tool. It **does not play, run, execute, or emulate commercial Xbox games, and contains no commercial BIOS or keys**. All interactive controls are strictly diagnostic and validation-oriented (no "Play" or "Run" buttons).

## System Requirements

- **Node.js**: 20.x or newer, with `npm` 10+
- **Rust**: 1.80+ (stable toolchain)
- **CMake**: 3.25+ and a C++20 compliant compiler (Clang 16+, GCC 13+, or MSVC 2022)
- **Linux Prerequisites**:
  ```bash
  sudo apt-get install -y libwebkit2gtk-4.1-dev build-essential curl wget file libxdo-dev libssl-dev libayatana-appindicator3-dev librsvg2-dev
  ```

## Development Commands

### Web Layer

```bash
cd apps/desktop

# Install dependencies
npm ci

# Run Vite dev server with synthetic mock bridge
npm run dev

# Type check
npm run typecheck

# Lint (ESLint strict, zero 'any' allowed)
npm run lint

# Run Vitest unit and accessibility tests (vitest-axe)
npm test

# Build production web bundle
npm run build
```

### Tauri Desktop Shell

```bash
cd apps/desktop/src-tauri

# Format check
cargo fmt --check

# Clippy linter
cargo clippy -- -D warnings

# Run Rust unit/integration tests
cargo test --locked
```

## Licensing

Distributed under the MIT License. All dependencies and bundled fonts/assets are local, open-source, and compatible with MIT.

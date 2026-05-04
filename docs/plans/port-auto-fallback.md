# Configurable port with auto-fallback

## Context
The server hard-coded its listening port via `#define PORT 8880` and aborted with a single error if `bind()` failed. Two annoyances followed: you couldn't change the port without recompiling, and a leftover instance (or any other process holding 8880) made the next launch fail outright. The goal was to (a) accept a `--port` flag and (b) keep working when that port is busy by automatically trying the next free one.

## Approach

- New `--port <n>` CLI flag, parsed alongside `--public` and `--lan`. Default 8880. Range-validated (1–65535) and rejected on non-numeric input with a clear error.
- Replaced the `PORT` macro with a runtime `int port` variable seeded from the CLI.
- Auto-fallback loop around `bind()`:
  - Try `bind()` with the current port.
  - On `EADDRINUSE` (POSIX) / `WSAEADDRINUSE` (Windows): increment and retry.
  - Cap retries at 20.
  - Other `bind()` errors (e.g. permission denied on a privileged port) fail immediately — no point looping.
- Updated startup log to print the actually-bound port. If it differs from the requested one, print an extra line:

```
port 8880 busy, using 8881 instead
serving . on http://localhost:8881/
```

## Cross-platform notes
- `errno` after `bind()` on POSIX vs. `WSAGetLastError()` on Windows — wrapped with `#ifdef _WIN32` to read the right error code (`WSAEADDRINUSE` vs `EADDRINUSE`).
- Added `#include <errno.h>` on the POSIX branch; `winsock2.h` already provides what's needed on Windows.

## Files
- `src/htserve.c` — argv loop, the `bind()` retry loop, startup log.

## Verification
- `htserve --port 9000` binds 9000.
- Two instances on `--port 18880` simultaneously: second logs `port 18880 busy, using 18881 instead`.
- `htserve --port 0` and `--port abc` rejected with `invalid --port: ...`, exit 1.
- `htserve --port 80` (non-root) fails immediately without the retry loop, since `EACCES`/`EPERM` is not retried.

## Source
Commit `93d8204` — "Add --port CLI option with auto-fallback when port is busy". The original Claude-managed plan file got reused for the next task; this file reconstructs the plan from the implemented diff plus the README and `docs/usage.html`.

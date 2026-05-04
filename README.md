# htserve

Minimal C99 static-file HTTP server. Single source file ([src/htserve.c](src/htserve.c)), no dependencies beyond the C standard library and the platform sockets API. Builds on Linux and Windows (MinGW-w64 cross-compile supported).

## Build

```sh
cmake -B build
cmake --build build
```

The binary is produced at `build/htserve`. For Windows cross-compile, see [cmake/](cmake/) and [scripts/](scripts/).

## Usage

```
htserve [--public <path>] [--port <n>] [--lan]
```

### Options

| Flag | Default | Description |
|---|---|---|
| `--public <path>` | `.` | Directory to serve. The server `chdir`s into it at startup. |
| `--port <n>` | `8880` | TCP port to listen on (1–65535). If busy, falls back to the next free port (up to 20 attempts). |
| `--lan` | off | Bind on `0.0.0.0` (LAN-exposed) instead of loopback only. |

### Examples

```sh
# Serve the current directory on http://localhost:8880/
./build/htserve

# Serve ./site on a custom port
./build/htserve --public ./site --port 9000

# Expose on the LAN
./build/htserve --lan
```

### Port auto-fallback

If the requested port is already bound by another process, `htserve` increments the port number and retries (up to 20 times). When this happens it prints, e.g.:

```
port 8880 busy, using 8881 instead
serving . on http://localhost:8881/
```

Errors other than `EADDRINUSE` (for example, permission denied on a privileged port) fail immediately without retrying.

## Features

- `GET` only; `HTTP/1.0` responses with `Connection: close`.
- Directory listing when no `index.html` is present (sorted: directories first, then case-insensitive by name; HTML files bolded).
- Built-in MIME types for common web assets (html, css, js, wasm, json, svg, png, jpg, gif, ico, txt).
- URL decoding and basic path-traversal protection (rejects `..`, `\`, `:` in the request path with `400 Bad Request`).
- One thread per connection (`pthread` on POSIX, `_beginthreadex` on Windows). 5-second `recv` timeout.

## Limitations

- No `HEAD`, `POST`, or any method other than `GET`.
- No keep-alive, no range requests, no compression, no TLS.
- No access logging beyond a single `GET /<path>` line per request on stdout.

Intended for local development and quick file sharing — not for production exposure.

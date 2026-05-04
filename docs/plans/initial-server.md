# Initial server

## Context
Needed a tiny zero-dependency HTTP static-file server for local development and quick file sharing — small enough to be a single C99 source file, portable to Linux and Windows, no external libraries beyond the platform sockets API.

## What was done
Single-file C99 HTTP server in `src/htserve.c` (~340 lines). Listens on port 8880, serves `GET` requests for static files from the current working directory. Features:

- `HTTP/1.0` responses with `Connection: close`.
- `GET` only; rejects other methods with `405`.
- One thread per connection (`pthread` on POSIX, `_beginthreadex` on Windows).
- Built-in MIME type table for common web assets.
- URL decoding and basic path-traversal protection (rejects `..`, `\`, `:`).
- Directory listing when no `index.html` is present, with HTML escaping and URL encoding.
- 5-second `recv` timeout per connection.

CMake build configured for both POSIX and Windows (linking `ws2_32` on Windows).

## Files
- `src/htserve.c`
- `CMakeLists.txt`

## Source
Commit `915d58b` — initial commit. This plan is reconstructed post-hoc from the commit; no contemporaneous plan-mode file existed.

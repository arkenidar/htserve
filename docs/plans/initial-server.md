# Plan: Minimal C99 static file HTTP server (`htserve`)

## Context
`c:\code\htserve` is empty. Goal: the smallest reasonable C99 program that serves files from the current directory over HTTP on port **8880**, intended purely as a stand-in for `file://` when a browser refuses local-file access (ES modules, `fetch`, WASM, CORS-sensitive assets). Built with **CMake + MinGW** (MSYS2 mingw64 confirmed at `C:\msys64\mingw64\bin\`). Self-contained `.exe`, statically linked. Source kept portable to POSIX with one `#ifdef _WIN32` shim, but Windows is the primary target.

**Explicit non-goals** (per user): no HEAD/POST/PUT, no keep-alive, no caching headers, no config flags. Just `GET <file>` → bytes, plus a minimal directory index when no `index.html` exists.

**Concurrency**: thread-per-connection (detached). Required because browsers open 6+ parallel connections per page; serial accept would stall ES-module graphs and large assets behind each other. No shared mutable state → no locking.

## Files

### 1. `c:\code\htserve\CMakeLists.txt`
```cmake
cmake_minimum_required(VERSION 3.15)
project(htserve C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_executable(htserve src/htserve.c)
if(WIN32)
  target_link_libraries(htserve PRIVATE ws2_32)
  target_link_options(htserve PRIVATE -static -static-libgcc)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(htserve PRIVATE Threads::Threads)
endif()
target_compile_options(htserve PRIVATE -Wall -Wextra -O2)
```

### 2. `c:\code\htserve\src\htserve.c`
Single C99 file, target ~150 lines. Structure:

- Socket + thread shim:
  - `_WIN32`: `<winsock2.h>`, `<ws2tcpip.h>`, `WSAStartup`, `closesocket`, `SOCKET`; `_beginthreadex` from `<process.h>` (preferred over `CreateThread` since the handler uses CRT `fopen`/`fread`).
  - else: `<sys/socket.h>`, `<netinet/in.h>`, `<unistd.h>`, `close`, `int`; `pthread_create` + `pthread_detach` (link `-lpthread`).
- `main`: `WSAStartup` (Windows), create TCP socket, `SO_REUSEADDR`, bind `0.0.0.0:8880`, listen, accept loop. Print `serving . on http://localhost:8880/` once. For each accepted client socket: `malloc` a small struct holding the socket handle, spawn a **detached** thread running `handle_conn`, immediately loop back to `accept`. Thread frees the struct and closes the socket on exit.
- `handle_conn(socket)`:
  1. `recv` up to 4 KiB or until `\r\n\r\n`.
  2. Require request to start with `GET `; otherwise close (no response needed for the use case).
  3. Extract path token between `GET ` and the next space; strip query string at `?`.
  4. URL-decode `%XX` in place.
  5. **Safety**: reject if path contains `..`, `\`, or `:` — reply `400` and close. Strip leading `/`.
  6. **Resolve target**: if the path is empty or ends with `/`, treat it as a directory request — first try `<dir>index.html`; if that fails, generate a directory listing (see below). Otherwise treat as a file.
  7. **File path**: `fopen(path, "rb")`. On failure → `404 Not Found`.
  8. `fseek`/`ftell` for size. Pick `Content-Type` from a small extension table (`.html .htm .css .js .mjs .json .wasm .svg .png .jpg .jpeg .gif .ico .txt .data`); default `application/octet-stream`.
  9. Send `HTTP/1.0 200 OK\r\nContent-Type: ...\r\nContent-Length: N\r\nConnection: close\r\n\r\n`, then stream the file in 64 KiB `fread`/`send` chunks.
  10. `closesocket` / `close`.

### Directory listing
- Use `<dirent.h>` + `opendir`/`readdir`/`closedir` everywhere — MinGW-w64 provides `dirent.h`, so no `#ifdef` is needed for this.
- Use `stat()` (`<sys/stat.h>`, available on both) to distinguish files from subdirectories.
- Build the HTML in a dynamically-grown buffer (start 8 KiB, `realloc` on demand), then send with proper `Content-Length`.
- Layout:
  ```
  <!doctype html><meta charset="utf-8"><title>/path/</title>
  <h1>/path/</h1>
  <ul>
    <li><a href="../">../</a>           (omitted at root)
    <li><a href="sub/">sub/</a>          (directories, trailing slash)
    <li><b><a href="page.html">page.html</a></b>   (.html / .htm in <b>)
    <li><a href="other.txt">other.txt</a>
  </ul>
  ```
- Sort entries: directories first, then files, each group alphabetically (case-insensitive). Use `qsort`.
- HTML-escape entry names (`& < > "` → entities) for the visible text.
- Percent-encode the `href` for safety on space, `?`, `#`, `%`, `&` (small helper, ~10 lines).
- `Content-Type: text/html; charset=utf-8`.

That is the entire program. No select, no fancy parsing.

## Build & run
```powershell
cd c:\code\htserve
cmake -G "MinGW Makefiles" -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
# then, in any directory you want to serve:
c:\code\htserve\build\htserve.exe
```

## Verification
1. Build clean under `-Wall -Wextra`.
2. `htserve.exe` runs on a vanilla Windows box with no MSYS on PATH (static link works).
3. `Invoke-WebRequest http://localhost:8880/CMakeLists.txt` returns 200 with matching bytes and correct `Content-Length`.
4. Loading an `index.html` that uses ES modules / `fetch` works in a browser (the original motivation — replaces `file://`).
5. `http://localhost:8880/../something` returns 400 (traversal blocked).
6. `http://localhost:8880/missing` returns 404.
7. `http://localhost:8880/` in a directory with no `index.html` shows a listing; subdirectories have trailing `/`, `.html`/`.htm` entries are bold, and `..` link appears for non-root directories.
8. Clicking a directory link navigates into it; clicking a file link serves it.

## Source
Curated from `~/.claude/plans/cmake-use-mingw-flickering-firefly.md`; implemented in commit `915d58b`.

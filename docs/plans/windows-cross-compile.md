# Windows cross-compile via MinGW-w64

## Context
The user wanted a Windows 11 executable of `htserve`. The source `src/htserve.c` was already cross-platform (it has `_WIN32` branches for `winsock2`, `_beginthreadex`, `_chdir`, etc.) and `CMakeLists.txt` already had a `WIN32` branch linking `ws2_32` with `-static -static-libgcc` so the resulting `.exe` runs without extra DLLs. The simplest path from Linux was to cross-compile with MinGW-w64 rather than build natively on Windows.

## Approach
Cross-compile a static x86_64 `htserve.exe` using `x86_64-w64-mingw32-gcc`.

### Toolchain
Install once on the build host:
```
sudo apt-get install -y gcc-mingw-w64-x86-64
```

### One-shot build (no CMake)
```
x86_64-w64-mingw32-gcc -std=c99 -O2 -Wall -Wextra \
    -static -static-libgcc \
    src/htserve.c -o build/htserve.exe -lws2_32
```

### CMake-based build (toolchain file)
```
cmake -S . -B build-win-cmake -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-win-cmake
```
Both produce a fully-static `htserve.exe` (~100–300 KB).

## Files
- `src/htserve.c` — no source changes needed (already had `_WIN32` branches).
- `CMakeLists.txt` — no changes needed.
- `cmake/mingw-w64-x86_64.cmake` — added: CMake toolchain file pinning compiler, `CMAKE_SYSTEM_NAME=Windows`, target triple, and search paths.
- `scripts/build-win.sh`, `scripts/build-win-cmake.sh`, `scripts/build-win.bat` — added: convenience wrappers.

## Verification
- `file build/htserve.exe` reports `PE32+ executable (console) x86-64, for MS Windows`.
- Optional smoke test under Wine: `wine build/htserve.exe --public src`, then `curl http://localhost:8880/` from another terminal.
- On real Windows 11: copy the `.exe`, run it from `cmd`/PowerShell, browse `http://localhost:8880/`. Windows Firewall may prompt on first `--lan` run.

## Source
Commit `a7efee4` — "Add Windows cross-compile via MinGW-w64". Curated from `~/.claude/plans/could-you-build-htserve-exe-synchronous-giraffe.md`.

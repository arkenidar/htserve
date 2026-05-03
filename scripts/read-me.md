# Build scripts

Helpers for building `htserve` for Windows. Source ([../src/htserve.c](../src/htserve.c)) is cross-platform; these scripts target Windows x86_64 and produce a fully static `.exe` (no extra DLLs needed on the target machine).

## Files

| Script | Host | What it does |
|---|---|---|
| [build-win.sh](build-win.sh) | Linux | Cross-compile via `x86_64-w64-mingw32-gcc` directly. Output: `build-win/htserve.exe`. |
| [build-win-cmake.sh](build-win-cmake.sh) | Linux | Same target, via CMake using [../cmake/mingw-w64-x86_64.cmake](../cmake/mingw-w64-x86_64.cmake). Output: `build-win-cmake/htserve.exe`. |
| [build-win.bat](build-win.bat) | Windows | Native build with MinGW-w64 (e.g. MSYS2). Output: `build-win\htserve.exe`. |

## Prerequisites

### Linux cross-compile (Debian/Ubuntu)
```
sudo apt-get install -y gcc-mingw-w64-x86-64
```
For the CMake variant also install `cmake`.

### Windows native
Install MinGW-w64, e.g. via MSYS2:
```
pacman -S mingw-w64-x86_64-gcc
```
Run `build-win.bat` from a shell where `gcc` is on `PATH` (MSYS2 MinGW64 shell, or plain `cmd` after adding the MinGW `bin` dir to `PATH`).

## Usage

From the repo root:
```
./scripts/build-win.sh           # direct gcc
./scripts/build-win-cmake.sh     # via CMake toolchain file
scripts\build-win.bat            # on Windows
```

## Verify

```
file build-win/htserve.exe
# PE32+ executable (console) x86-64, for MS Windows
```

Smoke-tested under Wine on Linux — works:
```
wine build-win/htserve.exe --public src &
curl http://localhost:8880/
```

## Run on Windows 11

Copy `htserve.exe` to any folder, open `cmd` or PowerShell there, and run:
```
htserve.exe --public path\to\site
htserve.exe --lan            # bind 0.0.0.0 (Windows Firewall will prompt)
```
Then browse `http://localhost:8880/`.

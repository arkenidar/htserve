# Bind mode: localhost vs 0.0.0.0 (`--lan`)

## Context
[src/htserve.c:361](src/htserve.c#L361) currently always binds `INADDR_ANY` (0.0.0.0), exposing the server on every interface — including the LAN — by default. The user wants a way to switch between localhost-only and all-interfaces mode. Safer default for a dev static server is localhost; LAN exposure should be opt-in.

## Approach
Add a flag `--lan` that binds `0.0.0.0`. Default (no flag) binds `127.0.0.1`. The flag composes with the existing `--public <path>` arg parsing.

### CLI grammar
- `htserve` → loopback, serves `.`
- `htserve --lan` → 0.0.0.0, serves `.`
- `htserve --public <path>` → loopback, serves `<path>`
- `htserve --lan --public <path>` (or `--public <path> --lan`) → 0.0.0.0, serves `<path>`
- anything else → usage error, exit 1

### Implementation in [src/htserve.c](src/htserve.c)
1. Replace the rigid `argc == 1 / argc == 3` parsing in `main` ([L332-L341](src/htserve.c#L332-L341)) with a small loop over `argv[1..argc-1]`:
   - `--lan` → set `int lan = 1`
   - `--public` → consume next arg into `public_path`; error if missing
   - else → usage error
   New usage string: `usage: htserve [--public <path>] [--lan]`.
2. At [L361](src/htserve.c#L361), choose bind address:
   ```c
   a.sin_addr.s_addr = htonl(lan ? INADDR_ANY : INADDR_LOOPBACK);
   ```
3. Update startup banner ([L367](src/htserve.c#L367)) to reflect the mode, e.g.:
   - loopback: `serving <path> on http://localhost:8880/`
   - lan: `serving <path> on http://0.0.0.0:8880/ (LAN-exposed)`

No new headers required — `INADDR_LOOPBACK` is in the same `<netinet/in.h>` / `<winsock2.h>` already included.

## Verification
1. `cmake --build build`.
2. `./build/htserve.exe` → banner shows `localhost`. From another machine on LAN, `curl http://<this-host-ip>:8880/` → connection refused. From this host, `curl http://localhost:8880/` → 200.
3. `./build/htserve.exe --lan` → banner shows `0.0.0.0`. LAN curl → 200.
4. `./build/htserve.exe --lan --public src` → loopback curl returns listing of `src/` only.
5. `./build/htserve.exe --bogus` / `./build/htserve.exe --public` → exit 1 with usage.

## Source
Curated from `~/.claude/plans/serve-directory-public-path-toasty-oasis.md`; implemented in commit `51d190e`.

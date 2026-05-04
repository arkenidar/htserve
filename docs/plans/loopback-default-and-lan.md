# Loopback default with opt-in --lan

## Context
The server originally bound to `INADDR_ANY` (`0.0.0.0`), making it reachable from any host on the LAN by default. For a development-grade tool with no authentication, no TLS, and minimal request validation, this is a footgun — running the server on a coffee-shop wifi exposes the served directory to every other client on that network.

## What was done
Changed the default bind address to `INADDR_LOOPBACK` (`127.0.0.1`), so by default the server is only reachable from the same machine. Added a `--lan` flag to opt into the previous behaviour (bind on `0.0.0.0` for LAN exposure).

Startup log now distinguishes the two cases:
- Default: `serving . on http://localhost:8880/`
- With `--lan`: `serving . on http://0.0.0.0:8880/ (LAN-exposed)`

## Files
- `src/htserve.c` — `lan` flag in argv parsing; `htonl(lan ? INADDR_ANY : INADDR_LOOPBACK)` at bind time; updated startup print.

## Source
Commit `51d190e` — "Default to loopback bind, add --lan to expose on 0.0.0.0". This plan is reconstructed post-hoc from the commit; no contemporaneous plan-mode file existed.

# --public path CLI option

## Context
The initial server hard-coded "serve files from the current working directory." Users had to `cd` into the target directory before launching, which was inflexible — you couldn't run the binary from one terminal location and serve a different folder.

## What was done
Added a `--public <path>` CLI flag. The server `chdir`s into the given path at startup (before binding the socket) and continues to serve relative to that directory. Default remains `.` so existing usage is unchanged.

Argument parsing kept minimal: a tiny loop over `argv` recognising the single flag, with a usage error on unknown arguments.

## Files
- `src/htserve.c` — argv loop in `main()`, plus the `chdir` call before `WSAStartup`/`socket`.

## Source
Commit `3ce09af` — "Add --public <path> CLI option to choose served directory". This plan is reconstructed post-hoc from the commit; no contemporaneous plan-mode file existed.

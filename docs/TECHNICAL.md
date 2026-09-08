# Architecture and compatibility

## Two separate layers

1. **Data preparation (public source):** Python's standard library reads the user's PC `data.win`, invokes
   the legacy converter and Steam-API adapter, verifies the result and copies audio into an SD staging folder.
2. **Switch execution (not provided):** the proprietary GameMaker NX runner executes VM bytecode. Its source
   is not in this project. `patch_nso.py` and `runner_exit.py` modify a user-provided, hash-pinned executable locally.

There is no native open-source NRO engine here. A Steam-to-SD wizard cannot solve runtime distribution rights.
For a genuinely standalone public port, obtain the necessary developer/rights-holder permissions or implement
an independently licensed compatible engine. Renaming an NSP or distributing a modified `main` is not a substitute.

## Data format

The supported PC layout is GameMaker 2024.11 VM (bytecode 17); target runner: 2024.14.3.260.
Room creation-order pointers were added in 2024.13; audio-group path strings in 2024.14.
The converter appends replacement pointer-referenced structures and updates their tables, leaving existing
bytecode addresses intact. `verify_data.py` checks the resulting loader-facing structures.

The initial validator checks bounded FORM chunks, game identifier, VM version, known room/group/sprite counts,
room pointer bounds and sound group references. This is **not** an exact whitelist of all Steam versions:
matching counts cannot prove bytecode compatibility. Later script/event lookups, conversion checks and the
output verifier are also mandatory. Unsupported cases fail closed. Do not run parsers on untrusted arbitrary files.

`patch_steam.py` changes five optional Steamworks call sites for the non-Steam platform. It preserves function
reference-chain operands while branching past unavailable achievement/status/shutdown work. It does not modify
the PC executable or implement Steam entitlement/DRM emulation.

A recognized previously converted SHA-256 is copied and reverified instead of patched twice. Unknown already
patched files may be rejected; use your original Steam `data.win`. Audio archives are copied unchanged. The
outer FORM size and copied hashes are checked; audio is not decoded or transcoded by the preparation utility.

## Transaction and privacy

All work occurs in a temporary folder beside a new output directory, followed by a rename after success.
Source files are hashed before/after, audio copies are checked, and concurrent updates cause rejection.
The destination may not overlap the Steam folder; an existing destination is never intentionally replaced.
The tool never writes directly to a mounted SD by default and never overwrites saves.
Generated logs have temporary paths redacted; manifests contain filenames and hashes, not Steam account details.

## Verification scope

- Public tests use synthetic FORM files, mock the legacy conversion subprocesses for orchestration tests,
  and exercise rejection, copying, hashing, atomic staging and output privacy. They do not contain game fixtures.
- Private ARM64 tests are opt-in with `SANAE_RUNTIME_MAIN`.
- The prior converted `game.win` was validated locally; the fixed quit path was confirmed by the user on Switch.
- The new end-to-end wizard has not been tested against a fresh, unconverted Steam installation here.
  Synthetic orchestration tests and validation of an already converted local file are not equivalent to that test.

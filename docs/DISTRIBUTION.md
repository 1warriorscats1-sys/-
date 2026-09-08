# Source-only distribution policy

## Allowed content in the current tree

Preparation/patch source, synthetic tests, documentation, configuration and notices. No copyrighted game assets,
PC game executables, converted game data, Nintendo/GameMaker runtime/SDK, encrypted packages or keys.
Do not add a download URL or workflow that fetches those materials from another repository.
A file being publicly downloadable is not evidence of permission to redistribute it.

Generated `sdcard-pack` contains the user's game data. A locally built exit ZIP contains a complete proprietary
runner. **Neither is a public release artifact.** Both are ignored by Git. `scripts/audit_distribution.py`
checks tracked filenames and file signatures as an additional safeguard, not a complete legal audit.

## Runtime and licensing limits

Steam ownership is a prerequisite for the user's game data, not a license for Nintendo/GameMaker Switch technology.
Users must independently have the rights needed for the runtime and any local modifications. This repository
supplies no acquisition instructions or protection-bypass keys. It is unofficial and not endorsed by the rights holders.
The absence of bundled assets does not by itself resolve every legal or contractual question; this is not legal advice.
See LICENSE.md and NOTICE.md for the narrower scope of the code license.

## Existing GitHub history is NOT erased by this cleanup

Earlier commits in this repository included `th08.exe`, `th10.exe` and `SANAE_exit_fix.zip` (a complete patched runner).
They have been removed from the **current tree**, not magically from Git history, caches, forks, old refs or downloads.
The session is restricted to the working branch `arena/01a080a0-repo`; other branches have not been rewritten or deleted.
In particular the legacy `main` branch may still retain old files. Do not advertise the entire historical repository
as free of third-party binaries. Use the current source tree, not old binary links.

Repository-owner follow-up for a fully source-only public repository:

1. Review and remove any binary release assets or attachments; do not republish the old exit ZIP.
2. With explicit approval, migrate to a fresh source-only history or rewrite all affected refs, including legacy main.
3. Coordinate cleanup of retained GitHub objects/cached views with GitHub Support where appropriate.
4. Ask holders of forks/copies to remove unauthorized material. A local history rewrite cannot remove their copies.
5. Confirm permission/license provenance for retained legacy conversion scripts before broadly relicensing them.

No force-push or deletion of other branches is performed by the cleanup scripts.

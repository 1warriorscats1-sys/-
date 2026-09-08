# Deferred exit — private runtime patch

The prior patch's quit behavior was reported working by the user on their Switch.
That is a device report for the tested build, not a guarantee across all firmware or game revisions.
The executable/ZIP used for that test is no longer distributed in the current source tree.

## Implementation

`game_end` sets a new private BSS flag and returns through the original epilogue.
The next platform-loop iteration performs the existing guarded save commit, then resolves
`nn::oe::ExitApplication` through `nn::ro::LookupSymbol` and calls it.
If lookup fails, returns NULL, or the SDK function unexpectedly returns, the fallback is
SVC 7 (`ExitProcess`), not C `exit(0)` and its destructor chain.

The BSS extent is increased in NSO and MOD0; no existing globals are borrowed.
Save stubs and all five file-close hooks match the previously working implementation byte for byte.
Return-to-title, audio-group, and deprecated-builtin fixes are retained.

## Private reproduction only

You must provide your **own lawfully obtained** original GameMaker NX 2024.14.3.260 ExeFS `main`.
The Steam installation does not contain it. No download source is provided.

Expected original SHA-256:
`dcd5b9fc9ca50bf61f781a6a381c2f149ba97f82f395b94fe8f89d6c05ef3a2c`

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements-patch.txt
SANAE_RUNTIME_MAIN="/your/runtime/main" .venv/bin/python -m unittest discover -s tests -v
.venv/bin/python scripts/build_exit_patch.py --main "/your/runtime/main"
```

The output defaults to `local-inputs/SANAE_exit_fix.zip` and is ignored by Git.
It contains a complete patched proprietary executable: **do not publish or redistribute it**.
This command does not build an NSP or provide encryption keys.
Only use it where your rights and applicable terms permit the modification.

To install privately, close the compatible game, back up its previous `exefs/main`, then merge the generated
`atmosphere` folder into the SD root. Do not touch `game.win` or saves. To roll back, restore that previous
file; if none existed, remove only the newly added `exefs/main`.

## Tests

11 optional private integration tests execute emitted ARM64 code in Unicorn: original epilogue,
normal loop, deferred exit, two load addresses, lookup failures, guarded commits, layout checks and
fail-closed instruction checks. SDK services are mocked; these tests alone cannot prove platform behavior.
Public tests use synthetic data and need neither the runtime nor the game.

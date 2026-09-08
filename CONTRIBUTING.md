# Contributing

Please keep contributions source-only. Never attach game files, audio, PC executables, Switch runtime/SDK,
keys, NSPs, complete patched runners, or generated SD packs to commits, pull requests, issues or releases.
Do not introduce scripts/workflows that download them from someone else's repository.

Before publishing:

```sh
python3 -m unittest discover -s tests -v
python3 scripts/audit_distribution.py
git diff --check
```

Public tests must use synthetic fixtures. Private integration tests must be optional, require explicit local
inputs, and skip in a normal clean checkout. Report runtime tests separately from source-only tests.
On Windows, test the folder picker and drag-and-drop launcher when changing launcher behavior.

Do not label this repository a native open-source engine or claim that Steam files alone are sufficient to
boot the game on Switch. Do not add a blanket license without resolving the legacy-file provenance in NOTICE.md.

Inspect bug reports for private paths and personal data before posting. A useful report contains the command,
OS/Python version, game version, error text and hashes — not the commercial data itself.

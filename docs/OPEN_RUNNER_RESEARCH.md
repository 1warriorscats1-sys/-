# Alternative runners checked — 2026-09-08

## Updated acceptance decision

The later hardware reports invalidate treating the initial smoke test as evidence
that this engine is ready for SANAE. The owner requires behavior equivalent to the
working NSP, not an indefinite sequence of per-screen repairs. Butterscotch remains
a research candidate; it is not an accepted replacement. Changing NRO/NSP packaging
cannot restore missing runtime semantics. See
[SANAE compatibility investigation](SANAE_GAMEPLAY_COMPATIBILITY.md).

## Earlier candidate selection (not gameplay acceptance)

Prioritize **Butterscotch's existing Switch backend**, rather than continue to base the public
port on a proprietary GameMaker NX runtime. It already offers the desired direct-data
architecture and can execute initial SANAE logic from the supplied original Steam file in
a host-side smoke test. Game compatibility work is still required.

This corrects the earlier implication that an independent engine would necessarily have to
be written from scratch. An existing licensed implementation can instead be extended.
See [the pinned implementation and actual test results](../open-runner/README.md).

## Projects compared

| Project / approach | Evidence | Fit for this request |
|---|---|---|
| **Butterscotch** | Independent C runner, AGPL-3.0; README lists WAD 17 and Nintendo Switch, directly loading `sdmc:/switch/butterscotch/data.win`. [2](https://github.com/ButterscotchRunner/Butterscotch) | Best candidate. Existing libnx backend; test the real SANAE game features, not just the WAD number. |
| **Cinnamon / Project Sunshine** | Fork of Butterscotch aimed at 3DS/Wii U, WAD 16/17. [1](https://github.com/Project-Sunshine-Native/cinnamon) | Confirms the independent-runner approach, but the main Butterscotch repository already has the Switch backend we need. |
| **OpenGM** | MIT, .NET/OpenTK runner. README says it makes an intermediate `data_OpenGM.win`; compatibility is incomplete. [2](https://github.com/misternebula/OpenGM) | Useful compatibility reference, less direct than an existing native Switch backend. Not a drop-in NRO for SANAE. |
| **OpenGML** | MIT GML 1.4 interpreter with experimental GML 2.0, primarily loading `.project.gmx`. [OpenGML README](https://github.com/maiple/opengml) | Not a demonstrated direct loader for this compiled GameMaker 2024.11 Steam data. |
| **gmloader / gmloader-next** | Linux/ARM compatibility layers for the Android `libyoyo.so` runner, not a reimplementation of the runner itself. [5](https://jantrueno.github.io/PortMaster-Wiki/contribute/porting/engines/gamemaker-studio/) | An open wrapper does not establish rights to distribute its closed Android runner, nor a ready Horizon/libnx NRO. |
| **Undertale Yellow via runner replacement** | The cited porting discussion describes replacing a Switch game's `game.win`, and identifies RussellNX as containing a modified GMS Switch runtime. [3](https://gbatemp.net/threads/play-port-your-gamemaker-games-on-nintendoswitch.519660/page-4) | Evidence that a port exists, not evidence of an independently licensed engine or of redistribution permission. |
| **sskyNS/Undertale-Yellow** | At inspection, the repository tree contained only README.md; the search result reports no published releases. [1](https://github.com/sskyNS/Undertale-Yellow) | Insufficient implementation/licensing evidence. Its claims are not used as technical proof. |

A community disclaimer (“non-profit”, “support the original”) does not substitute for actual
licenses. Conversely, an AGPL engine can be distributed subject to its license, with user-owned
game data supplied separately; it does not need to reuse the closed Nintendo/GameMaker runtime.
No general legal clearance for game trademarks, assets or all jurisdictions is asserted here.

## NRO vs NSP

Use an **NRO** as the first open-engine deliverable: the Switch backend already builds it with
libnx/devkitPro. The legal distinction is in its contents and licenses, not its extension.
An optional NSP launcher later would be a packaging/UI choice, not a way to legalize the old
closed runner. Keeping the old executable and calling it an NRO would not solve the problem.

## Supplied input

The temporary original `data.win` was copied into the ignored private input directory for
local testing. It must not be part of the public port/release. The current public tree removes
it again while preserving local access. Prior commits still contain the uploaded file; moving
the final project to another account will not erase that old copy or its history.

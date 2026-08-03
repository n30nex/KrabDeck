# KrabOS 1.0 RC1 integration manifest

Status: draft M2-M5 product checkpoint; not release-ready.

Integration office: Commodore <commodore@canadaverse.org>

Target: `main`

Base: `origin/main` at `49c09e324e7617f98679bc9fe3821a05391cafc0`

Head branch: `agent/commodore/rc1-integration`

This is the single RC1 convergence branch. It records source selection only;
exact-device, hardware, RF, release, tag, and merge gates remain pending.

## Lineage and overlap decisions

| PR | Decision | Reason |
|---|---|---|
| #11 | Deferred | Broad 101-file product candidate overlaps the M0/M1 foundation and focused fixes. Its unresolved M2–M5/UI scope is not safe to stack wholesale into the smallest RC1 checkpoint. |
| #12 | Superseded by #19 | Navigator’s recovery checkpoint is an independent, overlapping candidate; Captain’s M0/M1 reconciliation is the selected foundation. |
| #13 | Superseded by #19 | The fail-closed hardware-report implementation and test are already present in the selected Captain tree. |
| #14 | Accepted, focused | Exact-device runner label and contract coverage remain useful; apply to the final `krabos-edge.yml` without replacing Bulwark’s observer wiring. |
| #15 | Accepted, focused | Retain only the `radio_profile_at()` count-boundary test. The PR’s older radio-default assertions conflict with the selected Canada-first baseline. |
| #16 | Accepted | Disjoint security hardening: suppress SSIDs and local IPs in OTA/Wi-Fi logs. |
| #17 | Superseded by #19 | Hardware-admission documentation is already represented in the selected release-evidence document. |
| #18 | Accepted, workflow portion | The lock helper and its focused tests are already in #19. Retain the environment-aware routing in the non-edge workflows; preserve #22’s observer-aware edge workflow. |
| #19 | Accepted foundation | Smallest coherent M0/M1 reconciliation, including the retained release/evidence contracts and recovery roles. Original Captain authorship is preserved in the reused commit. |
| #20 | Superseded by #22 | Breakwater’s two commits are ancestors of #22 and are included once through that lineage. |
| #21 | Accepted, focused | One-shot production boot advert is a small, isolated M3 handoff; keep remote-test RF silence and the existing guarded `sendAdvert()` path. |
| #22 | Accepted lineage | Direct successor of #19 and #20; retains candidate-bound evidence handoff plus independent RF-observer admission wiring. Original Breakwater/Bulwark authorship is preserved in reused commits. |

## M2-M5 product overlap checkpoint

PR #11's four-commit candidate was not applied wholesale because it replaces
the current M0/M1 safety and evidence lineage and touches protected policy and
workflow paths. Architect applied the product hunks that close the documented
gap:

| Product area | Included checkpoint | Deferred or retained gate |
|---|---|---|
| M2 UI | Four primary touch surfaces (Chats, Map, Network, More), larger touch targets, and keyboard/trackball focus parity | Exact-device visual captures and memory soak |
| M3 client parity | Canada-first GPS/time defaults, local-time conversion, companion/OTA channel validation, and existing guarded one-shot production advert | Correlated DM/channel/RF evidence and companion interoperability |
| M4 maps/GPS/SD | NRCan/permitted XYZ downloader, HTTPS/PNG admission, `.part`/atomic writes, SD mutex/format-off path, map download dialog, and reboot/shutdown quiesce | Live GPS and offline reboot evidence on the exact T-Deck Plus |
| M5 contracts | Typed map/release/source checks and native contract coverage added without replacing candidate-bound evidence | Full current-SHA Actions matrix, exact-device admission, release signing, and publication |

The retained M0/M1 workflows, RF-off roles, private fixture binding, privacy
redaction, and candidate-bound evidence remain authoritative. No release
readiness is claimed by this checkpoint.

## Accepted commit provenance

The selected lineage reuses these source commits with their original authors;
cherry-pick committer identity is Commodore:

- #19: `7c14e417b2dd934a10ca3c198fc5b0df4b9ce237` — Captain
- #20/#22 ancestor: `b990b87` and `a72a80c4a33e772b60c26c8eb875ba6c472a336e` — Breakwater
- #22: `0c03fd14956c0498f28d92e01099cb1daeba144a` — Bulwark

Any conflict-resolution or partial-application commit in this branch is
authored and committed as Commodore.

## Checks and remaining gates

Before this manifest checkpoint, no local firmware build or long-running
verification was started. Historical PR checks were inspected but are not
reused as current-head evidence. After the draft PR exists, run bounded
focused contracts and native checks, then update this manifest and the draft
description with exact results. Hardware, serial/USB, BLE/GPIO/I2C/SPI, meshcli,
RF, flashing/reset, backups, merge, tag, and release actions are out of scope.

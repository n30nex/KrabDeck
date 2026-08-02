## KrabOS v1.0.0 release

Version/tag: `v1.0.0`

Candidate branch: `main`

Candidate commit: <!-- exact lowercase 40-character SHA -->

Stable workflow: `.github/workflows/krabos-edge.yml`

Pi 5 gate run: <!-- exact GitHub Actions run URL -->

Protected publication approval: <!-- krabos-v1-production deployment URL -->

Evidence admission run: <!-- exact krabos-evidence.yml run URL -->

Immutable evidence artifact: <!-- artifact ID, sha256: digest, and URL -->

Evidence bundle: <!-- reviewed issue comment and immutable CI artifact URLs -->

All machine-readable requirements below require a fresh `pass` record whose
`firmware_version` is `v1.0.0` and whose `candidate_commit` and
`production_image_sha256` match the exact candidate in every record. Hardware
records also require the peer version and test date. The stable evidence
validator does not accept `N/A`, prose waivers, checked-in version-only
evidence, or another candidate's bytes. Never include device IDs, public keys,
contact names, message text, Wi-Fi credentials, coordinates, serial paths,
private flash backups or keys.

## Candidate and Pi-only build admission

- [ ] The requested SHA equals the `main` remote head, workflow SHA and checked-out commit; the checkout is clean and includes initialized submodules.
- [ ] `release_channel=stable-v1.0.0`, `candidate_branch=main`, and the exact full candidate SHA were supplied to `krabos-edge.yml`.
- [ ] The path-bound `krabos-evidence.yml` admission artifact is pinned by source run ID, artifact ID and archive digest and binds every record to this commit and production image SHA-256.
- [ ] No open non-PR issue labelled `gate`, P0 or P1 exists.
- [ ] Native and sanitizer suites, release contracts, roadmap contract, dependency pin/lock checks, exact-device safety tests and first-party warning gates pass on the dedicated Pi job.
- [ ] `KrabOS_TDeckPlus` built the production candidate and app/merged/Web/Launcher aliases.
- [ ] `KrabOS_TDeckPlus_recovery` independently built `krabos-recovery-rf-off.bin` and its ELF.
- [ ] `KrabOS_TDeckPlus_debug` independently built `firmware-debug.bin` and `krabos-debug-rf-off.elf`.
- [ ] Debug and recovery are retained as separate roles and bytes; neither image was renamed or substituted for the other.

## Exact-device and recovery gate

- [ ] Candidate and recovery flash manifests name only admitted exact-SHA files and their size/SHA-256 values were rechecked immediately before use.
- [ ] The pinned T-Deck Plus identity, 16 MiB flash and security state passed without probing or touching neighbouring serial devices.
- [ ] Pre-flash full capture and privacy-safe state export completed before erase; restoration receipts remained runner-local.
- [ ] Candidate erase/write/readback verification and USB reconnection passed.
- [ ] The candidate completed the 900-second smoke with exactly one verified boot advert, no Public chat messages and the required outbound proof.
- [ ] The separate recovery image was actually flashed, remained RF-off for its 60-second drill, and the exact candidate was restored and revalidated afterwards.
- [ ] `krabos-public-receipt.json` is bound to the candidate commit and both manifests; all twelve canonical gates are exactly `true`, `recovery.used=true`, and `recovery.ok=true`.
- [ ] The canonical redaction validator found no supplied secret or private device material in public artifacts.

## Companion interop

- [ ] `INT-BLE` — official app scenarios A-J pass over BLE, including bond/reconnect, sync, messaging, time, login/CLI, identity, and malformed frames. Evidence: <!-- HTTPS link -->
- [ ] `INT-USB` — official `meshcore.js` companion USB matrix passes against both pinned stock peer and this release. Evidence: <!-- HTTPS link -->
- [ ] `INT-GOLDEN` — the pinned stock companion golden-frame corpus passes. Evidence: <!-- HTTPS link -->
- [ ] `BLE-BOND-REVOKE` — a removed BLE bond cannot resume an administrative session without pairing again. Evidence: <!-- HTTPS link -->
- [ ] `RESET-RECONNECT` — after factory reset, old companion credentials/bonds cannot silently reconnect. Evidence: <!-- HTTPS link -->

## Soak

- [ ] `SOAK-IDLE` — the dedicated debug build has a privacy-safe idle report with at least 120 samples spanning at least 600 seconds. Report: <!-- HTTPS link -->
- [ ] `SOAK-ACTIVE` — the dedicated debug build has a privacy-safe active navigation/chat/map/companion report with at least 120 samples spanning at least 600 seconds. Report: <!-- HTTPS link -->

## OTA matrix

- [ ] `OTA-UPGRADE` — authenticated update installs, reboots, reports `v1.0.0`, and preserves settings/stores. Evidence: <!-- HTTPS link -->
- [ ] `OTA-NO-UPDATE` — no-new-release result does not mutate flash or state. Evidence: <!-- HTTPS link -->
- [ ] `OTA-AUTH-FAIL` — missing/rejected credentials produce a bounded error and leave the device bootable. Evidence: <!-- HTTPS link -->
- [ ] `OTA-DOWNLOAD-FAIL` — offline/TLS/404/truncated transfer produces a bounded error and leaves the device bootable. Evidence: <!-- HTTPS link -->
- [ ] `OTA-IMAGE-FAIL` — invalid image/write failure does not replace the bootable image. Evidence: <!-- HTTPS link -->
- [ ] `OTA-SIGNATURE` — device-side publisher-signature status is recorded; a checksum or GitHub publisher attestation is not submitted as device verification. Evidence: <!-- HTTPS link -->
- [ ] `OTA-DOWNGRADE` — an image with an older security epoch is rejected before activation. Evidence: <!-- HTTPS link -->
- [ ] `OTA-TLS-ROTATION` — current/alternate maintained roots work, an expired or untrusted chain fails closed, and offline recovery is exercised. Evidence: <!-- HTTPS link -->

## Launcher matrix

- [ ] `LAUNCH-DETECT` — Launcher installation is detected and displayed using the exact `KrabOS-tdeck-plus-launcher.bin`. Evidence: <!-- HTTPS link -->
- [ ] `LAUNCH-HANDOFF` — exit/relaunch hands control back to Launcher without a reset loop. Evidence: <!-- HTTPS link -->
- [ ] `LAUNCH-OTA-GATE` — firmware self-OTA is unavailable while Launcher owns updates. Evidence: <!-- HTTPS link -->
- [ ] `LAUNCH-STATE` — settings, contacts, channels and messages survive relaunch. Evidence: <!-- HTTPS link -->

## Release artifacts and publication

- [ ] `REL-ARTIFACTS` — production app/merged images, four Web components, full and both Launcher aliases, dedicated debug and recovery images, ELFs, exact flash manifests, redacted receipt, web manifest/build metadata, SBOM, licences and release evidence all exist and satisfy their byte/offset/hash contracts.
- [ ] `REL-WARNINGS` — no first-party compiler-warning fingerprint exceeds its checked-in budget.
- [ ] `krabos-bundle-manifest.json` covers the exact regular-file set, and `SHA256SUMS.txt` is complete, ordered and current.
- [ ] Protected environment `krabos-v1-production` approved exactly `v1.0.0` only after the Pi gate succeeded.
- [ ] Tag `v1.0.0` and the non-draft, non-prerelease GitHub release target the exact candidate commit; no existing asset was overwritten with different bytes.
- [ ] Cosign verifies `SHA256SUMS.sigstore.json` against `https://github.com/n30nex/KrabDeck/.github/workflows/krabos-edge.yml@refs/heads/main` and the GitHub Actions OIDC issuer.
- [ ] `gh attestation verify` passes for every downloaded asset with repo `n30nex/KrabDeck`, the `krabos-edge.yml` signer workflow, `refs/heads/main`, the full candidate SHA, and `--deny-self-hosted-runners`.
- [ ] A fresh download of the published release passes `bundle_release.py verify`, checksum signature, provenance attestations and exact GitHub release metadata checks.

## Exceptions and residual risk

<!-- Link follow-up issues for residual non-blocking risk. Any missing or failed
inventory item, exact-device gate, recovery gate, P0/P1 issue, protected
approval, signature or publication verification blocks v1.0.0. -->

# KrabOS v1.0 release evidence

Every KrabOS v1.0 release PR uses
`.github/PULL_REQUEST_TEMPLATE/release.md`. The machine-readable inventory in
`ci/release_evidence_requirements.json` is the source of truth; the contract
check fails if any requirement disappears from the template. For `v1.0.0`,
every inventory item must have a fresh passing record. The evidence validator
does not accept prose waivers or `N/A` outcomes.

## Publication gate

A stable release never accepts a checked-in `release-evidence/v1.0.0.json`.
Such a file changes the commit it tries to describe, while a version-only file
can be reused for unrelated candidate bytes. The stable gate instead consumes
one out-of-tree schema-3 evidence packet from an immutable GitHub Actions
artifact. The source artifact is pinned by repository, source run ID, artifact
ID, exact artifact name and `sha256:` archive digest.

Evidence admission is a separate, callable workflow:

1. Dispatch `krabos-edge.yml` from `main` with `release_channel=validation`,
   `candidate_branch=main`, and the exact candidate SHA. Record the successful
   run ID plus the ID and digest of `krabos-validation-<candidate-sha>`. This
   validation channel compiles with the same deterministic `v1.0.0` identity
   as stable so the production firmware bytes are reproducible across runs.
2. Prepare `release-evidence-input.json` outside the repository. Its top-level
   identity and every requirement record must repeat the exact 40-character
   candidate commit and SHA-256 of that validation artifact's
   `firmware-merged.bin`.
3. Dispatch `krabos-evidence.yml` from that same `main` commit. Supply the
   validation run/artifact identities, the base64-encoded JSON packet, and the
   SHA-256 of its decoded bytes. Do not put secrets or private device data in a
   workflow input. The hosted job admits only a successful, non-PR, internal
   `krabos-edge.yml` validation run from `main`; revalidates the downloaded Pi
   build; checks the packet against its production image; and uploads exactly
   `krabos-v1-evidence-<candidate-sha>` with 90-day retention.
4. Record the successful evidence run ID, evidence artifact ID, and its exact
   `sha256:` digest from the Actions API. Supply those three values to the
   stable dispatch. Names alone are not accepted.

Both source runs must still exist, be unexpired, be no older than 30 days, and
identify the exact trusted workflow paths. A source run from any other manual
workflow, fork, branch, pull request, commit or repository fails closed.

`.github/workflows/krabos-edge.yml` owns this release. It is manually dispatched
from `main` with `release_channel=stable-v1.0.0`,
`candidate_branch=main`, the exact 40-character `candidate_sha`, and the three
immutable evidence artifact inputs. The Pi 5 job rejects any mismatch between
the requested SHA, workflow SHA, remote branch head and checked-out source
before building or touching the fixture. The legacy
`.github/workflows/build-release.yml` does not build, gate, tag or publish
KrabOS `v1.0.0`.

The Pi 5 job builds three separate environments:

| Environment | Role | Public image |
|---|---|---|
| `KrabOS_TDeckPlus` | Production candidate | `firmware.bin`, `firmware-merged.bin`, `krabos-candidate.bin` and web/Launcher aliases |
| `KrabOS_TDeckPlus_recovery` | RF-off recovery drill | `krabos-recovery-rf-off.bin` |
| `KrabOS_TDeckPlus_debug` | Diagnostic build | `firmware-debug.bin` |

Recovery and debug are independent builds with different release roles. A
debug image is never evidence that recovery passed, and a recovery image must
never be renamed or substituted as `firmware-debug.bin`.

After the production artifact directory passes its manifest, metadata, image
layout and digest contract, the stable run downloads the evidence packet by
artifact ID from its exact completed source run. It validates the Actions API
metadata and archive digest, then generates `release-evidence.json` as a
post-build attestation. The attestation retains the immutable source artifact
identity, SHA-256 of the packet bytes, exact candidate commit, production image
SHA-256, every exact-bound requirement record, and hashes of the complete
validated artifact contract.
The final sealed bundle additionally binds the admitted candidate/recovery
flash manifests and canonical redacted hardware receipt to the same commit and
bytes. SHA-256 values prove byte consistency, not publisher identity:
**checksums are not signatures**. See
[Security model](SECURITY_MODEL.md#firmware-update-trust).

Use this shape (repeat the requirement object for every ID in the requirements inventory):

```json
{
  "schema_version": 3,
  "kind": "krabos-exact-release-evidence-input",
  "candidate_commit": "<full-lowercase-40-character-sha>",
  "tag": "v1.0.0",
  "generated_at": "<ISO-8601 UTC timestamp>",
  "production_image_sha256": "<firmware-merged.bin-sha256>",
  "requirements": [
    {
      "id": "INT-BLE",
      "outcome": "pass",
      "evidence_url": "<reviewed-evidence-url>",
      "tested_at": "<YYYY-MM-DD>",
      "firmware_version": "v1.0.0",
      "candidate_commit": "<same-full-candidate-sha>",
      "production_image_sha256": "<same-firmware-merged.bin-sha256>",
      "peer_version": "official-client-version"
    }
  ]
}
```

Validate the out-of-tree packet against the downloaded Pi validation build
before admission:

```bash
python3 scripts/verify_release_evidence.py \
  --evidence /outside/release-evidence-input.json \
  --commit "<candidate-sha>" --tag v1.0.0 \
  --artifacts-dir artifacts --require-exact
```

After flattening the release build into `artifacts/`, generate and immediately
recheck the byte-bound attestation:

```bash
python3 scripts/verify_release_evidence.py \
  --evidence /outside/release-evidence-input.json \
  --commit "$(git rev-parse HEAD)" --tag v1.0.0 \
  --artifacts-dir artifacts \
  --source-repository n30nex/KrabDeck \
  --source-run-id "<evidence-run-id>" \
  --source-artifact-id "<evidence-artifact-id>" \
  --source-artifact-digest "sha256:<archive-digest>" \
  --source-artifact-metadata /tmp/evidence-artifact.json \
  --source-run-metadata /tmp/evidence-run.json \
  --require-exact \
  --write-attestation artifacts/release-evidence.json
python3 scripts/verify_release_evidence.py \
  --attestation artifacts/release-evidence.json \
  --commit "$(git rev-parse HEAD)" --tag v1.0.0 \
  --artifacts-dir artifacts \
  --source-repository n30nex/KrabDeck \
  --source-run-id "<evidence-run-id>" \
  --source-artifact-id "<evidence-artifact-id>" \
  --source-artifact-digest "sha256:<archive-digest>" \
  --require-exact
```

The production byte attestation covers the exact files enforced by
`scripts/release_artifact_contract.py`:

- `firmware.bin`, `firmware-merged.bin`, and the separately built
  `firmware-debug.bin`;
- `KrabOS-tdeck-plus-launcher.bin`;
- `manifest.json` and `build-metadata.json`;
- `krabos-tdeck-plus-bootloader.bin`,
  `krabos-tdeck-plus-partitions.bin`,
  `krabos-tdeck-plus-boot_app0.bin`, and
  `krabos-tdeck-plus-firmware.bin`;
- the byte-identical production aliases `krabos-tdeck-plus-full.bin` and
  `krabos-tdeck-plus-launcher.bin`.

The sealed public bundle then requires those files plus `firmware.elf`,
`krabos-candidate.bin`, the dedicated recovery image and ELF,
`candidate-flash-manifest.json`, `recovery-flash-manifest.json`,
`krabos-public-receipt.json`, `firmware-sbom.cdx.json`, and
`krabos-licenses.tar.gz`. Stable publication also includes the generated
`release-evidence.json`, `krabos-bundle-manifest.json`, `SHA256SUMS.txt`, and
`SHA256SUMS.sigstore.json`; the debug ELF is published as
`krabos-debug-rf-off.elf`.

`firmware-merged.bin`, both Launcher aliases and
`krabos-tdeck-plus-full.bin` must be byte-identical production images.
`firmware-debug.bin` and `krabos-recovery-rf-off.bin` are not members of that
alias set. Legacy schema-1 evidence remains readable only for historical
non-stable tags. Schema 2 version-only source evidence is rejected, and the
`v1.0.0` workflow passes `--require-exact` for both its schema-3 input and
schema-3 published attestation.

The successful Pi gate uploads the complete sealed bundle once and exports its
immutable artifact ID and archive digest to the publish job. Publication
downloads that ID from the same workflow run, not the newest artifact with a
matching name. If publication stops after creating a tag or uploading only
some assets, use **Re-run failed jobs** on that original workflow run. This
reuses the frozen bundle, including its random hardware challenge and receipt
bytes. Starting a new stable dispatch after partial publication creates a new
receipt and intentionally cannot replace or clobber existing immutable assets.

After the Pi gate, exact-device receipt, sealed-bundle verification and
protected `krabos-v1-production` approval pass, the GitHub-hosted publish job
hashes every sealed input into `SHA256SUMS.txt`. It uses GitHub OIDC to create
`SHA256SUMS.sigstore.json` and build-provenance attestations. Verify the workflow
identity before trusting the checksum file:

```bash
cosign verify-blob \
  --bundle SHA256SUMS.sigstore.json \
  --certificate-identity \
    'https://github.com/n30nex/KrabDeck/.github/workflows/krabos-edge.yml@refs/heads/main' \
  --certificate-oidc-issuer https://token.actions.githubusercontent.com \
  SHA256SUMS.txt
sha256sum --check SHA256SUMS.txt
```

GitHub build-provenance attestations bind the same files to the `main` workflow
and exact candidate SHA. Verify each downloaded asset, substituting that full
40-character SHA:

```bash
gh attestation verify <artifact> \
  --repo n30nex/KrabDeck \
  --signer-workflow \
    https://github.com/n30nex/KrabDeck/.github/workflows/krabos-edge.yml \
  --source-ref refs/heads/main \
  --source-digest <full-candidate-sha> \
  --deny-self-hosted-runners
```

`--deny-self-hosted-runners` is intentional: firmware bytes are built and
physically tested on the dedicated Pi, while publisher attestations are issued
only by the GitHub-hosted publish job after it re-verifies the sealed bundle.
These publisher signatures do not enable ESP32-S3 secure boot or device-side
signed OTA verification; those require a separate device-key provisioning and
rollback policy.

### Exact-device and recovery receipt

The stable bundle is ineligible unless `krabos-public-receipt.json` is the
canonical redacted schema, is bound to the candidate SHA and admitted candidate
and recovery bytes, and reports every required gate as exactly `true`:

- manifest validity, exact-device binding, pre-flash capture and state export;
- byte-verified flash and USB reconnection;
- the 900-second candidate smoke, candidate RF-policy binding and exactly one
  verified boot advert with no Public chat traffic;
- an actually exercised 60-second RF-off recovery drill, followed by restored
  candidate readiness;
- secret redaction.

The receipt's `recovery.used` and `recovery.ok` values must both be `true`.
Private flash backups, device identifiers, credentials, coordinates, serial
paths and raw logs remain runner-local and must not enter artifacts, issues or
the release. `scripts/krabos/bundle_release.py seal` revalidates the receipt,
both flash manifests and their referenced bytes before it creates the bundle
manifest and checksum list.

## Dependency pin policy

Every build dependency must resolve without local Git configuration:

- `lib/meshcore` is an exact gitlink. Its object must be publicly fetchable
  from the URL committed in `.gitmodules`; a maintainer-owned read-only mirror
  is acceptable, but `insteadOf` rules, unpublished fork objects, and deleted
  branches are not.
- PlatformIO application libraries use exact registry versions or immutable
  upstream commit archives. Branch names, version ranges, and mutable release
  downloads are not accepted as build pins.
- `ci/platformio-packages.lock` records the resolved package graph. A dependency
  PR reviews the declared pin, resolved graph, license/security inventory,
  affected firmware builds, and native tests together.

Normal build/test jobs key the shared immutable-download cache from the complete
declared and resolved dependency inputs. The dependency-refresh job is the one
exception: it restores a pre-update bootstrap cache before changing the selected
pin, then regenerates `ci/platformio-packages.lock`; the resulting PR is built
and tested by normal CI using the new complete hash.
The security inventory job also bypasses the package cache intentionally so its
scheduled dependency resolution observes the current registries and audit data;
it does not produce release build artifacts.

Before merging a gitlink change, initialize it in a clean clone rather than
relying on objects already present in a developer checkout:

```bash
clean_root="$(mktemp -d)"
git clone --recurse-submodules \
  https://github.com/n30nex/KrabDeck.git \
  "$clean_root/KrabDeck"
git -C "$clean_root/KrabDeck/lib/meshcore" rev-parse HEAD
```

The printed SHA must equal the superproject gitlink. Then run the companion
verification below and the affected build matrix. A submodule update is not a
hash-only maintenance change: reconcile local MeshCore patches and contact,
path, room-connection, transport-key, and persistence behaviour explicitly.

## Companion interop and golden frames

Run the official USB client matrix as documented in `scripts/official-meshcore-client-test/README.md` and the BLE scenarios in `docs/COMPANION_BLE_TEST_ENV.md`. Record firmware versions, date, pass/fail per case, and links to sanitized output. Do not record device IDs, public keys, contact names, message contents, credentials, or private keys.

The checked-in golden corpus is tied to the exact MeshCore submodule commit and SHA-256 of stock `examples/companion_radio/MyMesh.cpp`:

```bash
python3 scripts/verify_companion_golden_frames.py
```

The corpus also records a full public-upstream commit, its stock source digest,
and a canonical digest of all numeric `CMD_*`, `RESP_*`, `PUSH_*`, and
`ERR_*` values. To review a newly fetched public-upstream candidate:

```bash
git -C lib/meshcore fetch https://github.com/meshcore-dev/MeshCore.git main
python3 scripts/verify_companion_golden_frames.py \
  --candidate-ref FETCH_HEAD --json
```

The command resolves and prints the candidate's immutable full SHA. It fails if
the candidate changes any numeric protocol code and additionally verifies the
recorded source digest when the candidate is the reviewed-upstream commit.
New or changed frame layouts still require source-diff review and corresponding
golden/native assertions; numeric-code compatibility alone is not a layout
claim.

When either the submodule or reviewed-upstream baseline changes, review stock
protocol constants and frame layouts before updating the commit, source digest,
protocol digest, and corpus. A hash-only update is not review. Run both the
pinned and candidate commands plus:

```bash
pio test -e native_test -f test_companion_protocol -v
pio test -e native_sanitize -f test_companion_protocol -v
```

The protocol module includes deterministic stateful offline-sync coverage that
combines repeated app starts, bounded-page refill, write rejection, disconnect,
reconnect, retry, ordering, and durable delivered markers. This is the required
property/sequence gate for dependency refreshes; focused tests remain required
for any newly introduced state machine.

## Privacy-safe soak reports

Use the dedicated `KrabOS_TDeckPlus_debug` build at verbosity level 2 and
capture at least 10 minutes (at least 120 `[stat]` samples spanning 600
seconds) for each scenario. This diagnostic build is separate from
`KrabOS_TDeckPlus_recovery`; recovery output cannot satisfy a debug soak. Idle
means the home screen with radio and configured transports running. Active
means repeated screen navigation, message send/receive, map use, and companion
reconnects representative of normal operation. These privacy-safe heap soaks
are additional to the exact production candidate's 900-second hardware smoke.

```bash
python3 scripts/analyze_soak_log.py idle.log \
  --scenario idle --json-out idle.json --markdown-out idle.md
python3 scripts/analyze_soak_log.py active.log \
  --scenario active --json-out active.json --markdown-out active.md
```

The reports contain only numeric memory summaries and a source-log digest. Attach the reports, not raw serial logs. Review raw logs locally for secrets before retaining them. Defaults fail on fewer than 120 samples, a span shorter than 600 seconds, non-monotonic timestamps, heap range over 16 KiB, or first-to-last heap drop over 8 KiB.

## OTA matrix

Test the final release artifact and record evidence for:

- successful authenticated upgrade with settings and stores preserved;
- no update available;
- missing and rejected credentials;
- offline, TLS, HTTP error, and truncated download paths;
- invalid image or write failure, followed by a clean reboot into a known-good image.
- an image with a lower security epoch, rejected before activation;
- the current and alternate maintained TLS roots, plus an expired/untrusted
  chain and the documented offline recovery path;
- an explicit firmware-signature result. Until device-side signature checking
  exists, record this as a known gap; a passing checksum is not a substitute.

Companion security evidence also covers BLE bond revocation and verifies that
factory reset prevents an old bond from silently reconnecting. USB evidence
must label the connected host as trusted because companion USB has no protocol
authentication. See [Security model](SECURITY_MODEL.md#release-evidence).

Use versioned release URLs. A negative test passes only when the error is bounded, the UI remains usable or reboots intentionally, and a bootable image is retained.

## Launcher matrix

Follow `docs/LAUNCHER.md` with the exact
`KrabOS-tdeck-plus-launcher.bin` from the candidate bundle. Verify environment
detection, update ownership/OTA gating, handoff back to Launcher, relaunch, and
persistence of settings, contacts, channels, and messages. Record Launcher and
KrabOS versions without recording a device identifier.

## Release artifacts and warnings

The Pi job in `.github/workflows/krabos-edge.yml` runs the native and sanitizer
gates, verifies the resolved PlatformIO graph, builds all three KrabOS
environments, validates manifest identity, image layouts, exact production
aliases, offsets, provenance and SHA-256 values, and then verifies the
out-of-tree exact evidence packet before hardware access. The publish job then
re-verifies the sealed exact-tested bundle before tag creation and verifies the
downloaded GitHub release after upload. Confirm both job results in the release
PR. The first-party warning gate parses only compiler warnings whose paths
start with `src/`; third-party warnings do not spend this budget. When a
warning is fixed, reduce `ci/first_party_warnings.json` in the same PR so the
debt cannot return.

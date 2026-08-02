# KrabOS T-Deck Plus release artifacts

KrabOS binaries are published only as immutable, versioned assets on the
[KrabDeck Releases page](https://github.com/n30nex/KrabDeck/releases). The
stable v1 release tag is `v1.0.0`.

> [!WARNING]
> These images target the **LILYGO T-Deck Plus only**. Do not flash them to a
> T-Deck Pro, Seeed Indicator, generic ESP32-S3 or another board. Verify the
> release signature, checksums, exact tag and hardware identity before install.

`.github/workflows/krabos-edge.yml` is the sole owner of the KrabOS v1 release.
Its dedicated Pi 5 job builds and physically tests the exact candidate before a
separate GitHub-hosted job signs and publishes the sealed bytes. The inherited
`build-release.yml` workflow does not produce KrabOS `v1.0.0`.

## Production install images

| File | Role | Flash offset |
|---|---|---:|
| `firmware-merged.bin` | Complete standalone production image | `0x0` |
| `firmware.bin` | Production app-only update for an already compatible layout | `0x10000` |
| `KrabOS-tdeck-plus-launcher.bin` | Production image for bmorcelli/Launcher installation | Launcher-managed |
| `krabos-tdeck-plus-full.bin` | Complete production image used by Web/ESP flashing tools | `0x0` |
| `krabos-tdeck-plus-launcher.bin` | Lowercase Web/Launcher alias | Launcher-managed |
| `krabos-candidate.bin` | Exact production merged image admitted to the automated hardware gate | Automation evidence |

The release validator requires `firmware-merged.bin`,
`KrabOS-tdeck-plus-launcher.bin`, `krabos-tdeck-plus-full.bin`, and
`krabos-tdeck-plus-launcher.bin` to be byte-identical valid ESP32-S3 merged
images with the canonical DIO header. `firmware.bin` must be byte-identical to
`krabos-tdeck-plus-firmware.bin`.

## Web-flasher parts

`manifest.json` names exactly these four production components:

| File | Offset |
|---|---:|
| `krabos-tdeck-plus-bootloader.bin` | `0x0000` |
| `krabos-tdeck-plus-partitions.bin` | `0x8000` |
| `krabos-tdeck-plus-boot_app0.bin` | `0xe000` |
| `krabos-tdeck-plus-firmware.bin` | `0x10000` |

The manifest must identify `KrabOS T-Deck Plus`, version `v1.0.0`, one
`ESP32-S3` build and `new_install_prompt_erase=true`. The companion
`build-metadata.json` binds the files to `KrabOS_TDeckPlus`, the full candidate
Git SHA, pinned MeshCore SHA, clean source state, partition table, sizes,
offsets and SHA-256 values.

Do not combine components from different tags or substitute a locally built
file. The merged image must contain those exact component bytes at the listed
offsets.

## Recovery and debug are separate

| File | Built by | Purpose |
|---|---|---|
| `krabos-recovery-rf-off.bin` | `KrabOS_TDeckPlus_recovery` | Dedicated RF-off recovery drill image |
| `krabos-recovery-rf-off.elf` | `KrabOS_TDeckPlus_recovery` | Recovery symbols |
| `firmware-debug.bin` | `KrabOS_TDeckPlus_debug` | Full diagnostic image for controlled testing |
| `krabos-debug-rf-off.elf` | `KrabOS_TDeckPlus_debug` | Debug symbols |

The recovery image is not a normal install image. The automated gate actually
flashes it, proves RF remains blocked, and restores the exact production
candidate. The debug image is independently built and is not a recovery alias.
Neither role can be used as evidence for the other.

## Evidence and supply-chain files

The stable bundle also contains:

| File | Purpose |
|---|---|
| `firmware.elf` | Production symbols for the exact candidate |
| `candidate-flash-manifest.json` | Exact admitted production bytes, addresses, sizes and hashes |
| `recovery-flash-manifest.json` | Exact admitted recovery bytes, addresses, sizes and hashes |
| `krabos-public-receipt.json` | Canonical redacted exact-device and recovery result |
| `release-evidence.json` | Checked-in v1 evidence bound to the candidate commit and production bytes |
| `krabos-bundle-manifest.json` | Size and SHA-256 record for the complete sealed regular-file set |
| `SHA256SUMS.txt` | Ordered checksums for every sealed input and bundle manifest |
| `SHA256SUMS.sigstore.json` | Sigstore bundle signing `SHA256SUMS.txt` with GitHub OIDC |
| `firmware-sbom.cdx.json` | CycloneDX firmware dependency inventory |
| `krabos-licenses.tar.gz` | Project and bundled dependency licence texts |

Private flash backups, state exports, device identifiers, credentials,
coordinates, serial paths and raw logs are deliberately excluded.

## Verify before installing

Download all release assets into one directory. Verify the checksum signature
against the exact workflow identity:

```bash
cosign verify-blob \
  --bundle SHA256SUMS.sigstore.json \
  --certificate-identity \
    'https://github.com/n30nex/KrabDeck/.github/workflows/krabos-edge.yml@refs/heads/main' \
  --certificate-oidc-issuer https://token.actions.githubusercontent.com \
  SHA256SUMS.txt
sha256sum --check SHA256SUMS.txt
```

Then verify GitHub provenance for each asset, substituting the release's exact
40-character candidate SHA:

```bash
gh attestation verify <artifact> \
  --repo n30nex/KrabDeck \
  --signer-workflow \
    https://github.com/n30nex/KrabDeck/.github/workflows/krabos-edge.yml \
  --source-ref refs/heads/main \
  --source-digest <full-candidate-sha> \
  --deny-self-hosted-runners
```

Checksums protect byte integrity after signature verification. They do not mean
the ESP32-S3 verifies publisher signatures at boot or during OTA; that remains
a separate device-side trust capability and must not be inferred from the
Sigstore or GitHub attestations.

## Which production file to use

| Scenario | File |
|---|---|
| Fresh standalone install | `firmware-merged.bin` at `0x0` |
| App-only update on the matching standalone layout | `firmware.bin` at `0x10000` |
| ESP Web Tools | `manifest.json` plus its four named component files |
| bmorcelli/Launcher | `KrabOS-tdeck-plus-launcher.bin` |

Do not force QIO when flashing a merged image; the admitted bootloader header
is DIO. An app-only image does not contain the bootloader or partition table and
must not be used for a fresh install. Flashing a complete merged image directly
at `0x0` replaces any existing Launcher bootloader and layout.

Under Launcher, use Launcher's own installer/update path. KrabOS detects that
Launcher owns updates and disables self-OTA to avoid corrupting co-installed
apps. Switching between Launcher and standalone layouts can relocate or reset
NVS and filesystem state; preserve identity and contacts through the documented
export path before changing modes.

## Reproducing non-release builds

The three release environments are:

```bash
pio run -e KrabOS_TDeckPlus
pio run -e KrabOS_TDeckPlus_recovery
pio run -e KrabOS_TDeckPlus_debug
```

These commands are useful for development, but local output is not a release
candidate. Only bytes produced from the exact clean `main` SHA by the Pi job,
validated against the artifact contract, exercised on the pinned fixture,
sealed, approved, signed and re-downloaded by `krabos-edge.yml` are publishable.

KrabOS is derived from SigurdOS T-Deck and retains its GPL and third-party
attribution. See [`NOTICE.md`](../NOTICE.md) and the licence bundle included in
the release.

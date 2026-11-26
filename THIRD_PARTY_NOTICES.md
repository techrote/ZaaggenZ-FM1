# Third-party provenance and licensing

## Original implementation

The original portable C engine, Windows evaluator, tooling, tests and authored
presets in this repository are under the MIT license in `LICENSE`.

## Zaaggenz source authority and compiled audio

The reference is `techrote/zaaggenz` at
`d7e580c190d00e1d00a573a6b845c75ddddc1d4c`.
Its recovered legacy implementation is authenticated against the upstream
recovery-payload and protected `locked_bloom` render hashes. The compiler reads
that repository without modifying it. No legacy Python implementation is
vendored into this project.

The source images in `src/source_bank_data.inc` are derived from that owner
provided reference. They retain the upstream identity
**LicenseRef-Owner-Provided-Unspecified**. The owner's request authorizes this
derivative project; this repository does not invent a permissive redistribution
grant over upstream material. The MIT license for new implementation code does
not relicense those audio images. `assets/source_bank_manifest.json` records the
exact sources, recipes, hashes, common PCM scaling, measured fidelity and tools.
Third-party reuse of those assets requires resolving that upstream grant.

## Fune

The exact pinned Fune SDK and build-tool subset is in `third_party/fune`.
Its `UPSTREAM.json` records every vendored file hash and revision
`dc1eb1b15a6bc108e09098e554ea8b0a7dbe8a1b`.

The SDK entry header and SDK license are MIT. Transitive interface headers,
integer helpers and build/installation tools have MPL-2.0 notices. They are
preserved unmodified with their notices and source. The project does not claim
the entire SDK subset is MIT. Original plugin source remains separately MIT;
the combined binary includes those separately licensed components.

The full upstream Fune host is fetched at the pin for integration tests and
native song/preset serialization. It is not modified or redistributed as a
replacement firmware. Upstream core tests/builds preserve its original licenses.

## JieLi compiler and FM-1 research

`tools/get_fune_toolchain.py` downloads the public vendor compiler and verifies
the pinned archive hash. The compiler is a build dependency and is not included
in the delivery ZIP. `AL-255/FM-1-RE` is a read-only hardware-research reference;
no stock firmware images, bootloaders or recovery payloads are redistributed.

Fune, M-VAVE and FM-1 names belong to their respective owners. This project is
an independent instrument integration and is not endorsed by those owners.

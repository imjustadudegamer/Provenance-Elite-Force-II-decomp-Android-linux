# Provenance and evidence

Every function, struct, table and constant in this port is held to one of three
witnesses, in priority order:

1. **The EF2 SDK**, the shipped game/engine source, the tie-break for behaviour.
2. **The retail Mac PPC binaries**, the primary decompile, at
   `personal/evidence/mac_symbols/` (untracked). The PC x86 binary is corroboration
   only: the two address spaces do not align.
3. **The shipped retail data**, the paks, censused byte-for-byte where an
   authored value, table size or count is at issue.

A claim that cannot be tied to one of these is recorded as open, not asserted.
The accepted divergences from retail are confined to platform glue, the sound
backend and the renderer's platform layer (the Vulkan renderer and its GLES 3.0
twin).

## The licence ledger

Provenance also decides the notice every file carries, and the licence ledger is
machine-checked:

- `PROVENANCE.tsv` records, per file, the bucket (`gpl`, `sdk`, `recon`,
  `vendored`), the basis for the decision and the nearest ancestor it was taken
  from. It is generated and checked by the project's local verification scripts
  against the upstream corpora.
- The bucket is decided by the file's own record where one survived (a port note
  naming the SDK or the retail binaries) and otherwise by the nearest ancestor:
  the OpenMoHAA tree at `personal/upstream/openmohaa-main/` first, then the EF2
  SDK. `renderergles3/` is matched against OpenMoHAA's `renderergl2/` and
  `renderergl1/` backends by filename, because the port's renderer is a renamed
  GLES 3.0 descendant of them, not port-original code; `renderervulkan/` is the
  same renderer mirrored onto Vulkan file for file, and carries the same GPL
  lineage notice.
- `LICENSING.md` states what each bucket means and why no single licence covers
  the tree; the project's local license check fails if a file carries no notice or two
  conflicting ones.

## Where a claim lives

A claim is carried by the source that makes it, in the comment at the line, with
the address or the SDK name that grounds it. Where a claim is behavioural rather
than structural, `docs/controls/` holds it up: a tracked host program that
compiles the shipping source, requires it green as written, and requires red on
every planted fault. A rig never launches the game. The fuller engineering notes
-- capture logs, the decompile corpus, the retail binary -- stay off the
repository, on disk only.

## Evidence is untracked

The decompile corpus, the retail binary and the SDK source are retail-derived
and are never published, so `personal/evidence/` and `personal/oracle/` are
gitignored and live on disk only. The tooling that reads them says so loudly
when a corpus is absent.

## Rules on this tree

- **Nothing is executed to prove a claim.** Nothing in the tree runs the game,
  an engine binary, a probe or a first-light executable. Evidence comes from
  static reads, raw-word assertions and shipped-asset censuses; the `docs/controls/`
  rigs are the one exception, and they are host programs built for the check, not
  the tree's own binaries.
- **No log carries a network address.** Probe rows keep the port, the state and
  the failure reason; the address-free form is `NET_AdrToLogString`.
- **Startup logging is minimal.** The engine prints the device model and the CPU
  model, and nothing further. On Android the instrument set is silent unless a
  crash prints or a probe family is armed explicitly.
- **A performance number comes from a quiet boot.** A probe-armed boot runs under
  the Vulkan validation layer with the probe families, the engine log and the
  host monitors on, so it measures the cost of being measured. Frame timings
  quoted as performance are taken without that instrumentation.
- **The prose speaks in the engine's terms.** Names, identifiers and quoted
  evidence come from the engine, the renderer or retail.

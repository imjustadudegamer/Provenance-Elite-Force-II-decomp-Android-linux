# Provenance and evidence

Every function, struct, table and constant in this port is held to one of three
witnesses, in priority order:

1. **The EF2 SDK** — the shipped game/engine source, the tie-break for behaviour.
2. **The retail Mac PPC binaries** — the primary decompile, at
   `docs/evidence/mac_symbols/` (untracked). The PC x86 binary is corroboration
   only: the two address spaces do not align.
3. **The shipped retail data** — the paks, censused byte-for-byte where an
   authored value, table size or count is at issue.

A claim that cannot be tied to one of these is recorded as open, not asserted.
The accepted divergences from retail are confined to platform glue, the sound
backend and the GLES 3.0 renderer/platform layer.

## The citation register

The reverse-engineering claims and the source comments that carried them are
recorded in `docs/RE_REFERENCE_CITATIONS.tsv`, one row per comment
(`file`, `line`, `kind`, `comment`). A claim stays recorded there,
checkable in the tree that carries it. The
fuller engineering notes are kept off the repository, on disk only.

## Evidence is untracked

The decompile corpus, the retail binary, the SDK source and the oracle corpora
are retail-derived and are never published, so `docs/evidence/` and
`docs/oracle/` are gitignored and live on disk only. The tooling says so loudly
when an untracked corpus is absent.

## Rules on this tree

- **Nothing is executed to prove a claim.** Nothing in the tree runs
  the game, an engine binary, a probe or a first-light executable. Evidence comes
  from static reads, raw-word assertions, offline replays of the shipping code
  and shipped-asset censuses. 
- **No log carries a network address.** Diagnostics keep the port, the state and
  the failure reason; the address-free form is `NET_AdrToLogString`.
- **Startup logging is minimal.** The engine prints the device model and the CPU
  model and nothing else identifying.
- **The prose speaks in the engine's terms.** Names, identifiers and quoted
  evidence come from the engine, the renderer or retail.

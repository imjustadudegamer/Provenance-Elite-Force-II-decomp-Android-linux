# Light flares: the compressed record

The flare family is retail's lens-flare pass, run after the world draw list
(`RB_RenderFlares @0x0004A6E0`): sphere lights flagged `lensflare`, dlight
flares, entity flares, the sun, and the gas-vent emitters. The pass is small.
The saga is that several unrelated defects fed it, and each one alone was
enough to keep the lights dark.

## Retail's law

- `r_flares` is registered as `"1"` (PC `EF2.exe` 0x5258ff): the boot tier is
  Simple, one world trace with a twenty-four-unit grace
  (`TestVisibility @0x00049150`).
- `"2"` is the precise tier: an occlusion query over a depth-tested,
  colour-silent quad, or a one-pixel depth readback when the query is absent.
- A flare fades out beyond `r_flaresMaxDist` (768); the corona itself draws
  with depth test off, so the visibility test is the only thing that hides it.
- The pass runs for the main view only, after the world walk.

## The defects, in landing order

1. **92d61c9** — the pass projected with the wrong camera; one wrong camera
   silenced every arm.
2. **6024914** — the sphere lights' ambient seed was 255 where retail writes
   zero.
3. **ba46cbf** — the visibility tiers ran swapped (precise traced the world,
   simple used the depth answer), and a light-group tile kept its creation
   white.
4. **c7f462a** — the occlusion query read the other frame's slot, and a tree
   copy had silently reverted the vertex-lit rewrite.
5. **aa72869** — the occlusion quad sat at the flare's own depth; retail's
   readback hides a flare only when the surface under it is more than
   twenty-four units nearer. The test quad now carries that tolerance.
6. **ae3b64e** — the query never answered here: the range read `ready=0` even
   when collected before the reset. The precise tier now falls back to
   retail's simple-tier trace whenever the depth answer is absent, and I
   restored the registered default `r_flares=1`.

## Final evidence

My probe-armed boot before `ae3b64e` read 0 visible states and 0 flare draws.
After it, the same rows read 200 visible states and 39 draws, with the
fallback rows naming the states the trace revived. A backend that returns
query results still takes the precise path; the fallback only covers the one
that does not.

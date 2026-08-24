# Credits

**ProvenanceEF2** is an unofficial, non-commercial port of *Star Trek: Voyager —
Elite Force II* to Android. It is not affiliated with or endorsed by any rights
holder.

*Star Trek: Voyager — Elite Force II* (2003) was developed by **Ritual
Entertainment** and published by **Activision**. The 2000 predecessor, *Star
Trek: Voyager — Elite Force*, was developed by Raven Software; Raven Software did
not develop the sequel. The Elite Force II SDK was released by Ritual
Entertainment in 2003; its own agreement, dated 2003-10-15, ships with this
repository at `docs/EF2-SDK-licenseAgreement.txt`, and the licence position is
set out in `LICENSING.md`.

## Upstream work this port builds on

- **id Tech 3** — the engine lineage. EF2 shipped a modified id Tech 3; this port
  starts from the de-MOHAAd tree of **OpenMoHAA**, itself derived from id Software's
  Quake III Arena GPL release and from the OpenMoHAA project.
- **Elite Force II SDK** (Ritual Entertainment, 2003) — the officially released
  server-side game source for the 1.10 patch, the authority for `fgame`. Used as
  reference and, where it is the shipped source, transcribed verbatim; the SDK's
  per-file banner is kept intact and the Agreement travels with it.
- **The retail Mac PPC binaries** — the primary reverse-engineering reference for
  every body retail did not ship source for; no rights are asserted over the
  material rebuilt from them (see `LICENSING.md`).
- **SDL2** — windowing, input and the audio device layer.
- **OpenAL Soft** — the sound backend.
- **libmad, libogg, libvorbis, opus, opusfile, libxmp** — audio decoding.
- **zlib, libjpeg, freetype** — compression, image and font support.
- **GameSpy SDK** — the multiplayer service glue.

## Art and assets

No retail game assets are distributed. The on-screen touch-control art and the
launcher icon are the port's own.

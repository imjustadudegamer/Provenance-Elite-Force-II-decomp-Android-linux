# Renderer

`code/renderergles3/` is the shipping renderer: GLES 3.0 on Android, GLES 3.0 on
the desktop reference through the same sources. It is selected by the
`cl_renderer` cvar and implements `refexport_t`; the engine talks to it only
through that interface. The fixed-function and Vulkan backends are deleted.

## Path

`R_RenderView` sorts draw surfaces, issues the pending render commands, then
draws debug lines. The backend walks the command list; `GLES3_DrawStage` turns
each shader stage into a program, binds the bundle images and the state bits, and
submits the geometry. Programs are built from the shipped `.shader` files parsed
with retail's `ParseStage` / `ParseShader` / `FinishShader` semantics, and are
cached on disk so a launch does not recompile them.

## Data

`tr_program_table.inc` is generated data: one row per stage permutation the
shipped shader corpus produces, plus rows for the expressible-but-unreachable
arms. It is `#include`d five times by `tr_program.c`, once per section, and it is
read as text by the shade census, so the C table and the audit record are one
file. 

`glsl/gles3_glsl_embedded.h` is generated from the `.glsl` sources by
`glsl/build_glsl.py`; the `.glsl` files are the sources of truth and the header must not drift from them.

## Views and effects

The renderer carries the seven view modes (normal, night vision, and the other
HUD/vision effects), the portal clip plane, the shadow-volume silhouette path,
the decal/mark path and the tiki/skeletor model path. Retail-faithful details —
the `RDF_HUD` reach on the stage path, the inline-colour reads, the texture-unit
class — are recorded with their addresses in the citation register.

## Probes

`renderergles3` builds offline probe binaries (`r200_gles3_firstlight`,
`r201_hwtex_probe`, `r207_progcache_probe`…) that compile and read back,
and are never run.

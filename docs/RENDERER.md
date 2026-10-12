# Renderer

`code/renderervulkan/` is the shipping renderer: Vulkan on Android and on the
Linux reference through the same sources. It is selected by the `cl_renderer`
cvar, whose compiled default is `vulkan`, and implements `refexport_t`; the
engine talks to it only through that interface. `code/renderergles3/` is the
GLES 3.0 reference twin: it builds beside Vulkan, is selected with
`cl_renderer gles3`, and the fixed-function backend is deleted.

The Vulkan module carries none of the older backend's vocabulary. Its
identifiers, files and cvars are named in Vulkan's own terms; the only GL-spelled
strings in it are the blend-mode tokens retail's `.shader` files spell
(`GL_ONE`, `GL_SRC_ALPHA`, ...), which are data the module parses, not names it
uses.

## Path

`R_RenderView` sorts draw surfaces, issues the pending render commands, then
draws debug lines. The back end walks the command list and records it into the
frame's Vulkan command buffers; the walk can run on the render thread
(`vk_renderthread.c`), which takes the frame through a park/handoff slot and
measures the front end's park time. The front end
(`tr_cmds.c`, `tr_backend.c`, `tr_shade.c`, `tr_draw.c`, `tr_shader.c`) is
mirrored file for file from `renderergles3/`, so both backends keep the same
surfaces, stages and ordering. `vk_shade.c` turns each stage into a pipeline
permutation, binds the descriptor set and the state, and submits the geometry;
`vk_draw.c`, `vk_descriptor.c`, `vk_pipeline.c` and `vk_vertex.c` own the draw
calls, the bind funnels, the pipelines and the per-frame vertex ring.

The Vulkan substrate is:

- `vk_procs.c` -- the loader. It is dlopen'd (`libvulkan.so.1` on the desktop,
  `libvulkan.so` on Android) and every entry point comes off
  `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr`, so the module carries no
  link-time Vulkan dependency.
- `vk_instance.c` -- the instance and the device, whose API version is requested
  from 1.3 down to 1.0 against the loader.
- `vk_swapchain.c`, `vk_platform.c` -- the `SDL_Vulkan_CreateSurface` surface, the
  swapchain, and the pre-rotation transform for Android's panel.
- `vk_frame.c`, `vk_target.c`, `vk_offscreen.c`, `vk_memory.c` -- the frame, the
  render targets and the allocator.
- `vk_texture.c` -- image creation, layout and the staged upload path.
- `vk_pipelinecache.c`, `vk_guard.c`, `vk_firstlight.c` -- the on-disk pipeline
  cache, the gated failure rows, and the first-light core.

## Shaders

`renderervulkan/shaders/*.vert` and `*.frag` are the sources of truth. The module
has no runtime shader compiler: `tools/build_vk_spirv.py` compiles every
permutation offline to SPIR-V and embeds it in `vk_shaders_embedded3d.h` (the
stage families) and `vk_shaders_embedded.h` (the fixed 2D and first-light
programs). The stage uniform block is spliced in at a marker and its member
offsets are read back out of the compiled SPIR-V, so the CPU-side packer is
checked against the compiler's own layout. The permutation set is the shipped
census the GLES 3.0 twin carries in `tr_program_table.inc`, and the census is a
build input on the same footing as the sources: the generated header records the
digest of everything the embedded SPIR-V is a function of, and
`python3 tools/build_vk_spirv.py --check` compares that digest against the shader
sources and the permutation census on disk and fails on drift.

## Texture uploads

A texture's uploads are staged, not submitted. One epoch per chain holds a
device buffer, its mapping and the byte extent it covers; a queue of pending
records names the copies; and the flush is what runs both. The two lifetimes are
deliberately disjoint and separately owned (`vkTexture_t`, `vk_texture.c`): a
path that writes a record must not be able to release the pointer it writes
through, and a record is never queued without a mapped buffer behind it. The
epoch belongs to the thread that opened it, so a flush from any other thread is
refused whole instead of freeing a map that thread is writing.

The flush closes the epoch unconditionally, and the queue dies with it: every
record names an offset inside that epoch, so a record that outlives the buffer it
points into is not a record that can be replayed. A failure at any point in the
chain leaves no epoch, no map and no queue, and the mips it would have carried
fall back to the per-mip path, which counts what it was handed. A refilled chain
arms a quiesce flag, so the flush parks the walk before its submit rather than
writing memory the GPU is still sampling.

## Pipelines, swapchain and the frame

The on-disk pipeline cache loads on Android behind crash markers. A load that
kills the process would otherwise repeat on every boot, so the marker names the
attempt in the write path and a later boot reads it back: a load that died has
its blob removed and its status printed once, turning a repeating failure into a
one-boot cost.

`VK_SUBOPTIMAL_KHR` from acquire still handed an image over, so the frame
presents it and records the hit rather than tearing the swapchain down mid-frame;
a pre-rotation transform that is itself a rotation makes the presentation engine
answer SUBOPTIMAL every frame, so the handler distinguishes the steady state from
the one-off. Out-of-date and post-present SUBOPTIMAL take the rebind path, and
the surface's current transform is re-read each frame so a rebind costs one
compare when nothing changed.

Vertex data is a ring per frame slot (`vk_vertex.c`): a submit does not wait, so
slot 1 fills while slot 0 executes, and one ring would rewrite geometry the GPU is
still fetching.

## Views and effects

The renderer carries the shipped view modes (normal, night vision and the other
HUD/vision effects retail registers), the portal clip plane, the shadow-volume silhouette path,
the decal/mark path and the tiki/skeletor model path. Retail-faithful details --
the `RDF_FULLBRIGHT` reach on the stage path, the inline-colour reads, the
texture-unit class -- are recorded with their addresses in the source comments
that carry them.

## The GLES 3.0 reference twin

`renderergles3/` is the same renderer with GLSL ES 3.0 programs built at run
time from the shipped `.shader` files parsed with retail's `ParseStage` /
`ParseShader` / `FinishShader` semantics: `GLES3_DrawStage` turns each stage into
a program, binds the bundle images and the state bits, and submits the geometry,
with the programs cached on disk so a launch does not recompile them.
`tr_program_table.inc` is generated data -- one row per stage permutation the
shipped shader corpus produces, plus rows for the expressible-but-unreachable
arms -- `#include`d five times by `tr_program.c`, once per section, so the C table
and the audit record are one file. `glsl/gles3_glsl_embedded.h` is generated from
the `.glsl` sources by `glsl/build_glsl.py`; the `.glsl` files are the sources of
truth and the header must not drift from them.

## Probes

The GLES 3.0 twin builds offline probe binaries (`gles3_firstlight`,
`smp_hwtex_probe`, `progcache_probe`, ...) that link the shipping renderer
sources and read back what they compile. They are never run. The Vulkan module
has no separate probe executable: its first-light core (`vk_firstlight.c`, the
`VK:firstlight` row) is linked into the renderer and answers to the master
`qa_probe` gate.

## The tess vertex stores (design notes)

These notes moved out of `renderergles3/tr_vbo.c` so the source keeps only the
short comment a reader needs at the line; the reasoning and the capture numbers
that justified it live here.

- **The tessellation store is a ring of persistent buffer pairs. The pair...**

  The tessellation store is a ring of persistent buffer pairs.  The pair is allocated ONCE at the structural maximum and is never re-specified; a send writes the batch's exact bytes at the pair's base with glBufferSubData.  The allocation is the ring, the upload is the batch, and the two are not the same number.  The slot rotates per batch so a send never writes the storage the preceding draws read; eight slots put eight batches between a write and the next write to the same store.  The device capture shows the cost of the old form is per call, not per byte (`[BE:send] sum=723374us batches=3552 first=239us mid=723135us`), so removing the per-send re-specification is the fix and padding the send is not required to reach it.

- **One VAO a store. The three stores rotate by frame, and the...**

  One VAO a store.  The three stores rotate by frame, and the attribute pointers are VAO state: one VAO shared across the rotation names whichever store it was captured against, so the draw reads a frame's bytes out of that store instead of out of the one the frame had just written -- which is the first two frames of every rotation drawn from a store the previous frames filled, and the reason a mapped store that carries correctly can still render garbage.  tools/arena_store_harness's `mapped` case holds this shape up and fails on it.

- **---- the mapped store (r_tessArena 4) ------------------------------------- The ring's per-send price is...**

  ---- the mapped store (r_tessArena 4) ------------------------------------- The ring's per-send price is the allocation: `glBufferData' with the batch's own size hands the driver a fresh block 220 times a frame, and the stall is size-independent (a 12-vertex send cost 11.8 ms).  This arm allocates each store once through GL_EXT_buffer_storage and maps it persistently, so a batch is a memcpy and the frame issues no driver call for the send at all.  Persistent is not a preference here.  An ordinary glMapBufferRange mapping forbids the draw that reads the buffer while it is mapped (ES 3.0 §6.3.1), and the draw is the very next thing this path does, so the only mapping that works is one that stays live -- hence the extension gate: without it the arm stands down and the ring runs.  Three stores rotate by frame, which is what keeps the unsynchronised write honest: a store is reused three frames after the GPU last read it.  A fence per store says when that is not true, and the frame takes the orphan-and-remap path -- one frame of the old store's price -- instead of writing memory the GPU is still reading.

- **---- static world vertex arena ------------------------------------------ A world surface whose batch the...**

  ---- static world vertex arena ------------------------------------------ A world surface whose batch the lightmapped multitexture fast path draws takes the same vertex bytes on every frame: the positions and texture coordinates come out of the BSP's own arrays, the colour is the constant white array, and the batch carries no dynamic light.  The arena therefore bakes each such surface once, appends the packed bytes to a device buffer that is written once and never re-specified, and later frames draw the surface from its region.  The bake runs the shipping pack body over the same tess range, so the bytes are the bytes the streaming path would have uploaded; the only thing that moves is where the vertex data comes from. Draw order, batch composition, index order, uniforms and state are untouched.  What is proven static for this class, and why it is the whole input set: - the surface is world BSP geometry, never an entity (the record hook tests `backEnd.currentEntity == &tr.worldEntity`); BSP surfaces do not move; - the shader takes RB_StageIteratorLightmappedMultitextureUnfogged, so the batch is one stage with a constant vertex colour and bundle 0/1 texture coordinates taken straight from `tess.texCoords` (the BSP drawVert st and lightmap fields), and no svars generator runs; - the batch has no dynamic light (`tess.dlightBits == 0`), so no dlight projection consumes the tess arrays, and `GLP_NORMAL` is not set by this program for a lightmapped shader, so the normal slot is not an input; - neither `r_showtris` nor `r_shownormals` is on, so no debug pass reads the batch back out of `tess`. Anything else -- a moving entity, a dlight, a deformed or waveform shader, the shadow pass, the generic or vertex-lit iterator, 2D -- is left on the streaming path byte for byte as it was.

- **Why the store was refused, for the stand-down row. A capture that starts mid-session...**

  Why the store was refused, for the stand-down row.  A capture that starts mid-session can miss the row that prints once at the first frame, and the question the acceptance had -- extension absent, or present and the map refused -- goes into the log's missing first line.  The reason is now a field of the state, and the arm row prints it on the same 600-frame cadence the arm itself uses. Why the store is not the arm: the number is the state, the name is what the row prints.  0 is what the 2026-10-07 PC capture read 208 times -- the arm was never asked for, `r_tessMap' shipped disarmed, so it is not a refusal.

- **`r_tessMap' is the arm switch and `r_tessArena' only the legacy override, because...**

  `r_tessMap' is the arm switch and `r_tessArena' only the legacy override, because the device's config.cfg already carries a value for the older name: earlier sessions registered it with different defaults, so a Cvar_Get on it now returns whatever that session left and a new default never reaches the code.  Only the two arms r_tessMap does not name are forced now: 1 the per-frame store, 2 the alternating A/B.  The default is 1.  The store disarms until tools/arena_store_harness carries it on a host context, because an arm whose verdict no capture saw is not an arm that ran: that acceptance PASSES all four cases on the host it asks for (Mesa ES 3.2, EGL surfaceless, GL_EXT_buffer_storage present), and the 2026-10-07 PC capture shows the extension gate was never the reason the arm stayed dark: "procs=165 core_missing=0 staged_desktop_traps=0", the extension list carries GL_EXT_buffer_storage, and every [BE:send] window still read arena=0 at two buffer writes a batch (18.8k to 80k batches a second).

- **The saved-matrix copies are fortify-checked and the bound the compiler derives for...**

  The saved-matrix copies are fortify-checked and the bound the compiler derives for the destination is the whole-array remainder, sizeof(stack) - 16*depth, which is zero for every depth outside [0, RB_MATRIX_STACK_DEPTH).  A one-ended test does not bound that copy: a negative depth satisfies a signed upper test, the remainder collapses to zero, and the abort lands in __memcpy_chk.  Each accessor therefore tests the index as unsigned.

## The 2D batch's spans, and the walk's pack (design notes)

- **The 2D batch keeps one span per window, because the window is dynamic state.**

  A UI widget sets its own window before it draws (`UIWidget::set2D` -> `Rend_Set2D`, and the container's own `Rend_Set2D`/`Rend_Scissor` pair), and a widget's window carries its own ortho, so a menu's 2D run is a window change every few quads.  The Vulkan lane answered each one with `RB_2D_Flush()` on `RC_2D_SET_WINDOW` and `RC_2D_SCISSOR`, which ends the tess surface: the stage pass re-ran its pipeline and descriptor lookup, texture and sampler bind, uniform ring slice and index copy for the handful of vertices that had accumulated, and the `ui` walk bucket paid for the whole menu that way.  What actually changes at a widget boundary is the ortho push constant and the viewport and scissor rect, and the two rects are dynamic state, so the batch now closes the queued quads as a *span*: `RB_2D_SpanBreak()` copies the state in force (`VK_Draw_2DStateGet`, the ortho `VK_Draw_Set2D` pushed plus `vk_2dWindow`/`vk_2dScissor`), the walk applies the new state, `RB_2D_SpanNote()` opens it as the next span, and the stage pass draws one indexed range per span with that span's state re-applied (`VK_Draw_2DStateApply`: the ortho alone, then `VK_Draw_Window2D` and the scissor if a later `RE_Scissor` moved it).  The last span runs to `tess.numIndexes`; under two spans the draw is the single draw the batch always made.  The quads, their order and their clip rects are unchanged: it is the surface work per widget that goes away, and the immediate (box and line) run still closes at a window change because its vertices carry no span.  The batch still ends where the recorded order needs it to -- a shader change, the pre-3D guard in `tr_cmds.c` (the Controls stardome's fill-vs-model order), a purge, the frame boundary -- and a split that cannot be represented (no window set, or the 256-span table full) falls back to the flush this case always did.
  The control is `docs/controls/d2span`: the source pin on that sequencing, and a model over generated request streams that checks every quad is drawn exactly once, under the state it was recorded with, in the order it was recorded, with fewer surface ends.

- **The pipeline probe comes before the program lookup, because the key decides it.**

  `VK_Shade_Pipeline` is called once per surface bind, and it used to resolve the shader-module pair (`VK_Program_Modules`, two hash probes) before consulting its own cache, then throw that result away whenever the probe hit -- which is nearly always, since a frame asks for a few dozen distinct pipelines over thousands of binds.  The cache key is (stateBits, family, defines, cull, polygonOffset, topology), and the module lookup's whole input is family and the permutation bits, both of which the key carries; a built pipeline also proves its modules exist, because the pipeline table and the module cache are created and dropped together (`VK_Shade_Init`, `VK_Shade_ResetTables`).  The probe therefore runs first and a hit returns before the lookup, with the permutation census unchanged: a hit still notes a request, and the miss row still prints where the modules are actually missing.  The control is `docs/controls/bindkey`.


- **The walk's vertex pack is a JobPool range, because it is parallel by construction.**

  `RB_VBO_DrawTessArrays` packed the surface's vertices in one loop on the walk thread.  Every element reads and writes only its own index (the ring slice is reserved before the batch is kicked), the sources are immutable, and the per-element expression order is untouched, so `JobPool_ParallelRange` over `VK_PACK_CHUNK` 256-vertex chunks gives the same bytes whichever worker runs which chunk -- which is exactly the contract `qcommon/q_jobs.h` states, and the one place in the walk where it holds element for element.  Below 256 vertices the pool runs the range inline, so a small surface pays nothing for the dispatch, and a 20000-vertex surface splits one range per worker.  `docs/controls/jobwalk` lifts the shipping `VK_Stream_Pack`, `VK_Stream_PackRange`, `vkPackCtx_t` and `vkStreamVertex_t` out of `vk_shade.c` and `tr_vbo.h` verbatim, links the shipping `q_jobs.c`, and checks the ranges cover every vertex exactly once, the parallel bytes are the sequential bytes over thirteen shapes, the pool really spreads the pack, and the parallel build is measurably faster (2.9x on the host).

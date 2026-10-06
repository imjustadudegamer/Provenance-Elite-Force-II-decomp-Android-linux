r39 — the markShadow fragment and the vertex-colour white pack
================================================================

WORKTREE: docs/wt/r39, branch r39-cone-root, base = master 17a9375d (the
blend mapping is REVERTED on that base).
COMMIT:   d425f2b6 "vk: the three vertex gens carry their own tess array into the pack"

RDC ROUTE ABANDONED (hard order honoured)
-----------------------------------------
This RenderDoc install ships /home/adam/renderdoc/bin/qrenderdoc and
lib/librenderdoc.so but NO headless python module: there is no pymodules/
directory and `python3 -c "import renderdoc"` fails with ModuleNotFoundError.
`renderdoccmd` has no replay-to-file mode (capture/convert/embed/extract/
inject/remoteserver/replay/thumb only; `replay` opens a preview window).
`qrenderdoc --python` runs the script inside the Qt app before the UI opens,
which risks a window on the operator's desktop.  Per the hard order the RDC
route was dropped and the case was convicted from the boot log, static code
and the retail pak bytes.  EID 651/654's surface identity is therefore
UNRESOLVED and is reported as such below.


1. THE CONE TEXTURE'S TRUTH  (charter step 2)
---------------------------------------------
Independently decoded pak16.pk3!textures/lights/volume_yellow_dark.dds
(32x256, DXT3, 9 mips) with scratch/ref_dxt3.py, written from the BC2 spec,
not from the port.

  mip0 mean = (25.36, 25.79, 18.52, 255.00)
  mip0 MAX  = (51, 53, 41, 255)      <-- the brightest texel in the whole file
  mip0 mid  = (24, 26, 18, 255)      port reported texel_mid=25,26,19,255
  mip0 alpha = 255 at EVERY texel

The port's decode is CORRECT.  Evidence B is settled: the FILE is dark, not
the decoder.  The column profile runs (51,53,38) near the top down to
(0,0,0) at the bottom -- a genuine vertical fade.

CONSEQUENCE: the cone's own stage is `blendFunc GL_SRC_ALPHA GL_ONE`
`rgbGen constant 1.0 0.9 0.8` `alphaGen dot` (lights.shader:164-180), so the
most it can add to any pixel is
  (51*1.0, 53*0.9, 41*0.8)/255 = (0.200, 0.187, 0.129).
Retail's cone built from THESE bytes is a FAINT warm tint, not a bright
glow.  No decode defect and no missing multiplier can make it brighter than
0.20.  If the operator's memory of retail is a strong glow, the carrier is
something else in the frame (the fixture, a flare), not this stage.


2. THE MARKSHADOW FRAGMENT  (charter's deliverable)
---------------------------------------------------
Retail markShadow (pak0.pk3!shaders/common.shader:238-246):

  markShadow
  {
      polygonOffset
      {
            map textures/common/shadow.tga
          blendFunc GL_ZERO GL_ONE_MINUS_SRC_COLOR
          rgbGen exactVertex
      }
  }

The port's parse and mapping are retail-exact:
  [VK:coldst] shader='markShadow' stage=-1 d=0 sb=0x41 b0='textures/common/shadow.tga'
              tex0=341 fmt=0x83f2 upto=32x32 rg=5 ag=0 const=0,0,0,0
  sb=0x41 -> src nibble 1 = VKS_SRCBLEND_ZERO, dst nibble 4 =
  VKS_DSTBLEND_ONE_MINUS_SRC_COLOR.  rg=5 = CGEN_EXACT_VERTEX, ag=0 =
  AGEN_IDENTITY.  tr_local.h:1616-1635.  (Settled ground #1 holds: this
  stage is the ONE ordinal-3 pass the census caught, and it is the only
  GL_ONE_MINUS_SRC_COLOR-dst stage likely live in this scene; the rest of
  the retail census is liquid/attrexian/drull/forever/viewmodes + 4
  bootprints stages.)

shadow.tga truth (pak15.pk3!textures/common/shadow.dds, 32x32 DXT3, 6 mips),
independently decoded:
  mip0 mean (93.51, 92.08, 93.51, 93.32)   max (216, 213, 216, 221)   min 0
  mip0 mid  (216, 213, 216, 221)           port reported 216,213,216,221  -> EXACT match
Grayscale radial blob, peak 216/255 = 0.847, row 16 falls 0 -> 216 -> 0.
r37's exoneration is reaffirmed for this file too.


THE DEFECT
----------
Retail's mark fragment is  frag.rgb = shadow.tga.rgb * vertex.rgb,  where the
cgame caller supplies the mark's own modulate as the vertex colour:

  cg_modelanim.c:636   alpha = (1.0 - trace.fraction) * 0.65f;
  cg_modelanim.c:664   CG_ImpactMark(..., alpha, alpha, alpha, 1.0f, ...)
  cg_marks.c:407-409   v->modulate[0..3] = pInfo->colors[0..3]
  tr_surface.c:169     *(int *)&tess.vertexColors[numv] = *(int *)p->verts[i].modulate;
  tr_shade.c:450       case CGEN_EXACT_VERTEX: svars.colors <- tess.vertexColors   (memcpy)

So the brightest retail fragment is 0.847 * 0.65 = 0.55, and with
dst = dst * (1 - frag) the mark darkens the surface to AT WORST 45 percent
of it.

This lane lost the modulate on the way to the GPU:

  vk_shade.c:1009  VK_StageNeedsCPUColor() returns qfalse for
                   CGEN_EXACT_VERTEX / CGEN_VERTEX / CGEN_ONE_MINUS_VERTEX
                   (it only names CGEN_LIGHTING_SPHERICAL, CGEN_STATIC and
                   four alphaGens).
  vk_shade.c:4648  colors = cpuColor ? svars.colors
                                     : (in2D ? vertexColors : NULL);   -> NULL in 3D
  vk_shade.c:2129  VK_Stream_Pack: if (!colors) out->color = 255,255,255,255;
  vk_stage.vert:278  case 5: color = floor(vcol * 255.0);      -> (255,255,255,255)

So frag.rgb = shadow.tga.rgb * 1.0, up to 0.847, and the mark darkened the
surface to AT WORST 15 percent of it -- 3.0x retail's density.  Over a lit
surface that is a black blob where retail draws a soft grey shadow.  This is
exactly the "fragment brighter than retail's making it too dark" case the
brief named.

BLAST RADIUS: 188 shipped shaders put rgbGen vertex in a blended stage
(weapon_fx's beams, impacts, flashes, rings, sparks among them).  Every one
of them lost its per-vertex modulate the same way -- the vertex shader's
u_ColorGen branch read a packed-white attribute instead of the tess array.


THE FIX (d425f2b6)
------------------
A new predicate VK_StageReadsVertexColor() names the three gens whose
u_ColorGen branch reads a_Color, and VK_DrawStage hands the pack
input->vertexColors for exactly those, instead of NULL:

  colors = cpuColor ? (const unsigned char *)input->svars.colors
                    : ( backEnd.in2D || VK_StageReadsVertexColor( pStage ) )
                      ? (const unsigned char *)input->vertexColors
                      : NULL;

Why the raw tess array and not svars: the vertex stage's own branches already
implement retail's CPU bodies exactly --
  case 5 (vert:278)  color = floor(vcol*255)                       == tr_shade.c:450 memcpy
  case 6 (vert:281)  color.rgb = vcol.rgb*u_IdentityLight          == tr_shade.c:475 *identityLight
  case 7 (vert:284)  color.rgb = (1.0-vcol.rgb)*u_IdentityLight    == tr_shade.c:496
Routing them through svars would apply identityLight twice, because
ComputeColors is only invoked for VK_StageNeedsCPUColor stages
(tr_shade.c:1228) and would then fill svars with the already-modulated
value.  2D already took this array, so 2D is bit-identical.

Ordering and depth are NOT defects here, checked and exonerated:
  - markShadow has polygonOffset and no sort -> SS_DECAL (tr_shader.c:2922);
    the cone's `sort additive` -> SS_BLEND1 (tr_shader.c:1925).  5 < 10, so
    the mark draws BEFORE the cone and the cone adds on top.  Retail order.
  - sb=0x41 carries no VKS_DEPTHMASK_TRUE (that is 0x100; the nearby
    met_light11 row is sb=0x100), so depth.depthWriteEnable is FALSE
    (vk_shade.c:2937).  A decal that writes no depth, as retail's does.


3. WHAT IS NOT EXPLAINED  (stated plainly)
-----------------------------------------
The operator's EID 654 wrote pure black (0,0,0) with A=1.0 and no depth
write.  Neither the pre-fix nor the retail mark fragment can reach pure
black: dst*(1-0.847) = 0.153*dst pre-fix, dst*(1-0.55) = 0.45*dst at retail,
and both keep alpha at dst.a*(1-1) = 0, not 1.0.  So EID 654 is NOT fully
accounted for by markShadow, and with the headless module unavailable I
could not name its surface.  EID 651 (dark WITH a depth write) is also not
markShadow, since markShadow's pipeline writes no depth.  What the fix does
establish is that the mark was 3x retail's density, which is a real defect
in the named path and is now retail-true; whether it is the whole of the
cone's black is unresolved.

Also noted, not fixed (out of scope, recorded for a later round):
  - The foreign unit-1 bind (cscreen-scroll.tga on the cone's unit 1) is a
    descriptor-hygiene defect but is UNREACHABLE: vk_stage.frag samples
    u_Tex1 only under #ifdef VKP_LIGHTMAP (vk_stage.frag:13,25-26) and the
    cone's program is 0x004001 = VKP_RGBAGEN|VKP_NORMAL (tr_program.h:95,
    112), no VKP_LIGHTMAP.  Evidence D is exonerated as a cause.
  - [VK:imgup] reports mips=9 for the cone and mips=6 for shadow but
    bytes=10752 and 1344 -- the sums of the first THREE levels only
    (8192+2048+512 and 1024+256+64).  Either the byte count is a different
    accounting or levels 3+ are never uploaded and read undefined.  Worth a
    look on its own; it cannot cause black (undefined s3tc usually reads
    small, and the sampled texels match the file).
  - The [VK:coldst] census has no throttle on its ordinal-3/4 branch
    (vk_shade.c:4726-4749) and printed one row for the whole boot.  Coverage
    was checked: VK_StageBindCensus is called at vk_shade.c:4716 and
    tr_shade.c:1452, which together cover every live 3D tess draw site.  The
    dlight path (tr_shade.c:410) is NOT covered but always sets
    VKS_DSTBLEND_ONE, so it cannot hide an ordinal-3/4 stage.
    RB_VBO_DrawTessIndexed_Stage (vk_shade.c:2402) is also uncovered but has
    no caller in this lane.  RB_StagePass (tr_backend.c:779) is 2D-only.


GATE
----
ninja -C app/src/main/cpp/build  0 warnings / 0 errors, 786/786
ldd -r  six artifacts (provenanceef2, provenanceef2ded, game.so, cgame.so,
        renderer_gles3.so, renderer_vulkan.so)  0 undefined each
license_lint --tree app/src/main/cpp/code --ledger PROVENANCE.tsv  GREEN (1588)
ship_text_lint  GREEN
make_zpak --check  GREEN (pak current, 11 entries)
controls  23/23 GREEN (arenabound beamradarcolour bindkey bonearm cfgwiring
          copysite d2color d2span d2window desccount footprint jobsizing
          jobwalk mapstate poolowner printpath roqintro saveshot
          shaderfallback uiscrollbar vbostatic walkaccount worldsetcensus)
comment_budget  RED pre-existing (unchanged by this round)


WHAT THE NEXT BOOT SHOWS
------------------------
On master as it stands (mapping still reverted) the visible change is the
mark-shadow and the 188-shader class, not the cone: character and impact
marks and every rgbGen vertex effect draw their own modulate again.  Marks
over a lit surface become retail's soft grey instead of near-black.

To settle the cone the coordinator should re-land the dst mapping on top of
d425f2b6.  With the fragment now retail-true the mark darkens the cone's
screen region to 45 percent of it at worst, in shadow.tga's soft radial
shape, instead of to 15 percent.  If the cone still goes black with both
landed, EID 654's surface is something else and the headless-module question
has to be answered first -- a pymodules/ install or a renderdoccmd
replay-to-file mode would reopen the RDC route inside the hard order.

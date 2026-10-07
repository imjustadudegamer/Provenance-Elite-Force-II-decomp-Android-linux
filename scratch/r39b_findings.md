r39b — the vertexColorValid gate, and why the menu A/B cannot be this diff
==========================================================================

WORKTREE: docs/wt/r39b, branch r39b-vertexcolours, base = master ea83cf14
COMMIT:   c9884173 "vk: a vertex gen carries the tess colour only when the surface wrote one"

1. THE REGRESSION AND ITS GATE
------------------------------
r39 carried input->vertexColors into the pack for every stage whose rgbGen is
one of the three vertex gens.  Retail's operand for those gens IS that array
-- but only for surfaces that have one.  tess.vertexColorValid is seeded false
per surface in RB_BeginSurface (tr_shade.c:197) and raised solely by the
handlers that fill the array:

  RAISES THE FLAG (array valid)             DOES NOT (flag stays at the seed)
    RB_SurfacePolychain  tr_surface.c:181     static models  tr_model.cpp:1880
    RB_SurfaceTriangles      :353             RB_SurfaceSprite   :128
    RB_SurfaceTerrain        :444             RB_SurfaceBeam     :486
    RB_SurfaceFace           :609             RB_SurfaceAxis     :802
    RB_SurfaceGrid           :797             RB_SurfaceEntity   :822
    RB_DrawSprite     tr_sprite.c:192        RB_SurfaceFlare   :843/880
    RB_ClipSkyPolygons tr_sky.c:479          DisplayList/Skip/Bad

The seed is (r_vertexLight != 0) || (hardwareType == 4).  r_vertexLight
defaults "0" (tr_init.c:178) and vkConfig.hardwareType is GLHW_GENERIC = 0
(vk_platform.c:169, GLHW_PERMEDIA2 = 4 is tr_types.h:306-310), so the seed is
false on this lane and the flag is a reliable per-surface witness.

Static models are the important hole: their colour copy sits inside #if 0
(tr_model.cpp:1880-1922) and tr.world has no smColors member to load it from
(tr_local.h has staticModels only), so a static model's tess array still held
the PREVIOUS surface's colours.  r39 handed that stale array to the shader.

The stagebind census splits the two populations exactly, in both boots:

  rg=10  CGEN_LIGHTING_SPHERICAL  -- every character skin
         (models/char/borg/*, models/char/hazardteam/*, chell-face, monocle)
         This is a VK_StageNeedsCPUColor gen, so ComputeColors has always
         filled it and r39 never touched it.
  rg=5   CGEN_EXACT_VERTEX        -- every environment model
         (models/borg/panel, models/enviro/borg-sphere/alcove/alcove1..3,
          pannel, borg-light)

The rg=5 class is the set-dressing statics -- the grey ship hull the operator
read.  Gate: only carry the array when input->vertexColorValid is true.
Marks keep their modulate (Polychain raises the flag), statics return to the
white pack they drew before r39.

Blast radius re-audit, non-vertex-gen passes (charter ask):
  rg=2  CGEN_IDENTITY  -> VK_StageReadsVertexColor is false -> colors is
         in2D ? vertexColors : NULL, byte-identical to pre-r39.
  rg=10 CGEN_LIGHTING_SPHERICAL -> VK_StageNeedsCPUColor is true -> the
         svars branch, which the gate never reaches.
  The gate is (in2D || (VK_StageReadsVertexColor(pStage)
                        && input->vertexColorValid)); the predicate switches
  only on CGEN_EXACT_VERTEX / CGEN_VERTEX / CGEN_ONE_MINUS_VERTEX
  (vk_shade.c:1037-1055).  Nothing else can take the raw array.


2. THE MENU A/B CANNOT BE PRODUCED BY THIS DIFF  (pipeline-cache audit)
----------------------------------------------------------------------
The diff ea83cf14 -> 87f8d92f is one file, vk_shade.c, and its ONLY
behavioural change is which dst ordinals resolve differently.  I read it in
full.  The new VK_Shade_DstBlendFactor's default returns VK_BLEND_FACTOR_ZERO
-- the same value the old code's `src ? ONE : ZERO` returned for dst -- so
ordinal 0 (opaque, no blend bits) is UNCHANGED.  My first reading, that every
opaque stage goes additive, is wrong and retracted.  The ordinals that
actually move are dst 3 (SRC_COLOR) and dst 4 (ONE_MINUS_SRC_COLOR).

  dst nibble   old (reverted)            new (landed)
      1        ZERO      (via src tbl)   ZERO        same
      2        ONE                     ONE         same
      3        DST_COLOR (via src tbl) SRC_COLOR   CHANGED
      4        ONE_MINUS_DST_COLOR     ONE_MINUS_SRC_COLOR  CHANGED
      5,6,7,8  correct                 correct     same
      0        default ZERO            default ZERO same

Against that:
  - The ship's three passes log sb = 0x100 / 0x22 / 0x65 -> dst ordinals
    0 / 2 / 6.  None is 3 or 4.  Identical in ALL FOUR available boots,
    both builds (run-20261020-150948, -150446, -152018, -152519).
  - VK:coldst (the dst-3/4 census) prints ZERO rows in the menu boot.
  - The in-memory shade pipeline cache compares the FULL key including
    stateBits, which carries the blend nibbles (vk_shade.c:2830-2837), so it
    cannot return a pipeline whose blend differs from the requesting
    stateBits.
  - The on-disk pipeline cache is behind r_vkPipelineCacheLoad, which
    defaults "0" (vk_pipelinecache.c:87); vk.pipelineCache stays
    VK_NULL_HANDLE, so the blob is never loaded.  Inert.
  - r39's fix (0848d92f) is an ancestor of BOTH ea83cf14 and 87f8d92f
    (git merge-base --is-ancestor), and is a provable no-op for all three
    ship passes per the blast-radius audit above.

CONCLUSION: no stage, no pipeline, no cache and no pack change differs
between the two builds for the menu frame.  The menu ship is bit-identical
by construction, so the reported A/B is confounded -- most plausibly the two
boots did not run the same rebuilt binary, or differ by frame/scene state.
The discriminating test needs no boot: the [VK:stagebind] sb= fields for
menu/Intrepid-02 are already identical across both builds in the logs on
disk.  If the operator re-boots both and still sees a difference, the build
directory is the suspect, not the blend code.

A test that WOULD convict if the operator wants it: set
r_vkPipelineCacheLoad 1 on both sides -- but since it is off by default on
both, it cannot be the current difference.


3. TEXTURE TRUTH  (charter ask: is the glow black-opaque?)
----------------------------------------------------------
intrepid2-glow.dds, pak20.pk3, 256x256 DXT3 9 mips, decoded independently
(scratch/ref_dxt3.py, written from the BC2 spec):

  mip0 mean = (9.13, 10.89, 11.33, 255.00)   <-- alpha 255 at EVERY texel
  mip0 MAX  = (239, 255, 255, 255)           <-- the file HAS bright content
  mip0 mid texel (128,128) = (0,0,0,255)     <-- matches the port exactly

The glow is mostly black with bright streaks peaking at 255.  "texel_mid =
0,0,0,255" is therefore the TRUE mid texel, not a decode defect.  This is the
same one-texel trap as the cone texture in r39: a mid-texel sample says
nothing about a texture whose content is localised.  No second DXT3 defect
class, and the .tga name changes nothing -- the pak stores a .dds under the
.tga name and the loader resolves it.

intrepid2.dds, pak20.pk3, 512x512 DXT3 10 mips:
  mip0 mean (38.73, 45.07, 51.56, 212.69)
  mip0 max (255, 251, 255, 255)   min (0, 0, 0, 102)
  mid texel (33, 40, 49, 221)     <-- matches the port exactly
The hull texture DOES contain black texels and its alpha is a gradient
102..255, not uniform.


4. EID 70 VERDICT
-----------------
EID 67 wrote (0.294, 0.325, 0.353) A=0.239.  EID 70 wrote (0,0,0) A=1.0, no
depth change, same 4086-index geometry.

Pass 3 is sb=0x22 = GL_ONE GL_ONE additive, so result.rgb = src + dst >=
dst.rgb.  With dst = EID 67's 0.294 the additive pass CANNOT write 0.  EID 70
is therefore not pass 3 drawing after pass 2.

The signature does fit a (SRC_ALPHA, ONE_MINUS_SRC_ALPHA) pass whose source
RGB is black and whose alpha is 1.0 -- that is pass 2 sampling a black texel
of intrepid2.tga, which the file does contain.  But EID 67's A=0.239 sits
BELOW intrepid2's minimum alpha of 102/255 = 0.400, which a correct pass 2
cannot produce, because vk_stage.frag:23 multiplies the texture's own alpha
into the result.  That is a real anomaly and the sharpest lead left: some
pass at that pixel is drawing with an alpha that is not the texture's.

Limit, stated plainly: with no headless RenderDoc module in this install I
cannot name EID 70's surface or pipeline from here.  The one-line probe that
would close it is the operator's pipeline-state read at EID 70 (blend factors
+ bound image + the fragment's alpha), which needs the GUI or a pymodules
install.


5. A GENUINE DIVERGENCE FOUND, LEFT UNCHANGED
---------------------------------------------
tr_shader.c:1022 maps `blendFunc ADD` to GL_ONE GL_ONE (sb 0x22).  The
QuakeIII/MOHAA convention for ADD is GL_SRC_ALPHA GL_ONE (sb 0x52).  For the
glow stage this is invisible -- its alphaGen is identity so src.a = 1 and the
two are equal -- but for any ADD stage carrying a non-identity alphaGen the
port over-adds and, on the alpha channel, differs unconditionally.

I did NOT change it.  I have no retail EF2 shader parser to prove which of
the two retail compiles, and asserting the Q3A convention over EF2's own
bytes would be exactly the kind of premise the recomp quarantine bans.  The
check that settles it is retail's NameToSrcBlendMode in the Mac engine db.


GATE
----
ninja -C app/src/main/cpp/build  0 warnings / 0 errors, 786/786
ldd -r  six artifacts  0 undefined each
license_lint GREEN (1588)   ship_text_lint GREEN   make_zpak current (11)
controls 23/23 GREEN
comment_budget RED pre-existing

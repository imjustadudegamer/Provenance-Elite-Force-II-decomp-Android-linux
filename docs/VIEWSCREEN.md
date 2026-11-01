# The bridge viewscreen: the compressed record

The viewscreen is a portal. `maps/ent-deck1_bridge.bsp` places a
`portal_surface` at (1952,-5666,72) that targets `portalViewscreenCam_Actor`,
whose `portal_camera` sits at (1952,-6028,72) facing 270. The engine renders
that camera's view into the hole the panel leaves, and the panel itself is the
`textures/common/portal` marker: `blendFunc GL_ZERO GL_ONE`, so it preserves the
destination and can never paint the panel. Whatever is not drawn inside that
rect stays as it was — black, or the previous frame's leftovers.

## Retail's law

- `R_MirrorViewBySurface @0x1DC20` copies the caller's view parameters and
  writes only `ori`, `portalPlane`, `pvsOrigin` and `isMirror`. It never writes
  `viewport*` or `fov*`, so the sub-view inherits the parent's rect and
  projection by design.
- `RB_BeginDrawingView @0x1490` arms the user clip plane on the dword sixteen
  bytes before `portalPlane`. In this struct that word is `isMirror`: the field
  order is `isPortal`, `isMirror`, `isPortalSky`, `frameSceneNum`,
  `frameCount`, `portalPlane`.
- The marker is the main view's first draw, with depth test and depth write at
  full range, which is what shields the hole from the room behind it.
- A portal sub-view does not render the sky (`skyRendered` is per frame and the
  sub-view's `R_Sky_Render` early-outs); the starfield that can appear in the
  panel is a skybox model entity (`sky/sky_stardome-black.tik`), reached through
  a sky-portal view, not through the sub-view.

## The defects, in landing order

1. **ecca11a** — the 3D immediate transform was built transposed
   (`(view*proj)^T`) while the shader reads a column-major mat4. Every vertex on
   that path came out behind the eye and was clipped: the sky sides, the shadow
   volumes, the debug lines, and the finish overlay.
2. **ecca11a** — the render command that owns its surface slice kept the slice
   inline but the walk advanced by the struct's size, so the slice was read as
   commands and every record after it was lost. That is what removed the whole
   2D screen while the 3D scene behind it still drew: the record now carries
   `totalBytes` and both walks step over it.
3. **ecca11a** — the stencil plane is cleared per view (retail's
   `RB_BeginDrawingView` clears `1280` = depth|stencil when the shadow tier is
   the precise one), and a volume whose `ori` is the view's own matrix is
   refused rather than painting the frame.
4. **the clip plane** — the user plane was armed on `isPortal` where retail's
   field test lands on `isMirror`. Every viewscreen frame logs `mirror=0`, so
   the port armed a plane retail never arms, and with
   `plane=(0,-1,0 | 6028)` and `ori.origin=(1952,-6028,72)` the eye-space plane
   is `(-1,0,0,0)`: `vk_stage.frag` discarded `x_eye > 0`, half the panel, in
   every bridge scene. The retail gameplay trace contains no `glClipPlane` at
   all, which is the same statement from the other side.

## Final evidence

- `[VK:portal] sub surfEnt=… mirror=0 hit=… cam=… plane=…` — the sub-view is
  built, and every viewscreen row says `mirror=0`.
- `[VK:portalframe] f=… sub vp=(…) scn=(…) range=… draws=… marker=(…) inter=W×H`
  — the sub-view's rect, the scissor its draws ran under, its depth range, its
  draw count, the marker's screen rect, and their intersection. A zero
  intersection means the sub-view draws where the panel is not; a non-zero one
  means composition supplies the panel and the panel's colour is whatever the
  sub-view's batches rasterised.
- `[VK:draworder] … head=#0 'textures/common/portal'` — the marker leads the
  main view, so the hole is shielded.
- The content list was never the problem, and that is now measured rather than
  argued: `[VK:fe] view adds=1647 rec=17 ents=56` is the nested sub-view's own
  list at frames 5264/5518/5718/5918, and it decomposes from the map bytes — the
  AttrexianCommand backdrop contributes its one +y face of bmodel `*45`, the
  guide `*42` is box-culled behind the camera, and `*1` is `RF_DONTDRAW` and is
  refused where retail refuses it. The sub-view camera sits in leaf 1057 /
  cluster 451 / area 5, and PVS(451) is twelve clusters whose leaves carry no
  surfaces and no brushes, so adding zero world and zero sky surfaces is the
  map's construction, not a cull.
- The sky chain is intact too: the `RF_SKYORIGIN` carrier is in the snapshot at
  (-768,-5478,-1274) even though its cluster 462 is outside both the client's
  and the camera's PVS, the bit survives the sublevel restore, and
  `cg.sky_origin`/`cg.sky_axis` reach `tr.refdef`. The stars that appear in the
  panel are that sky view's full-screen output surviving where the sub-view
  painted nothing, since no view clears colour after the pass opens.
- A RenderDoc capture of the failing frame is kept at
  `personal/viewscreen.zip` (frame 4023): two `DEPTH|STENCIL` clears, both views
  on the identical `(240,0) 1440x1080` rect, and the second view issuing its
  draws — geometry was always arriving; the plane was discarding it.

#version 450

/*
 * The tessellation stream's vertex shader: one vertex of a 2D/HUD quad, a
 * glyph, or (from M3) a world surface.  The projection travels as a push
 * constant because the engine re-issues it per 2D window and per view, and a
 * window change must not cost a descriptor write.
 *
 * Mali rule (MALI_FIXES #2): no gl_ClipDistance / gl_CullDistance /
 * gl_PointSize -- the clip is baked into the matrix the back end pushes.
 */

layout( push_constant ) uniform Push
{
	layout( offset = 0 ) mat4 ortho;
	layout( offset = 64 ) vec4 alphaTest;	/* the fragment stage reads this one */
} pc;

layout( location = 0 ) in vec3 aPosition;
layout( location = 1 ) in vec2 aTexCoord;
layout( location = 2 ) in vec4 aColor;

layout( location = 0 ) out vec2 vTexCoord;
layout( location = 1 ) out vec4 vColor;

void main()
{
#ifdef EF2_PROBE_VERTINDEX
	/* diagnostic: ignore the vertex input entirely; three dots across the frame */
	gl_Position = vec4( -0.6 + 0.6 * float( gl_VertexIndex ), 0.0, 0.0, 1.0 );
#elif defined( EF2_PROBE_FIXEDXFORM )
	/* diagnostic: ignore the pushed matrix entirely */
	gl_Position = vec4( aPosition.xy / 640.0 - vec2( 0.5 ), 0.0, 1.0 );
#else
	gl_Position = pc.ortho * vec4( aPosition, 1.0 );
#endif
	vTexCoord   = aTexCoord;
	vColor      = aColor;
}

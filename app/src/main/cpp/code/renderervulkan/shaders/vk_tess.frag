#version 450

/*
 * The tessellation stream's fragment shader: the stage bundle modulated by the
 * vertex colour.  This is the same multiply the reference renderer's 2D family
 * program does -- texture * colour -- with the alpha-test the stage's state
 * bits asked for (the atest bits), because Vulkan has no fixed-function alpha
 * test for the pipeline to inherit.
 */

layout( location = 0 ) in vec2 vTexCoord;
layout( location = 1 ) in vec4 vColor;

layout( binding = 0 ) uniform sampler2D uTexture;

layout( location = 0 ) out vec4 outColor;

/* The same block the vertex stage declares: one push-constant range covers both
   stages, which is what the pipeline layout declares. */
layout( push_constant ) uniform Push
{
	layout( offset = 0 ) mat4 ortho;
	layout( offset = 64 ) vec4 alphaTest;	/* x = bound (0 = off), y = 1 when the test is 'less than' */
} pc;

void main()
{
	vec4 texel = texture( uTexture, vTexCoord );
#ifdef EF2_PROBE_NOTEX
	texel = vec4( 1.0, 1.0, 1.0, 1.0 );
#endif
	vec4 colour = texel * vColor;

	if ( pc.alphaTest.x > 0.0 ) {
		if ( pc.alphaTest.y > 0.5 ) {
			if ( texel.a >= pc.alphaTest.x ) {
				discard;
			}
		} else if ( texel.a < pc.alphaTest.x ) {
			discard;
		}
	}

	outColor = colour;
}

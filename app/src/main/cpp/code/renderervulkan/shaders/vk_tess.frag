#version 450

/*
 * The tessellation stream's fragment shader: the stage bundle modulated by the
 * vertex colour, with the alpha-test the stage's state bits asked for -- Vulkan
 * has no fixed-function test to inherit.  The selector is the reference
 * renderer's own: 0 off, 1 GT0, 2 LT128, 4 GE128.
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
	layout( offset = 64 ) vec4 alphaTest;	/* x = the selector (0 = off) */
} pc;

void main()
{
	vec4 texel = texture( uTexture, vTexCoord );
#ifdef EF2_PROBE_NOTEX
	texel = vec4( 1.0, 1.0, 1.0, 1.0 );
#endif
	vec4 colour = texel * vColor;

	if ( pc.alphaTest.x == 1.0 ) {
		if ( !( colour.a > 0.0 ) ) {
			discard;
		}
	} else if ( pc.alphaTest.x == 2.0 ) {
		if ( !( colour.a < 0.5 ) ) {
			discard;
		}
	} else if ( pc.alphaTest.x == 4.0 ) {
		if ( !( colour.a >= 0.5 ) ) {
			discard;
		}
	}

	outColor = colour;
}

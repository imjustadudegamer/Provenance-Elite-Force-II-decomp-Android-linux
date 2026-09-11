#version 450

/*
 * The tessellation stream's fragment shader: the stage bundle modulated by the
 * vertex colour.  This is the same multiply the GLES3 twin's GLP_FAMILY_2D
 * program does -- texture * colour -- with the alpha-test the stage's state
 * bits asked for (GLS_ATEST_*), because Vulkan has no fixed-function alpha
 * test for the pipeline to inherit.
 */

layout( location = 0 ) in vec2 vTexCoord;
layout( location = 1 ) in vec4 vColor;

layout( binding = 0 ) uniform sampler2D uTexture;

layout( location = 0 ) out vec4 outColor;

layout( push_constant ) uniform Push
{
	layout( offset = 0 ) float alphaTest;	/* 0.0 = off, else the compare bound */
	layout( offset = 4 ) float alphaLess;	/* 1.0 = the test is 'less than' */
	layout( offset = 8 ) float unused0;
	layout( offset = 12 ) float unused1;
} pc;

void main()
{
	vec4 texel = texture( uTexture, vTexCoord );
	vec4 colour = texel * vColor;

	if ( pc.alphaTest > 0.0 ) {
		if ( pc.alphaLess > 0.5 ) {
			if ( texel.a >= pc.alphaTest ) {
				discard;
			}
		} else if ( texel.a < pc.alphaTest ) {
			discard;
		}
	}

	outColor = colour;
}

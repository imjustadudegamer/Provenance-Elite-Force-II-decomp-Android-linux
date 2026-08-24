#version 300 es
/* gles3: stage-matched default precision (see build_glsl.py); int and
   sampler2D defaults differ per stage in ES 3.00, and a shared uniform or
   varying must match or the pair is a LINK error on a strict driver.
   sampler2D is pinned highp in BOTH stages on purpose: u_WaveTable is an
   R32F sin/table texture read with texelFetch and its value precision is
   the sampler qualifier, so a lowp sampler would truncate the table. */
precision highp int;
precision highp float;
precision highp sampler2D;


precision highp float;	


uniform vec4	u_ClipPlane;		
uniform int		u_ClipPlaneEnable;	

in highp float v_ClipDist;
out vec4 fragColor;
void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	fragColor = vec4(0.0);
}

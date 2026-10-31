#version 450
#define VK_FRAGMENT 1


//@@VK_COMMON@@

//@@VK_CLIP@@
layout(location = 4) in float v_ClipDist;

layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec2 v_TexCoord0;
layout(location = 3) in float v_FogDepth;
#ifdef VKP_LIGHTMAP
layout(location = 5) in vec2 v_TexCoord1;
#endif

layout(location = 0) out vec4 fragColor;

void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	vec4 t0 = texture(u_Tex0, v_TexCoord0);
	vec4 c = t0 * v_Color;					

#ifdef VKP_LIGHTMAP
	vec4 t1 = texture(u_Tex1, v_TexCoord1);
	#if defined(VKP_TEXENV_ADD)
		c = t0 * v_Color + t1;
	#elif defined(VKP_TEXENV_INTERPOLATE)
		c.rgb = t1.rgb * v_Color.a + t0.rgb * (1.0 - v_Color.a);
		c.a   = t0.a * t1.a;
	#elif defined(VKP_TEXENV_COMBINE4)
		c.rgb = (1.0 - t0.rgb) * (1.0 - t1.rgb) + t1.rgb * (1.0 - t1.rgb) + t0.rgb * t1.rgb;
	#else
		
		c = t0 * t1 * v_Color;
	#endif
#endif

	
	if (u_AlphaTest == 1) {
		if (!(c.a > 0.0)) discard;			
	} else if (u_AlphaTest == 2) {
		if (!(c.a < 0.5)) discard;			
	} else if (u_AlphaTest == 4) {
		if (!(c.a >= 0.5)) discard;
	}

	// flat-world diagnostic: 1 forces red, 2 shows the batch's vertex colour; the
	// alpha the test above ran on is untouched, so the alpha axis stays readable
	if (u_FlatMode == 1) {
		c.rgb = vec3(1.0, 0.0, 0.0);
	} else if (u_FlatMode == 2) {
		c.rgb = v_Color.rgb;
	}

	if (u_FogMode == 2) {
		c.rgb = VK_FixedFog(c.rgb, v_FogDepth);
	}

	fragColor = c;
}

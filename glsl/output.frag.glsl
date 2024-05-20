#version 330

uniform usampler2D frame;
uniform usampler2D ref;
uniform sampler3D lut;
uniform uint samples;
uniform float gain = 2.0;
uniform bool use_lut = true;
uniform bool use_ref = false;
uniform uvec4 ref_mean = uvec4(0);
uniform bool ref_after_lut = false;

in  vec2 pos;
out vec4 color;

void main(void)
{
	vec2 size = textureSize(frame, 0);
	ivec2 texpos = ivec2(pos * size);

	uvec4 tex = texelFetch(frame, texpos, 0);
	uvec4 ref_tex = texelFetch(ref, texpos, 0);

	vec4 avg = vec4(tex / uvec4(samples));

	vec4 raw = clamp(avg * gain, 0.0, 65535.0);
	vec4 black = clamp(ref_tex * gain, 0.0, 65535.0);

	if(use_ref && !ref_after_lut) {
		raw = clamp((avg - ref_tex + vec4(ref_mean)) * gain, 0.0, 65535.0);
	}

	// apply LUT
	vec4 graded;
	if(use_lut) {
		vec3 idx = raw.rgb / 65535.0;
		graded = texture(lut, idx);

		if(ref_after_lut) {
			vec3 ref_idx = black.rgb / 65535.0;
			vec4 ref_graded = texture(lut, ref_idx);

			bvec4 clamped = greaterThan(avg * gain, vec4(65535.0));
			graded = clamp(graded - ref_graded + vec4(clamped), 0.0, 1.0);
		}
	} else {
		graded = raw / 65535.0;
	}

	// output
	color = vec4(graded.rgb, 1.0);
}

#version 330

#define M_PI	3.14159265358979323846

uniform usampler2D frame;
uniform sampler3D lut;
uniform uint samples;
uniform float gain = 2.0;

in  vec2 pos;
out vec4 color;

void main(void)
{
	vec2 size = textureSize(frame, 0);

	uvec4 tex = texelFetch(frame, ivec2(pos * size), 0);
	vec4 raw = clamp(vec4(tex / uvec4(samples)) * gain, 0.0, 65535.0);

	// apply LUT
	vec3 idx = raw.rgb / 65535.0;
	vec4 graded = texture(lut, idx);

	// output
	color = vec4(graded.rgb, 1.0);
}

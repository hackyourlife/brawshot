#version 330

uniform usampler2D frame;
uniform uint samples;

in  vec2 pos;
out uvec4 color;

void main(void)
{
	vec2 size = textureSize(frame, 0);

	uvec4 tex = texelFetch(frame, ivec2(pos * size), 0);
	vec4 raw = clamp(tex / uvec4(samples), 0, 65535);

	color = uvec4(raw);
}

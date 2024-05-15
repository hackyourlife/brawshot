#version 330

uniform usampler2D frame;
uniform usampler2D accumulator;

uniform bool add = true;

in  vec2 pos;
out uvec4 color;

void main(void)
{
	vec2 size = textureSize(frame, 0);
	ivec2 texpos = ivec2(pos * size);

	uvec4 old = texelFetch(accumulator, texpos, 0);
	uvec4 new = texelFetch(frame, texpos, 0);

	if(add) {
		color = old + new;
	} else {
		color = old - new;
	}
}

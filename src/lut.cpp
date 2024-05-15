#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <GL/gl.h>

#include "lut.h"

LUT::LUT(const char* filename) : lut(nullptr), points(0), size(0)
{
	FILE* f = fopen(filename, "rt");
	if(!f) {
		printf("Error opening %s: %s\n", filename, strerror(errno));
		return;
	}

	char buf[256];

	while(fscanf(f, "%s", buf) == 1) {
		if(!strcmp(buf, "LUT_3D_SIZE")) {
			fscanf(f, "%u", &points);
			break;
		}
	}

	if(!points) {
		fclose(f);
		return;
	}

	size = points * points * points;
	lut = new double[size * 3];

	double* l = lut;
	for(unsigned int i = 0; i < size; i++) {
		fscanf(f, "%lf %lf %lf", &l[0], &l[1], &l[2]);
		l += 3;
	}

	fclose(f);
}

LUT::~LUT()
{
	if(lut != nullptr) {
		delete[] lut;
	}
}

unsigned int LUT::get_points()
{
	return points;
}

double* LUT::get()
{
	return lut;
}

unsigned int LUT::cube_index(unsigned int r, unsigned int g, unsigned int b)
{
	return r + g * points + b * (points * points);
}

static double mix(double a, double b, double c)
{
	return a + (b - a) * (c - floor(c));
}

void LUT::apply(uint16_t r, uint16_t g, uint16_t b, uint16_t* out)
{
	double r_idx = (r / 65535.0) * (points - 1);
	double g_idx = (g / 65535.0) * (points - 1);
	double b_idx = (b / 65535.0) * (points - 1);

	unsigned int r_h = ceil(r_idx);
	unsigned int r_l = floor(r_idx);

	unsigned int g_h = ceil(g_idx);
	unsigned int g_l = floor(g_idx);

	unsigned int b_h = ceil(b_idx);
	unsigned int b_l = floor(b_idx);

	unsigned int index_h = cube_index(r_h, g_h, b_h);
	unsigned int index_l = cube_index(r_l, g_l, b_l);

	double* to_color_h = &lut[index_h * 3];
	double* to_color_l = &lut[index_l * 3];

	double to_r = mix(to_color_l[0], to_color_h[0], r / 65535.0);
	double to_g = mix(to_color_l[1], to_color_h[1], g / 65535.0);
	double to_b = mix(to_color_l[2], to_color_h[2], b / 65535.0);

	out[0] = round(to_r * 65535.0);
	out[1] = round(to_g * 65535.0);
	out[2] = round(to_b * 65535.0);

	if(to_r > 1.0) {
		out[0] = 0xFFFF;
	}

	if(to_g > 1.0) {
		out[1] = 0xFFFF;
	}

	if(to_b > 1.0) {
		out[2] = 0xFFFF;
	}
}

GLuint LUT::create_texture()
{
	GLuint tex;

	if(lut == nullptr) {
		return (GLuint) -1;
	}

	float* data = new float[size * 3];
	for(unsigned int i = 0; i < size * 3; i++) {
		data[i] = (float) lut[i];
	}

	// create LUT texture
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_3D, tex);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, points, points, points, 0,
			GL_RGB, GL_FLOAT, data);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	delete[] data;

	return tex;
}

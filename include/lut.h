#ifndef __LUT_H__
#define __LUT_H__

#include <GL/gl.h>

class LUT {
	public:
		LUT(const char* filename);
		~LUT();

		unsigned int	get_points();
		void		apply(uint16_t r, uint16_t g, uint16_t b,
					uint16_t* out);
		double*		get();

		GLuint		create_texture();

	private:
		double*		lut;
		unsigned int	points;
		unsigned int	size;

		unsigned int	cube_index(unsigned int r, unsigned int g,
					unsigned int b);
};

#endif

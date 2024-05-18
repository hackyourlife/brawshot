#ifndef __BRAWSHOT_H__
#define __BRAWSHOT_H__

#include "egl.h"
#include "lut.h"
#include "shader.h"

class VideoProcessor {
	public:
		VideoProcessor(unsigned int width, unsigned int height,
				float gain, const char* lut_filename);
		~VideoProcessor();

		void info() {
			egl.make_current();
			egl.info();
		}

		void		add(uint16_t* image);
		void		subtract(uint16_t* image);
		void		output(uint8_t* image);

	private:
		EGL		egl;

		unsigned int	width;
		unsigned int	height;

		unsigned int	samples;

		float		gain;

		LUT*		lut;

		Shader*		accumulate_shader;
		Shader*		output_shader;

		GLuint		accumulate_shader_frame;
		GLuint		accumulate_shader_tex;
		GLuint		accumulate_shader_add;

		GLuint		output_shader_frame;
		GLuint		output_shader_lut;
		GLuint		output_shader_samples;
		GLuint		output_shader_gain;
		GLuint		output_shader_use_lut;

		GLuint		input_tex;
		GLuint		accumulation_1_tex;
		GLuint		accumulation_2_tex;
		GLuint		output_tex;

		GLuint		lut_tex;

		GLuint		accumulation_1_fb;
		GLuint		accumulation_2_fb;
		GLuint		output_fb;

		GLuint		quad_vbo;
		GLuint		quad_vao;

		bool		current_accumulator;
};

#endif

#ifndef __BRAWSHOT_H__
#define __BRAWSHOT_H__

#include <cstdint>

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

		void		load_reference(uint16_t* image, bool after_lut);
		void		add(uint16_t* image);
		void		subtract(uint16_t* image);
		void		output(uint8_t* image);
		void		output_raw(uint16_t* image);

	private:
		EGL		egl;

		unsigned int	width;
		unsigned int	height;

		unsigned int	samples;

		float		gain;

		unsigned int	ref_mean[4];
		bool		use_ref;
		bool		ref_after_lut;

		LUT*		lut;

		Shader*		accumulate_shader;
		Shader*		output_shader;
		Shader*		output_raw_shader;

		GLuint		accumulate_shader_frame;
		GLuint		accumulate_shader_tex;
		GLuint		accumulate_shader_add;

		GLuint		output_shader_frame;
		GLuint		output_shader_ref;
		GLuint		output_shader_lut;
		GLuint		output_shader_samples;
		GLuint		output_shader_gain;
		GLuint		output_shader_ref_mean;
		GLuint		output_shader_use_lut;
		GLuint		output_shader_use_ref;
		GLuint		output_shader_ref_after_lut;

		GLuint		output_raw_shader_frame;
		GLuint		output_raw_shader_samples;

		GLuint		input_tex;
		GLuint		input_ref_tex;
		GLuint		accumulation_1_tex;
		GLuint		accumulation_2_tex;
		GLuint		output_tex;
		GLuint		output_raw_tex;

		GLuint		lut_tex;

		GLuint		accumulation_1_fb;
		GLuint		accumulation_2_fb;
		GLuint		output_fb;
		GLuint		output_raw_fb;

		GLuint		quad_vbo;
		GLuint		quad_vao;

		bool		current_accumulator;
};

#endif

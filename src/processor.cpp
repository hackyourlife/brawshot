#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <GL/gl.h>
#include <EGL/egl.h>

#include "brawshot.h"

extern "C" {
	extern const char accumulate_vert[];
	extern const char accumulate_frag[];

	extern const char output_vert[];
	extern const char output_frag[];
}

static const float quad_vertices[] = {
	-1.0f, -1.0f,  0.0f,
	 1.0f, -1.0f,  0.0f,
	 1.0f,  1.0f,  0.0f,

	 1.0f,  1.0f,  0.0f,
	-1.0f,  1.0f,  0.0f,
	-1.0f, -1.0f,  0.0f
};

#define	QUAD_VTX_CNT	(sizeof(quad_vertices) / (sizeof(*quad_vertices) * 3))

#ifdef NDEBUG
#define GL_ERROR()
#else
#define GL_ERROR()	check_error(__FILE__, __LINE__)

static void check_error(const char* filename, unsigned int line)
{
	GLenum error = glGetError();
	switch(error) {
		case GL_NO_ERROR:
			break;
		case GL_INVALID_ENUM:
			printf("%s:%u: Error: GL_INVALID_ENUM\n", filename, line);
			break;
		case GL_INVALID_VALUE:
			printf("%s:%u: Error: GL_INVALID_VALUE\n", filename, line);
			break;
		case GL_INVALID_OPERATION:
			printf("%s:%u: Error: GL_INVALID_OPERATION\n", filename, line);
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			printf("%s:%u: Error: GL_INVALID_FRAMEBUFFER_OPERATION\n", filename, line);
			break;
		case GL_OUT_OF_MEMORY:
			printf("%s:%u: Error: GL_OUT_OF_MEMORY\n", filename, line);
			exit(1);
			break;
		case GL_STACK_UNDERFLOW:
			printf("%s:%u: Error: GL_STACK_UNDERFLOW\n", filename, line);
			break;
		case GL_STACK_OVERFLOW:
			printf("%s:%u: Error: GL_STACK_OVERFLOW\n", filename, line);
			break;
		default:
			printf("%s:%u: Unknown error 0x%X\n", filename, line, error);
	}
}
#endif

VideoProcessor::VideoProcessor(unsigned int width, unsigned int height,
				const char* lut_filename)
	: width(width), height(height), samples(0), lut(nullptr),
			accumulate_shader(nullptr), output_shader(nullptr),
			current_accumulator(false)
{
	egl.make_current();

	// create input (video frame) texture
	glGenTextures(1, &input_tex);
	glBindTexture(GL_TEXTURE_2D, input_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16UI, width, height, 0,
			GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GL_ERROR();

	// create accumulation textures
	glGenTextures(1, &accumulation_1_tex);
	glBindTexture(GL_TEXTURE_2D, accumulation_1_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32UI, width, height, 0,
			GL_RGB_INTEGER, GL_UNSIGNED_INT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GL_ERROR();

	glGenTextures(1, &accumulation_2_tex);
	glBindTexture(GL_TEXTURE_2D, accumulation_2_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32UI, width, height, 0,
			GL_RGB_INTEGER, GL_UNSIGNED_INT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GL_ERROR();

	// create output texture
	glGenTextures(1, &output_tex);
	glBindTexture(GL_TEXTURE_2D, output_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA,
			GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GL_ERROR();

	// load LUT
	if(lut_filename != nullptr) {
		lut = new LUT(lut_filename);
		lut_tex = lut->create_texture();
	}
	GL_ERROR();

	// create accumulation framebuffer 1
	glGenFramebuffers(1, &accumulation_1_fb);
	glBindFramebuffer(GL_FRAMEBUFFER, accumulation_1_fb);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, accumulation_1_tex, 0);
	GL_ERROR();
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Error configuring framebuffer\n");
		exit(1);
	}

	// create accumulation framebuffer 2
	glGenFramebuffers(1, &accumulation_2_fb);
	glBindFramebuffer(GL_FRAMEBUFFER, accumulation_2_fb);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, accumulation_2_tex, 0);
	GL_ERROR();
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Error configuring framebuffer\n");
		exit(1);
	}

	// create output framebuffer
	glGenFramebuffers(1, &output_fb);
	glBindFramebuffer(GL_FRAMEBUFFER, output_fb);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, output_tex, 0);
	GL_ERROR();
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Error configuring framebuffer\n");
		exit(1);
	}

	// create VBO/VAO
	glGenVertexArrays(1, &quad_vao);
	glBindVertexArray(quad_vao);

	GLuint loc = 0;

	glGenBuffers(1, &quad_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices,
			GL_STATIC_DRAW);

	glEnableVertexAttribArray(loc);
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, 0);

	accumulate_shader = new Shader(accumulate_vert, accumulate_frag);
	output_shader = new Shader(output_vert, output_frag);

	accumulate_shader_frame = accumulate_shader->get_uniform("frame");
	accumulate_shader_tex = accumulate_shader->get_uniform("accumulator");
	accumulate_shader_add = accumulate_shader->get_uniform("add");

	output_shader_frame = output_shader->get_uniform("frame");
	output_shader_lut = output_shader->get_uniform("lut");
	output_shader_samples = output_shader->get_uniform("samples");
	output_shader_use_lut = output_shader->get_uniform("use_lut");

	egl.unbind();
}

VideoProcessor::~VideoProcessor()
{
	egl.make_current();

	if(accumulate_shader != nullptr) {
		delete accumulate_shader;
	}

	if(output_shader != nullptr) {
		delete output_shader;
	}

	glDeleteTextures(1, &input_tex);
	glDeleteTextures(1, &accumulation_1_tex);
	glDeleteTextures(1, &accumulation_2_tex);
	glDeleteTextures(1, &output_tex);

	if(lut != nullptr) {
		glDeleteTextures(1, &lut_tex);
		delete lut;
	}

	egl.unbind();
}

void VideoProcessor::add(uint16_t* image)
{
	samples++;

	egl.make_current();
	GL_ERROR();

	glViewport(0, 0, width, height);
	accumulate_shader->use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, input_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16UI, width, height, 0,
			GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, image);

	if(current_accumulator) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, accumulation_1_tex);
		glBindFramebuffer(GL_FRAMEBUFFER, accumulation_2_fb);
	} else {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, accumulation_2_tex);
		glBindFramebuffer(GL_FRAMEBUFFER, accumulation_1_fb);
	}

	current_accumulator = !current_accumulator;

	glUniform1i(accumulate_shader_frame, 0);
	glUniform1i(accumulate_shader_tex, 1);
	glUniform1i(accumulate_shader_add, 1);

	glBindVertexArray(quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, QUAD_VTX_CNT);

	GL_ERROR();

	egl.unbind();
}

void VideoProcessor::subtract(uint16_t* image)
{
	samples--;

	egl.make_current();
	GL_ERROR();

	glViewport(0, 0, width, height);
	accumulate_shader->use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, input_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16UI, width, height, 0,
			GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, image);

	if(current_accumulator) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, accumulation_1_tex);
		glBindFramebuffer(GL_FRAMEBUFFER, accumulation_2_fb);
	} else {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, accumulation_2_tex);
		glBindFramebuffer(GL_FRAMEBUFFER, accumulation_1_fb);
	}

	current_accumulator = !current_accumulator;

	glUniform1i(accumulate_shader_frame, 0);
	glUniform1i(accumulate_shader_tex, 1);
	glUniform1i(accumulate_shader_add, 0);

	glBindVertexArray(quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, QUAD_VTX_CNT);

	GL_ERROR();

	egl.unbind();
}

void VideoProcessor::output(uint8_t* image)
{
	egl.make_current();

	glViewport(0, 0, width, height);
	output_shader->use();

	glActiveTexture(GL_TEXTURE0);
	if(current_accumulator) {
		glBindTexture(GL_TEXTURE_2D, accumulation_1_tex);
	} else {
		glBindTexture(GL_TEXTURE_2D, accumulation_2_tex);
	}

	if(lut != nullptr) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D, lut_tex);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, output_fb);

	glUniform1i(output_shader_frame, 0);
	glUniform1i(output_shader_lut, 1);
	glUniform1ui(output_shader_samples, samples);
	glUniform1i(output_shader_use_lut, lut != nullptr);

	glBindVertexArray(quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, QUAD_VTX_CNT);

	GL_ERROR();
	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, image);
	GL_ERROR();
	egl.unbind();
}

#ifndef __SHADER_H__
#define __SHADER_H__

#include <GL/gl.h>

class Shader {
	public:
		Shader(const char* vs, const char* fs);
		~Shader();

		GLuint	get_uniform(const char* name);
		void	use();

	private:
		GLuint	program;

		GLuint	compile_shader(GLuint type, const char* src);
};

#endif

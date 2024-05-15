#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <GL/gl.h>

#include "shader.h"

Shader::Shader(const char* vs_src, const char* fs_src)
{
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);

	program = glCreateProgram();

	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	GLint linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if(linked == GL_FALSE) {
		GLint len = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

		if(!len) {
			printf("Failed to retrieve shader linking error log\n");
			exit(1);
		}

		char* log = new char[len];
		glGetProgramInfoLog(program, len, &len, log);

		glDeleteProgram(program);
		glDeleteShader(vs);
		glDeleteShader(fs);

		printf("Failed to link shader:\n%s\n", log);

		delete[] log;
		exit(1);
	} else {
		GLint len = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

		if(len) {
			char* log = new char[len];
			glGetProgramInfoLog(program, len, &len, log);

			printf("Shader linking error log:\n%s\n", log);

			delete[] log;
		}
	}

	glDetachShader(program, vs);
	glDetachShader(program, fs);

	glDeleteShader(vs);
	glDeleteShader(fs);
}

Shader::~Shader()
{
	glDeleteProgram(program);
}

GLuint Shader::get_uniform(const char* name)
{
	return glGetUniformLocation(program, name);
}

void Shader::use()
{
	glUseProgram(program);
}

GLuint Shader::compile_shader(GLuint type, const char* src)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, 0);
	glCompileShader(shader);

	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if(compiled == GL_FALSE) {
		GLint len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

		if(!len) {
			printf("Failed to retrieve shader compilation error log\n");
			exit(1);
		}

		char* log = new char[len];
		glGetShaderInfoLog(shader, len, &len, log);
		glDeleteShader(shader);

		printf("Failed to compile shader:\n%s\n", log);
		delete[] log;

		exit(1);
	} else {
		GLint len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

		if(len) {
			char* log = new char[len];
			glGetShaderInfoLog(shader, len, &len, log);
			printf("Shader compilation log:\n%s\n", log);
			delete[] log;
		}
	}

	return shader;
}

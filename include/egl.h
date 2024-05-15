#ifndef __EGL_H__
#define __EGL_H__

#include <GL/gl.h>
#include <EGL/egl.h>

class EGL {
	public:
		EGL();
		~EGL();

		void		info();
		bool		make_current();
		void		unbind();

	private:
		EGLDisplay	display;
		EGLint		major;
		EGLint		minor;
		EGLContext	context;

		bool		init();
		bool		error();
		const char*	error_msg(EGLint err);
};

#endif
